// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/process_metrics_memory_dump_provider.h"

#include <fcntl.h>
#include <stdint.h>

#include <map>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/process_memory_maps.h"
#include "base/trace_event/process_memory_totals.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/shared_region.h>
#include <sys/param.h>

#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach/mach.h>

#include "base/numerics/safe_math.h"
#include "base/process/process_metrics.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#include <psapi.h>
#include <tchar.h>
#include <windows.h>

#include <base/strings/sys_string_conversions.h>
#include <base/win/win_util.h>
#endif  // defined(OS_WIN)

namespace tracing {

namespace {

base::LazyInstance<
    std::map<base::ProcessId,
             std::unique_ptr<ProcessMetricsMemoryDumpProvider>>>::Leaky
    g_dump_providers_map = LAZY_INSTANCE_INITIALIZER;

#if defined(OS_LINUX) || defined(OS_ANDROID)
const char kClearPeakRssCommand[] = "5";

const uint32_t kMaxLineSize = 4096;

bool ParseSmapsHeader(const char* header_line,
                      base::trace_event::ProcessMemoryMaps::VMRegion* region) {
  // e.g., "00400000-00421000 r-xp 00000000 fc:01 1234  /foo.so\n"
  bool res = true;  // Whether this region should be appended or skipped.
  uint64_t end_addr = 0;
  char protection_flags[5] = {0};
  char mapped_file[kMaxLineSize];

  if (sscanf(header_line, "%" SCNx64 "-%" SCNx64 " %4c %*s %*s %*s%4095[^\n]\n",
             &region->start_address, &end_addr, protection_flags,
             mapped_file) != 4)
    return false;

  if (end_addr > region->start_address) {
    region->size_in_bytes = end_addr - region->start_address;
  } else {
    // This is not just paranoia, it can actually happen (See crbug.com/461237).
    region->size_in_bytes = 0;
    res = false;
  }

  region->protection_flags = 0;
  if (protection_flags[0] == 'r') {
    region->protection_flags |=
      base::trace_event::ProcessMemoryMaps::VMRegion::kProtectionFlagsRead;
  }
  if (protection_flags[1] == 'w') {
    region->protection_flags |=
      base::trace_event::ProcessMemoryMaps::VMRegion::kProtectionFlagsWrite;
  }
  if (protection_flags[2] == 'x') {
    region->protection_flags |=
      base::trace_event::ProcessMemoryMaps::VMRegion::kProtectionFlagsExec;
  }
  if (protection_flags[3] == 's') {
    region->protection_flags |=
      base::trace_event::ProcessMemoryMaps::VMRegion::kProtectionFlagsMayshare;
  }

  region->mapped_file = mapped_file;
  base::TrimWhitespaceASCII(region->mapped_file, base::TRIM_ALL,
                            &region->mapped_file);

  return res;
}

uint64_t ReadCounterBytes(char* counter_line) {
  uint64_t counter_value = 0;
  int res = sscanf(counter_line, "%*s %" SCNu64 " kB", &counter_value);
  return res == 1 ? counter_value * 1024 : 0;
}

uint32_t ParseSmapsCounter(
    char* counter_line,
    base::trace_event::ProcessMemoryMaps::VMRegion* region) {
  // A smaps counter lines looks as follows: "RSS:  0 Kb\n"
  uint32_t res = 1;
  char counter_name[20];
  int did_read = sscanf(counter_line, "%19[^\n ]", counter_name);
  if (did_read != 1)
    return 0;

  if (strcmp(counter_name, "Pss:") == 0) {
    region->byte_stats_proportional_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Private_Dirty:") == 0) {
    region->byte_stats_private_dirty_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Private_Clean:") == 0) {
    region->byte_stats_private_clean_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Shared_Dirty:") == 0) {
    region->byte_stats_shared_dirty_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Shared_Clean:") == 0) {
    region->byte_stats_shared_clean_resident = ReadCounterBytes(counter_line);
  } else if (strcmp(counter_name, "Swap:") == 0) {
    region->byte_stats_swapped = ReadCounterBytes(counter_line);
  } else {
    res = 0;
  }

  return res;
}

uint32_t ReadLinuxProcSmapsFile(FILE* smaps_file,
                                base::trace_event::ProcessMemoryMaps* pmm) {
  if (!smaps_file)
    return 0;

  fseek(smaps_file, 0, SEEK_SET);

  char line[kMaxLineSize];
  const uint32_t kNumExpectedCountersPerRegion = 6;
  uint32_t counters_parsed_for_current_region = 0;
  uint32_t num_valid_regions = 0;
  base::trace_event::ProcessMemoryMaps::VMRegion region;
  bool should_add_current_region = false;
  for (;;) {
    line[0] = '\0';
    if (fgets(line, kMaxLineSize, smaps_file) == nullptr || !strlen(line))
      break;
    if (isxdigit(line[0]) && !isupper(line[0])) {
      region = base::trace_event::ProcessMemoryMaps::VMRegion();
      counters_parsed_for_current_region = 0;
      should_add_current_region = ParseSmapsHeader(line, &region);
    } else {
      counters_parsed_for_current_region += ParseSmapsCounter(line, &region);
      DCHECK_LE(counters_parsed_for_current_region,
                kNumExpectedCountersPerRegion);
      if (counters_parsed_for_current_region == kNumExpectedCountersPerRegion) {
        if (should_add_current_region) {
          pmm->AddVMRegion(region);
          ++num_valid_regions;
          should_add_current_region = false;
        }
      }
    }
  }
  return num_valid_regions;
}

bool GetResidentAndSharedPagesFromStatmFile(int fd,
                                            uint64_t* resident_pages,
                                            uint64_t* shared_pages) {
  lseek(fd, 0, SEEK_SET);
  char line[kMaxLineSize];
  int res = read(fd, line, kMaxLineSize - 1);
  if (res <= 0)
    return false;
  line[res] = '\0';
  int num_scanned =
      sscanf(line, "%*s %" SCNu64 " %" SCNu64, resident_pages, shared_pages);
  return num_scanned == 2;
}

#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

std::unique_ptr<base::ProcessMetrics> CreateProcessMetrics(
    base::ProcessId process) {
  if (process == base::kNullProcessId)
    return base::ProcessMetrics::CreateCurrentProcessMetrics();
#if defined(OS_LINUX) || defined(OS_ANDROID)
  // Just pass ProcessId instead of handle since they are the same in linux and
  // android.
  return base::ProcessMetrics::CreateProcessMetrics(process);
#else
  // Creating process metrics for child processes in mac or windows requires
  // additional information like ProcessHandle or port provider.
  NOTREACHED();
  return std::unique_ptr<base::ProcessMetrics>();
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)
}

}  // namespace

// static
uint64_t ProcessMetricsMemoryDumpProvider::rss_bytes_for_testing = 0;

// static
ProcessMetricsMemoryDumpProvider::FactoryFunction
    ProcessMetricsMemoryDumpProvider::factory_for_testing = nullptr;

#if defined(OS_LINUX) || defined(OS_ANDROID)

// static
FILE* ProcessMetricsMemoryDumpProvider::proc_smaps_for_testing = nullptr;

// static
int ProcessMetricsMemoryDumpProvider::fast_polling_statm_fd_for_testing = -1;

bool ProcessMetricsMemoryDumpProvider::DumpProcessMemoryMaps(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  uint32_t res = 0;
  if (proc_smaps_for_testing) {
    res = ReadLinuxProcSmapsFile(proc_smaps_for_testing, pmd->process_mmaps());
  } else {
    std::string file_name = "/proc/" + (process_ == base::kNullProcessId
                                            ? "self"
                                            : base::IntToString(process_)) +
                            "/smaps";
    base::ScopedFILE smaps_file(fopen(file_name.c_str(), "r"));
    res = ReadLinuxProcSmapsFile(smaps_file.get(), pmd->process_mmaps());
  }

  if (res)
    pmd->set_has_process_mmaps();
  return res;
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_WIN)
bool ProcessMetricsMemoryDumpProvider::DumpProcessMemoryMaps(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  std::vector<HMODULE> modules;
  if (!base::win::GetLoadedModulesSnapshot(::GetCurrentProcess(), &modules))
    return false;

  // Query the base address for each module, and attach it to the dump.
  for (size_t i = 0; i < modules.size(); ++i) {
    wchar_t module_name[MAX_PATH];
    if (!::GetModuleFileName(modules[i], module_name, MAX_PATH))
      continue;

    MODULEINFO module_info;
    if (!::GetModuleInformation(::GetCurrentProcess(), modules[i],
                                &module_info, sizeof(MODULEINFO))) {
      continue;
    }
    base::trace_event::ProcessMemoryMaps::VMRegion region;
    region.size_in_bytes = module_info.SizeOfImage;
    region.mapped_file = base::SysWideToNativeMB(module_name);
    region.start_address = reinterpret_cast<uint64_t>(module_info.lpBaseOfDll);
    pmd->process_mmaps()->AddVMRegion(region);
  }
  if (!pmd->process_mmaps()->vm_regions().empty())
    pmd->set_has_process_mmaps();
  return true;
}
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)

namespace {

using VMRegion = base::trace_event::ProcessMemoryMaps::VMRegion;

bool IsAddressInSharedRegion(uint64_t address) {
  return address >= SHARED_REGION_BASE_X86_64 &&
         address < (SHARED_REGION_BASE_X86_64 + SHARED_REGION_SIZE_X86_64);
}

bool IsRegionContainedInRegion(const VMRegion& containee,
                               const VMRegion& container) {
  uint64_t containee_end_address =
      containee.start_address + containee.size_in_bytes;
  uint64_t container_end_address =
      container.start_address + container.size_in_bytes;
  return containee.start_address >= container.start_address &&
         containee_end_address <= container_end_address;
}

bool DoRegionsIntersect(const VMRegion& a, const VMRegion& b) {
  uint64_t a_end_address = a.start_address + a.size_in_bytes;
  uint64_t b_end_address = b.start_address + b.size_in_bytes;
  return a.start_address < b_end_address && b.start_address < a_end_address;
}

// Creates VMRegions for all dyld images. Returns whether the operation
// succeeded.
bool GetDyldRegions(std::vector<VMRegion>* regions) {
  task_dyld_info_data_t dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
  kern_return_t kr =
      task_info(mach_task_self(), TASK_DYLD_INFO,
                reinterpret_cast<task_info_t>(&dyld_info), &count);
  if (kr != KERN_SUCCESS)
    return false;

  const struct dyld_all_image_infos* all_image_infos =
      reinterpret_cast<const struct dyld_all_image_infos*>(
          dyld_info.all_image_info_addr);

  bool emitted_linkedit_from_dyld_shared_cache = false;
  for (size_t i = 0; i < all_image_infos->infoArrayCount; i++) {
    const char* image_name = all_image_infos->infoArray[i].imageFilePath;

    // The public definition for dyld_all_image_infos/dyld_image_info is wrong
    // for 64-bit platforms. We explicitly cast to struct mach_header_64 even
    // though the public definition claims that this is a struct mach_header.
    const struct mach_header_64* const header =
        reinterpret_cast<const struct mach_header_64* const>(
            all_image_infos->infoArray[i].imageLoadAddress);

    uint64_t next_command = reinterpret_cast<uint64_t>(header + 1);
    uint64_t command_end = next_command + header->sizeofcmds;
    uint64_t slide = 0;
    for (unsigned int j = 0; j < header->ncmds; ++j) {
      // Ensure that next_command doesn't run past header->sizeofcmds.
      if (next_command + sizeof(struct load_command) > command_end)
        return false;
      const struct load_command* load_cmd =
          reinterpret_cast<const struct load_command*>(next_command);
      next_command += load_cmd->cmdsize;

      if (load_cmd->cmd == LC_SEGMENT_64) {
        if (load_cmd->cmdsize < sizeof(segment_command_64))
          return false;
        const segment_command_64* seg =
            reinterpret_cast<const segment_command_64*>(load_cmd);
        if (strcmp(seg->segname, SEG_PAGEZERO) == 0)
          continue;
        if (strcmp(seg->segname, SEG_TEXT) == 0) {
          slide = reinterpret_cast<uint64_t>(header) - seg->vmaddr;
        }

        // Avoid emitting LINKEDIT regions in the dyld shared cache, since they
        // all overlap.
        if (IsAddressInSharedRegion(seg->vmaddr) &&
            strcmp(seg->segname, SEG_LINKEDIT) == 0) {
          if (emitted_linkedit_from_dyld_shared_cache) {
            continue;
          } else {
            emitted_linkedit_from_dyld_shared_cache = true;
            image_name = "dyld shared cache combined __LINKEDIT";
          }
        }

        uint32_t protection_flags = 0;
        if (seg->initprot & VM_PROT_READ)
          protection_flags |= VMRegion::kProtectionFlagsRead;
        if (seg->initprot & VM_PROT_WRITE)
          protection_flags |= VMRegion::kProtectionFlagsWrite;
        if (seg->initprot & VM_PROT_EXECUTE)
          protection_flags |= VMRegion::kProtectionFlagsExec;

        VMRegion region;
        region.size_in_bytes = seg->vmsize;
        region.protection_flags = protection_flags;
        region.mapped_file = image_name;
        region.start_address = slide + seg->vmaddr;

        // We intentionally avoid setting any page information, which is not
        // available from dyld. The fields will be populated later.
        regions->push_back(region);
      }
    }
  }
  return true;
}

void PopulateByteStats(VMRegion* region,
                       const vm_region_top_info_data_t& info) {
  uint64_t dirty_bytes =
      (info.private_pages_resident + info.shared_pages_resident) * PAGE_SIZE;
  switch (info.share_mode) {
    case SM_LARGE_PAGE:
    case SM_PRIVATE:
    case SM_COW:
      region->byte_stats_private_dirty_resident = dirty_bytes;
    case SM_SHARED:
    case SM_PRIVATE_ALIASED:
    case SM_TRUESHARED:
    case SM_SHARED_ALIASED:
      region->byte_stats_shared_dirty_resident = dirty_bytes;
      break;
    case SM_EMPTY:
      break;
    default:
      NOTREACHED();
      break;
  }
}

// Creates VMRegions using mach vm syscalls. Returns whether the operation
// succeeded.
bool GetAllRegions(std::vector<VMRegion>* regions) {
  const int pid = getpid();
  task_t task = mach_task_self();
  mach_vm_size_t size = 0;
  mach_vm_address_t address = MACH_VM_MIN_ADDRESS;
  while (true) {
    base::CheckedNumeric<mach_vm_address_t> next_address(address);
    next_address += size;
    if (!next_address.IsValid())
      return false;
    address = next_address.ValueOrDie();
    mach_vm_address_t address_copy = address;

    vm_region_top_info_data_t info;
    base::MachVMRegionResult result =
        base::GetTopInfo(task, &size, &address, &info);
    if (result == base::MachVMRegionResult::Error)
      return false;
    if (result == base::MachVMRegionResult::Finished)
      break;

    vm_region_basic_info_64 basic_info;
    mach_vm_size_t dummy_size = 0;
    result = base::GetBasicInfo(task, &dummy_size, &address_copy, &basic_info);
    if (result == base::MachVMRegionResult::Error)
      return false;
    if (result == base::MachVMRegionResult::Finished)
      break;

    VMRegion region;
    PopulateByteStats(&region, info);

    if (basic_info.protection & VM_PROT_READ)
      region.protection_flags |= VMRegion::kProtectionFlagsRead;
    if (basic_info.protection & VM_PROT_WRITE)
      region.protection_flags |= VMRegion::kProtectionFlagsWrite;
    if (basic_info.protection & VM_PROT_EXECUTE)
      region.protection_flags |= VMRegion::kProtectionFlagsExec;

    char buffer[MAXPATHLEN];
    int length = proc_regionfilename(pid, address, buffer, MAXPATHLEN);
    if (length != 0)
      region.mapped_file.assign(buffer, length);

    // There's no way to get swapped or clean bytes without doing a
    // very expensive syscalls that crawls every single page in the memory
    // object.
    region.start_address = address;
    region.size_in_bytes = size;
    regions->push_back(region);
  }
  return true;
}

void AddRegionByteStats(VMRegion* dest, const VMRegion& source) {
  dest->byte_stats_private_dirty_resident +=
      source.byte_stats_private_dirty_resident;
  dest->byte_stats_private_clean_resident +=
      source.byte_stats_private_clean_resident;
  dest->byte_stats_shared_dirty_resident +=
      source.byte_stats_shared_dirty_resident;
  dest->byte_stats_shared_clean_resident +=
      source.byte_stats_shared_clean_resident;
  dest->byte_stats_swapped += source.byte_stats_swapped;
  dest->byte_stats_proportional_resident +=
      source.byte_stats_proportional_resident;
}

}  // namespace

bool ProcessMetricsMemoryDumpProvider::DumpProcessMemoryMaps(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using VMRegion = base::trace_event::ProcessMemoryMaps::VMRegion;

  std::vector<VMRegion> dyld_regions;
  if (!GetDyldRegions(&dyld_regions))
    return false;
  std::vector<VMRegion> all_regions;
  if (!GetAllRegions(&all_regions))
    return false;

  // Merge information from dyld regions and all regions.
  for (const VMRegion& region : all_regions) {
    bool skip = false;
    const bool in_shared_region = IsAddressInSharedRegion(region.start_address);
    for (VMRegion& dyld_region : dyld_regions) {
      // If this region is fully contained in a dyld region, then add the bytes
      // stats.
      if (IsRegionContainedInRegion(region, dyld_region)) {
        AddRegionByteStats(&dyld_region, region);
        skip = true;
        break;
      }

      // Check to see if the region is likely used for the dyld shared cache.
      if (in_shared_region) {
        // This region is likely used for the dyld shared cache. Don't record
        // any byte stats since:
        //   1. It's not possible to figure out which dyld regions the byte
        //      stats correspond to.
        //   2. The region is likely shared by non-Chrome processes, so there's
        //      no point in charging the pages towards Chrome.
        if (DoRegionsIntersect(region, dyld_region)) {
          skip = true;
          break;
        }
      }
    }
    if (skip)
      continue;
    pmd->process_mmaps()->AddVMRegion(region);
  }

  for (VMRegion& region : dyld_regions) {
    pmd->process_mmaps()->AddVMRegion(region);
  }

  pmd->set_has_process_mmaps();
  return true;
}
#endif  // defined(OS_MACOSX)

// static
void ProcessMetricsMemoryDumpProvider::RegisterForProcess(
    base::ProcessId process) {
  std::unique_ptr<ProcessMetricsMemoryDumpProvider> owned_provider;
  if (factory_for_testing) {
    owned_provider = factory_for_testing(process);
  } else {
    owned_provider = std::unique_ptr<ProcessMetricsMemoryDumpProvider>(
        new ProcessMetricsMemoryDumpProvider(process));
  }

  ProcessMetricsMemoryDumpProvider* provider = owned_provider.get();
  bool did_insert =
      g_dump_providers_map.Get()
          .insert(std::make_pair(process, std::move(owned_provider)))
          .second;
  if (!did_insert) {
    DLOG(ERROR) << "ProcessMetricsMemoryDumpProvider already registered for "
                << (process == base::kNullProcessId
                        ? "current process"
                        : "process id " + base::IntToString(process));
    return;
  }
  base::trace_event::MemoryDumpProvider::Options options;
  options.target_pid = process;
  options.is_fast_polling_supported = true;
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      provider, "ProcessMemoryMetrics", nullptr, options);
}

// static
void ProcessMetricsMemoryDumpProvider::UnregisterForProcess(
    base::ProcessId process) {
  auto iter = g_dump_providers_map.Get().find(process);
  if (iter == g_dump_providers_map.Get().end())
    return;
  base::trace_event::MemoryDumpManager::GetInstance()
      ->UnregisterAndDeleteDumpProviderSoon(std::move(iter->second));
  g_dump_providers_map.Get().erase(iter);
}

ProcessMetricsMemoryDumpProvider::ProcessMetricsMemoryDumpProvider(
    base::ProcessId process)
    : process_(process),
      process_metrics_(CreateProcessMetrics(process)),
      is_rss_peak_resettable_(true) {}

ProcessMetricsMemoryDumpProvider::~ProcessMetricsMemoryDumpProvider() {}

// Called at trace dump point time. Creates a snapshot of the memory maps for
// the current process.
bool ProcessMetricsMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  bool res = DumpProcessTotals(args, pmd);

  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED)
    res &= DumpProcessMemoryMaps(args, pmd);
  return res;
}

bool ProcessMetricsMemoryDumpProvider::DumpProcessTotals(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
#if defined(OS_MACOSX)
  size_t private_bytes;
  size_t shared_bytes;
  size_t resident_bytes;
  size_t locked_bytes;
  if (!process_metrics_->GetMemoryBytes(&private_bytes, &shared_bytes,
                                        &resident_bytes, &locked_bytes)) {
    return false;
  }
  uint64_t rss_bytes = resident_bytes;
  pmd->process_totals()->SetExtraFieldInBytes("private_bytes", private_bytes);
  pmd->process_totals()->SetExtraFieldInBytes("shared_bytes", shared_bytes);
  pmd->process_totals()->SetExtraFieldInBytes("locked_bytes", locked_bytes);

  base::trace_event::ProcessMemoryTotals::PlatformPrivateFootprint& footprint =
      pmd->process_totals()->GetPlatformPrivateFootprint();
  base::ProcessMetrics::TaskVMInfo info = process_metrics_->GetTaskVMInfo();
  footprint.phys_footprint_bytes = info.phys_footprint;
  footprint.internal_bytes = info.internal;
  footprint.compressed_bytes = info.compressed;
#else
  uint64_t rss_bytes = process_metrics_->GetWorkingSetSize();
#endif  // defined(OS_MACOSX)
  if (rss_bytes_for_testing)
    rss_bytes = rss_bytes_for_testing;

  // rss_bytes will be 0 if the process ended while dumping.
  if (!rss_bytes)
    return false;

  uint64_t peak_rss_bytes = 0;

#if defined(OS_LINUX) || defined(OS_ANDROID)
  auto& footprint = pmd->process_totals()->GetPlatformPrivateFootprint();

  base::ScopedFD autoclose;
  int statm_fd = fast_polling_statm_fd_.get();
  if (statm_fd == -1) {
    autoclose = OpenStatm();
    statm_fd = autoclose.get();
  }
  if (statm_fd == -1)
    return false;
  const static size_t page_size = base::GetPageSize();
  uint64_t resident_pages;
  uint64_t shared_pages;
  bool success = GetResidentAndSharedPagesFromStatmFile(
      statm_fd, &resident_pages, &shared_pages);
  if (!success)
    return false;

  // TODO(hjd): Implement swap in the next CL.
  footprint.rss_anon_bytes = (resident_pages - shared_pages) * page_size;
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if !defined(OS_IOS)
  peak_rss_bytes = process_metrics_->GetPeakWorkingSetSize();
#if defined(OS_LINUX) || defined(OS_ANDROID)
  if (is_rss_peak_resettable_) {
    std::string clear_refs_file =
        "/proc/" +
        (process_ == base::kNullProcessId ? "self"
                                          : base::IntToString(process_)) +
        "/clear_refs";
    int clear_refs_fd = open(clear_refs_file.c_str(), O_WRONLY);
    if (clear_refs_fd > 0 &&
        base::WriteFileDescriptor(clear_refs_fd, kClearPeakRssCommand,
                                  sizeof(kClearPeakRssCommand))) {
      pmd->process_totals()->set_is_peak_rss_resetable(true);
    } else {
      is_rss_peak_resettable_ = false;
    }
    close(clear_refs_fd);
  }
#elif defined(OS_WIN)
  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED) {
    uint64_t pss_bytes = 0;
    bool res = process_metrics_->GetProportionalSetSizeBytes(&pss_bytes);
    if (res) {
      base::trace_event::ProcessMemoryMaps::VMRegion region;
      region.byte_stats_proportional_resident = pss_bytes;
      pmd->process_mmaps()->AddVMRegion(region);
      pmd->set_has_process_mmaps();
    }
  }

#endif
#endif  // !defined(OS_IOS)

  pmd->process_totals()->set_resident_set_bytes(rss_bytes);
  pmd->set_has_process_totals();
  pmd->process_totals()->set_peak_resident_set_bytes(peak_rss_bytes);

  // Returns true even if other metrics failed, since rss is reported.
  return true;
}

#if defined(OS_LINUX) || defined(OS_ANDROID)
base::ScopedFD ProcessMetricsMemoryDumpProvider::OpenStatm() {
  std::string name =
      "/proc/" +
      (process_ == base::kNullProcessId ? "self"
                                        : base::IntToString(process_)) +
      "/statm";
  base::ScopedFD fd = base::ScopedFD(open(name.c_str(), O_RDONLY));
  DCHECK(fd.is_valid());
  return fd;
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

void ProcessMetricsMemoryDumpProvider::PollFastMemoryTotal(
    uint64_t* memory_total) {
  *memory_total = 0;
#if defined(OS_LINUX) || defined(OS_ANDROID)

  int statm_fd = fast_polling_statm_fd_for_testing;
  if (statm_fd == -1) {
    if (!fast_polling_statm_fd_.is_valid())
      fast_polling_statm_fd_ = OpenStatm();
    statm_fd = fast_polling_statm_fd_.get();
    if (statm_fd == -1)
      return;
  }

  uint64_t resident_pages = 0;
  uint64_t ignored_shared_pages = 0;
  if (!GetResidentAndSharedPagesFromStatmFile(statm_fd, &resident_pages,
                                              &ignored_shared_pages))
    return;

  static size_t page_size = base::GetPageSize();
  *memory_total = resident_pages * page_size;
#else
  *memory_total = process_metrics_->GetWorkingSetSize();
#endif
}

void ProcessMetricsMemoryDumpProvider::SuspendFastMemoryPolling() {
#if defined(OS_LINUX) || defined(OS_ANDROID)
  fast_polling_statm_fd_.reset();
#endif
}

}  // namespace tracing
