// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager.h"

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_metrics.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/private_working_set_snapshot.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/task_manager/background_information.h"
#include "chrome/browser/task_manager/browser_process_resource_provider.h"
#include "chrome/browser/task_manager/child_process_resource_provider.h"
#include "chrome/browser/task_manager/extension_information.h"
#include "chrome/browser/task_manager/guest_information.h"
#include "chrome/browser/task_manager/panel_information.h"
#include "chrome/browser/task_manager/printing_information.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/tab_contents_information.h"
#include "chrome/browser/task_manager/web_contents_resource_provider.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/worker_service.h"
#include "content/public/common/result_codes.h"
#include "extensions/browser/extension_system.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"

#if defined(OS_MACOSX)
#include "content/public/browser/browser_child_process_host.h"
#endif

using content::BrowserThread;
using content::ResourceRequestInfo;
using content::WebContents;
using task_manager::Resource;
using task_manager::ResourceProvider;
using task_manager::WebContentsInformation;

class Profile;

namespace {

template <class T>
int ValueCompare(T value1, T value2) {
  if (value1 < value2)
    return -1;
  if (value1 == value2)
    return 0;
  return 1;
}

// Used when one or both of the results to compare are unavailable.
int OrderUnavailableValue(bool v1, bool v2) {
  if (!v1 && !v2)
    return 0;
  return v1 ? 1 : -1;
}

// Used by TaskManagerModel::CompareValues(). See it for details of return
// value.
template <class T>
int ValueCompareMember(const TaskManagerModel* model,
                       bool (TaskManagerModel::*f)(int, T*) const,
                       int row1,
                       int row2) {
  T value1;
  T value2;
  bool value1_valid = (model->*f)(row1, &value1);
  bool value2_valid = (model->*f)(row2, &value2);
  return value1_valid && value2_valid ? ValueCompare(value1, value2) :
      OrderUnavailableValue(value1_valid, value2_valid);
}

base::string16 FormatStatsSize(const blink::WebCache::ResourceTypeStat& stat) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_CACHE_SIZE_CELL_TEXT,
      ui::FormatBytesWithUnits(stat.size, ui::DATA_UNITS_KIBIBYTE, false),
      ui::FormatBytesWithUnits(stat.liveSize, ui::DATA_UNITS_KIBIBYTE, false));
}

// Returns true if the specified id should use the first value in the group.
bool IsSharedByGroup(int col_id) {
  switch (col_id) {
    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:
    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:
    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:
    case IDS_TASK_MANAGER_CPU_COLUMN:
    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN:
    case IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN:
    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:
    case IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN:
      return true;
    default:
      return false;
  }
}

#if defined(OS_WIN)
void GetWinGDIHandles(base::ProcessHandle process,
                      size_t* current,
                      size_t* peak) {
  *current = 0;
  *peak = 0;
  // Get a handle to |process| that has PROCESS_QUERY_INFORMATION rights.
  HANDLE current_process = GetCurrentProcess();
  HANDLE process_with_query_rights;
  if (DuplicateHandle(current_process, process, current_process,
                      &process_with_query_rights, PROCESS_QUERY_INFORMATION,
                      false, 0)) {
    *current = GetGuiResources(process_with_query_rights, GR_GDIOBJECTS);
    *peak = GetGuiResources(process_with_query_rights, GR_GDIOBJECTS_PEAK);
    CloseHandle(process_with_query_rights);
  }
}

void GetWinUSERHandles(base::ProcessHandle process,
                       size_t* current,
                       size_t* peak) {
  *current = 0;
  *peak = 0;
  // Get a handle to |process| that has PROCESS_QUERY_INFORMATION rights.
  HANDLE current_process = GetCurrentProcess();
  HANDLE process_with_query_rights;
  if (DuplicateHandle(current_process, process, current_process,
                      &process_with_query_rights, PROCESS_QUERY_INFORMATION,
                      false, 0)) {
    *current = GetGuiResources(process_with_query_rights, GR_USEROBJECTS);
    *peak = GetGuiResources(process_with_query_rights, GR_USEROBJECTS_PEAK);
    CloseHandle(process_with_query_rights);
  }
}
#endif

}  // namespace

class TaskManagerModelGpuDataManagerObserver
    : public content::GpuDataManagerObserver {
 public:
  TaskManagerModelGpuDataManagerObserver() {
    content::GpuDataManager::GetInstance()->AddObserver(this);
  }

  ~TaskManagerModelGpuDataManagerObserver() override {
    content::GpuDataManager::GetInstance()->RemoveObserver(this);
  }

  static void NotifyVideoMemoryUsageStats(
      const content::GPUVideoMemoryUsageStats& video_memory_usage_stats) {
    TaskManager::GetInstance()->model()->NotifyVideoMemoryUsageStats(
        video_memory_usage_stats);
  }

  void OnVideoMemoryUsageStatsUpdate(const content::GPUVideoMemoryUsageStats&
                                         video_memory_usage_stats) override {
    if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      NotifyVideoMemoryUsageStats(video_memory_usage_stats);
    } else {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE, base::Bind(
              &TaskManagerModelGpuDataManagerObserver::
                  NotifyVideoMemoryUsageStats,
              video_memory_usage_stats));
    }
  }
};

TaskManagerModel::PerResourceValues::PerResourceValues()
    : is_title_valid(false),
      is_profile_name_valid(false),
      network_usage(0),
      is_process_id_valid(false),
      process_id(0),
      is_webcore_stats_valid(false),
      is_sqlite_memory_bytes_valid(false),
      sqlite_memory_bytes(0),
      is_v8_memory_valid(false),
      v8_memory_allocated(0),
      v8_memory_used(0) {}

TaskManagerModel::PerResourceValues::~PerResourceValues() {}

TaskManagerModel::PerProcessValues::PerProcessValues()
    : is_cpu_usage_valid(false),
      cpu_usage(0),
      is_idle_wakeups_valid(false),
      idle_wakeups(0),
      is_private_and_shared_valid(false),
      private_bytes(0),
      shared_bytes(0),
      is_physical_memory_valid(false),
      physical_memory(0),
      is_video_memory_valid(false),
      video_memory(0),
      video_memory_has_duplicates(false),
      is_gdi_handles_valid(false),
      gdi_handles(0),
      gdi_handles_peak(0),
      is_user_handles_valid(0),
      user_handles(0),
      user_handles_peak(0),
      is_nacl_debug_stub_port_valid(false),
      nacl_debug_stub_port(0) {}

TaskManagerModel::PerProcessValues::~PerProcessValues() {}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerModel class
////////////////////////////////////////////////////////////////////////////////

TaskManagerModel::TaskManagerModel(TaskManager* task_manager)
    : pending_video_memory_usage_stats_update_(false),
      update_requests_(0),
      listen_requests_(0),
      update_state_(IDLE),
      is_updating_byte_count_(false) {
  AddResourceProvider(
      new task_manager::BrowserProcessResourceProvider(task_manager));
  AddResourceProvider(new task_manager::WebContentsResourceProvider(
      task_manager,
      scoped_ptr<WebContentsInformation>(
          new task_manager::BackgroundInformation())));
  AddResourceProvider(new task_manager::WebContentsResourceProvider(
      task_manager,
      scoped_ptr<WebContentsInformation>(
          new task_manager::TabContentsInformation())));
#if defined(ENABLE_PRINT_PREVIEW)
  AddResourceProvider(new task_manager::WebContentsResourceProvider(
      task_manager,
      scoped_ptr<WebContentsInformation>(
          new task_manager::PrintingInformation())));
#endif  // ENABLE_PRINT_PREVIEW
  AddResourceProvider(new task_manager::WebContentsResourceProvider(
      task_manager,
      scoped_ptr<WebContentsInformation>(
          new task_manager::PanelInformation())));
  AddResourceProvider(
      new task_manager::ChildProcessResourceProvider(task_manager));
  AddResourceProvider(new task_manager::WebContentsResourceProvider(
      task_manager,
      scoped_ptr<WebContentsInformation>(
          new task_manager::ExtensionInformation())));
  AddResourceProvider(new task_manager::WebContentsResourceProvider(
      task_manager,
      scoped_ptr<WebContentsInformation>(
          new task_manager::GuestInformation())));
#if defined(OS_WIN)
  working_set_snapshot_.reset(new PrivateWorkingSetSnapshot);
  working_set_snapshot_->AddToMonitorList("chrome");
  working_set_snapshot_->AddToMonitorList("nacl64");
#endif
}

void TaskManagerModel::AddObserver(TaskManagerModelObserver* observer) {
  observer_list_.AddObserver(observer);
}

void TaskManagerModel::RemoveObserver(TaskManagerModelObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

int TaskManagerModel::ResourceCount() const {
  return resources_.size();
}

int TaskManagerModel::GroupCount() const {
  return group_map_.size();
}

int TaskManagerModel::GetNaClDebugStubPort(int index) const {
  base::ProcessHandle handle = GetResource(index)->GetProcess();
  PerProcessValues& values(per_process_cache_[handle]);
  if (!values.is_nacl_debug_stub_port_valid) {
    return nacl::kGdbDebugStubPortUnknown;
  }
  return values.nacl_debug_stub_port;
}

int64_t TaskManagerModel::GetNetworkUsage(int index) const {
  return GetNetworkUsage(GetResource(index));
}

double TaskManagerModel::GetCPUUsage(int index) const {
  return GetCPUUsage(GetResource(index));
}

int TaskManagerModel::GetIdleWakeupsPerSecond(int index) const {
  return GetIdleWakeupsPerSecond(GetResource(index));
}

base::ProcessId TaskManagerModel::GetProcessId(int index) const {
  PerResourceValues& values(GetPerResourceValues(index));
  if (!values.is_process_id_valid) {
    values.is_process_id_valid = true;
    base::ProcessHandle process(GetResource(index)->GetProcess());
    DCHECK(process);
    values.process_id = base::GetProcId(process);
    DCHECK(values.process_id);
  }
  return values.process_id;
}

base::ProcessHandle TaskManagerModel::GetProcess(int index) const {
  return GetResource(index)->GetProcess();
}

base::string16 TaskManagerModel::GetResourceById(int index, int col_id) const {
  if (IsSharedByGroup(col_id) && !IsResourceFirstInGroup(index))
    return base::string16();

  switch (col_id) {
    case IDS_TASK_MANAGER_TASK_COLUMN:
      return GetResourceTitle(index);

    case IDS_TASK_MANAGER_PROFILE_NAME_COLUMN:
      return GetResourceProfileName(index);

    case IDS_TASK_MANAGER_NET_COLUMN:
      return GetResourceNetworkUsage(index);

    case IDS_TASK_MANAGER_CPU_COLUMN:
      return GetResourceCPUUsage(index);

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:
      return GetResourcePrivateMemory(index);

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:
      return GetResourceSharedMemory(index);

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:
      return GetResourcePhysicalMemory(index);

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      return GetResourceProcessId(index);

    case IDS_TASK_MANAGER_GDI_HANDLES_COLUMN:
      return GetResourceGDIHandles(index);

    case IDS_TASK_MANAGER_USER_HANDLES_COLUMN:
      return GetResourceUSERHandles(index);

    case IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN:
      return GetResourceIdleWakeupsPerSecond(index);

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
      return GetResourceWebCoreImageCacheSize(index);

    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
      return GetResourceWebCoreScriptsCacheSize(index);

    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:
      return GetResourceWebCoreCSSCacheSize(index);

    case IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN:
      return GetResourceVideoMemory(index);

    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
      return GetResourceSqliteMemoryUsed(index);

    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN:
      return GetResourceV8MemoryAllocatedSize(index);

    case IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN:
      return GetResourceNaClDebugStubPort(index);

    default:
      NOTREACHED();
      return base::string16();
  }
}

const base::string16& TaskManagerModel::GetResourceTitle(int index) const {
  PerResourceValues& values = GetPerResourceValues(index);
  if (!values.is_title_valid) {
    values.is_title_valid = true;
    values.title = GetResource(index)->GetTitle();
  }
  return values.title;
}

const base::string16& TaskManagerModel::GetResourceProfileName(
    int index) const {
  PerResourceValues& values(GetPerResourceValues(index));
  if (!values.is_profile_name_valid) {
    values.is_profile_name_valid = true;
    values.profile_name = GetResource(index)->GetProfileName();
  }
  return values.profile_name;
}

base::string16 TaskManagerModel::GetResourceNaClDebugStubPort(int index) const {
  int port = GetNaClDebugStubPort(index);
  if (port == nacl::kGdbDebugStubPortUnknown) {
    return base::ASCIIToUTF16("Unknown");
  } else if (port == nacl::kGdbDebugStubPortUnused) {
    return base::ASCIIToUTF16("N/A");
  } else {
    return base::IntToString16(port);
  }
}

base::string16 TaskManagerModel::GetResourceNetworkUsage(int index) const {
  int64_t net_usage = GetNetworkUsage(index);
  if (net_usage == -1)
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
  if (net_usage == 0)
    return base::ASCIIToUTF16("0");
  base::string16 net_byte = ui::FormatSpeed(net_usage);
  // Force number string to have LTR directionality.
  return base::i18n::GetDisplayStringInLTRDirectionality(net_byte);
}

base::string16 TaskManagerModel::GetResourceCPUUsage(int index) const {
  return base::UTF8ToUTF16(base::StringPrintf(
#if defined(OS_MACOSX)
      // Activity Monitor shows %cpu with one decimal digit -- be
      // consistent with that.
      "%.1f",
#else
      "%.0f",
#endif
      GetCPUUsage(GetResource(index))));
}

base::string16 TaskManagerModel::GetResourcePrivateMemory(int index) const {
  size_t private_mem;
  if (!GetPrivateMemory(index, &private_mem))
    return base::ASCIIToUTF16("N/A");
  return GetMemCellText(private_mem);
}

base::string16 TaskManagerModel::GetResourceSharedMemory(int index) const {
  size_t shared_mem;
  if (!GetSharedMemory(index, &shared_mem))
    return base::ASCIIToUTF16("N/A");
  return GetMemCellText(shared_mem);
}

base::string16 TaskManagerModel::GetResourcePhysicalMemory(int index) const {
  size_t phys_mem;
  GetPhysicalMemory(index, &phys_mem);
  return GetMemCellText(phys_mem);
}

base::string16 TaskManagerModel::GetResourceProcessId(int index) const {
  return base::IntToString16(GetProcessId(index));
}

base::string16 TaskManagerModel::GetResourceGDIHandles(int index) const {
  size_t current, peak;
  GetGDIHandles(index, &current, &peak);
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_HANDLES_CELL_TEXT,
      base::SizeTToString16(current), base::SizeTToString16(peak));
}

base::string16 TaskManagerModel::GetResourceUSERHandles(int index) const {
  size_t current, peak;
  GetUSERHandles(index, &current, &peak);
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_HANDLES_CELL_TEXT,
      base::SizeTToString16(current), base::SizeTToString16(peak));
}

base::string16 TaskManagerModel::GetResourceWebCoreImageCacheSize(
    int index) const {
  if (!CacheWebCoreStats(index))
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
  return FormatStatsSize(GetPerResourceValues(index).webcore_stats.images);
}

base::string16 TaskManagerModel::GetResourceWebCoreScriptsCacheSize(
    int index) const {
  if (!CacheWebCoreStats(index))
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
  return FormatStatsSize(GetPerResourceValues(index).webcore_stats.scripts);
}

base::string16 TaskManagerModel::GetResourceWebCoreCSSCacheSize(
    int index) const {
  if (!CacheWebCoreStats(index))
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
  return FormatStatsSize(
      GetPerResourceValues(index).webcore_stats.cssStyleSheets);
}

base::string16 TaskManagerModel::GetResourceVideoMemory(int index) const {
  size_t video_memory;
  bool has_duplicates;
  if (!GetVideoMemory(index, &video_memory, &has_duplicates) || !video_memory)
    return base::ASCIIToUTF16("N/A");
  if (has_duplicates) {
    return GetMemCellText(video_memory) + base::ASCIIToUTF16("*");
  }
  return GetMemCellText(video_memory);
}

base::string16 TaskManagerModel::GetResourceSqliteMemoryUsed(int index) const {
  size_t bytes = 0;
  if (!GetSqliteMemoryUsedBytes(index, &bytes))
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
  return GetMemCellText(bytes);
}

base::string16 TaskManagerModel::GetResourceIdleWakeupsPerSecond(int index)
    const {
  return base::FormatNumber(GetIdleWakeupsPerSecond(GetResource(index)));
}

base::string16 TaskManagerModel::GetResourceV8MemoryAllocatedSize(
    int index) const {
  size_t memory_allocated = 0, memory_used = 0;
  if (!GetV8MemoryUsed(index, &memory_used) ||
      !GetV8Memory(index, &memory_allocated))
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT);
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_CACHE_SIZE_CELL_TEXT,
      ui::FormatBytesWithUnits(memory_allocated,
                               ui::DATA_UNITS_KIBIBYTE,
                               false),
      ui::FormatBytesWithUnits(memory_used,
                               ui::DATA_UNITS_KIBIBYTE,
                               false));
}

bool TaskManagerModel::GetPrivateMemory(int index, size_t* result) const {
  *result = 0;
  base::ProcessHandle handle = GetResource(index)->GetProcess();
  if (!CachePrivateAndSharedMemory(handle))
    return false;
  *result = per_process_cache_[handle].private_bytes;
  return true;
}

bool TaskManagerModel::GetSharedMemory(int index, size_t* result) const {
  *result = 0;
  base::ProcessHandle handle = GetResource(index)->GetProcess();
  if (!CachePrivateAndSharedMemory(handle))
    return false;
  *result = per_process_cache_[handle].shared_bytes;
  return true;
}

bool TaskManagerModel::GetPhysicalMemory(int index, size_t* result) const {
  *result = 0;

  base::ProcessHandle handle = GetResource(index)->GetProcess();
  PerProcessValues& values(per_process_cache_[handle]);

  if (!values.is_physical_memory_valid) {
    base::WorkingSetKBytes ws_usage;
    MetricsMap::const_iterator iter = metrics_map_.find(handle);
    if (iter == metrics_map_.end() ||
        !iter->second->GetWorkingSetKBytes(&ws_usage))
      return false;

    values.is_physical_memory_valid = true;
#if defined(OS_LINUX)
    // On Linux private memory is also resident. Just use it.
    values.physical_memory = ws_usage.priv * 1024;
#else
    // Memory = working_set.private which is working set minus shareable. This
    // avoids the unpredictable counting that occurs when calculating memory as
    // working set minus shared (renderer code counted when one tab is open and
    // not counted when two or more are open) and it is much more efficient to
    // calculate on Windows.
    values.physical_memory = iter->second->GetWorkingSetSize();
    values.physical_memory -= ws_usage.shareable * 1024;
#endif
  }
  *result = values.physical_memory;
  return true;
}

void TaskManagerModel::GetGDIHandles(int index,
                                     size_t* current,
                                     size_t* peak) const {
  *current = 0;
  *peak = 0;
#if defined(OS_WIN)
  base::ProcessHandle handle = GetResource(index)->GetProcess();
  PerProcessValues& values(per_process_cache_[handle]);

  if (!values.is_gdi_handles_valid) {
    GetWinGDIHandles(GetResource(index)->GetProcess(),
                     &values.gdi_handles,
                     &values.gdi_handles_peak);
    values.is_gdi_handles_valid = true;
  }
  *current = values.gdi_handles;
  *peak = values.gdi_handles_peak;
#endif
}

void TaskManagerModel::GetUSERHandles(int index,
                                      size_t* current,
                                      size_t* peak) const {
  *current = 0;
  *peak = 0;
#if defined(OS_WIN)
  base::ProcessHandle handle = GetResource(index)->GetProcess();
  PerProcessValues& values(per_process_cache_[handle]);

  if (!values.is_user_handles_valid) {
    GetWinUSERHandles(GetResource(index)->GetProcess(),
                      &values.user_handles,
                      &values.user_handles_peak);
    values.is_user_handles_valid = true;
  }
  *current = values.user_handles;
  *peak = values.user_handles_peak;
#endif
}

bool TaskManagerModel::GetWebCoreCacheStats(
    int index,
    blink::WebCache::ResourceTypeStats* result) const {
  if (!CacheWebCoreStats(index))
    return false;
  *result = GetPerResourceValues(index).webcore_stats;
  return true;
}

bool TaskManagerModel::GetVideoMemory(int index,
                                      size_t* video_memory,
                                      bool* has_duplicates) const {
  *video_memory = 0;
  *has_duplicates = false;

  base::ProcessId pid = GetProcessId(index);
  PerProcessValues& values(
      per_process_cache_[GetResource(index)->GetProcess()]);
  if (!values.is_video_memory_valid) {
    content::GPUVideoMemoryUsageStats::ProcessMap::const_iterator i =
        video_memory_usage_stats_.process_map.find(pid);
    if (i == video_memory_usage_stats_.process_map.end())
      return false;
    values.is_video_memory_valid = true;
    // If this checked_cast asserts, then need to change this code to use
    // uint64_t instead of size_t.
    values.video_memory = base::checked_cast<size_t>(i->second.video_memory);
    values.video_memory_has_duplicates = i->second.has_duplicates;
  }
  *video_memory = values.video_memory;
  *has_duplicates = values.video_memory_has_duplicates;
  return true;
}

bool TaskManagerModel::GetSqliteMemoryUsedBytes(
    int index,
    size_t* result) const {
  *result = 0;
  PerResourceValues& values(GetPerResourceValues(index));
  if (!values.is_sqlite_memory_bytes_valid) {
    if (!GetResource(index)->ReportsSqliteMemoryUsed())
      return false;
    values.is_sqlite_memory_bytes_valid = true;
    values.sqlite_memory_bytes = GetResource(index)->SqliteMemoryUsedBytes();
  }
  *result = values.sqlite_memory_bytes;
  return true;
}

bool TaskManagerModel::GetV8Memory(int index, size_t* result) const {
  *result = 0;
  if (!CacheV8Memory(index))
    return false;
  *result = GetPerResourceValues(index).v8_memory_allocated;
  return true;
}

bool TaskManagerModel::GetV8MemoryUsed(int index, size_t* result) const {
  *result = 0;
  if (!CacheV8Memory(index))
    return false;
  *result = GetPerResourceValues(index).v8_memory_used;
  return true;
}

bool TaskManagerModel::CanActivate(int index) const {
  CHECK_LT(index, ResourceCount());
  return GetResourceWebContents(index) != NULL;
}

bool TaskManagerModel::IsResourceFirstInGroup(int index) const {
  Resource* resource = GetResource(index);
  GroupMap::const_iterator iter = group_map_.find(resource->GetProcess());
  DCHECK(iter != group_map_.end());
  const ResourceList& group = iter->second;
  return (group[0] == resource);
}

bool TaskManagerModel::IsResourceLastInGroup(int index) const {
  Resource* resource = GetResource(index);
  GroupMap::const_iterator iter = group_map_.find(resource->GetProcess());
  DCHECK(iter != group_map_.end());
  const ResourceList& group = iter->second;
  return (group.back() == resource);
}

gfx::ImageSkia TaskManagerModel::GetResourceIcon(int index) const {
  gfx::ImageSkia icon = GetResource(index)->GetIcon();
  if (!icon.isNull())
    return icon;

  static const gfx::ImageSkia* default_icon =
      ResourceBundle::GetSharedInstance().
      GetNativeImageNamed(IDR_DEFAULT_FAVICON).ToImageSkia();
  return *default_icon;
}

TaskManagerModel::GroupRange
TaskManagerModel::GetGroupRangeForResource(int index) const {
  Resource* resource = GetResource(index);
  GroupMap::const_iterator group_iter =
      group_map_.find(resource->GetProcess());
  DCHECK(group_iter != group_map_.end());
  const ResourceList& group = group_iter->second;
  if (group.size() == 1) {
    return std::make_pair(index, 1);
  } else {
    for (int i = index; i >= 0; --i) {
      if (GetResource(i) == group[0])
        return std::make_pair(i, group.size());
    }
    NOTREACHED();
    return std::make_pair(-1, -1);
  }
}

int TaskManagerModel::GetGroupIndexForResource(int index) const {
  int group_index = -1;
  for (int i = 0; i <= index; ++i) {
    if (IsResourceFirstInGroup(i))
        group_index++;
  }

  DCHECK_NE(group_index, -1);
  return group_index;
}

int TaskManagerModel::GetResourceIndexForGroup(int group_index,
                                               int index_in_group) const {
  int group_count = -1;
  int count_in_group = -1;
  for (int i = 0; i < ResourceCount(); ++i) {
    if (IsResourceFirstInGroup(i))
      group_count++;

    if (group_count == group_index) {
      count_in_group++;
      if (count_in_group == index_in_group)
        return i;
    } else if (group_count > group_index) {
      break;
    }
  }

  NOTREACHED();
  return -1;
}

int TaskManagerModel::CompareValues(int row1, int row2, int col_id) const {
  CHECK(row1 < ResourceCount() && row2 < ResourceCount());
  switch (col_id) {
    case IDS_TASK_MANAGER_TASK_COLUMN: {
      static icu::Collator* collator = NULL;
      if (!collator) {
        UErrorCode create_status = U_ZERO_ERROR;
        collator = icu::Collator::createInstance(create_status);
        if (!U_SUCCESS(create_status)) {
          collator = NULL;
          NOTREACHED();
        }
      }
      const base::string16& title1 = GetResourceTitle(row1);
      const base::string16& title2 = GetResourceTitle(row2);
      UErrorCode compare_status = U_ZERO_ERROR;
      UCollationResult compare_result = collator->compare(
          static_cast<const UChar*>(title1.c_str()),
          static_cast<int>(title1.length()),
          static_cast<const UChar*>(title2.c_str()),
          static_cast<int>(title2.length()),
          compare_status);
      DCHECK(U_SUCCESS(compare_status));
      return compare_result;
    }

    case IDS_TASK_MANAGER_PROFILE_NAME_COLUMN: {
      const base::string16& profile1 = GetResourceProfileName(row1);
      const base::string16& profile2 = GetResourceProfileName(row2);
      return profile1.compare(0, profile1.length(), profile2, 0,
                              profile2.length());
    }

    case IDS_TASK_MANAGER_NET_COLUMN:
      return ValueCompare(GetNetworkUsage(GetResource(row1)),
                          GetNetworkUsage(GetResource(row2)));

    case IDS_TASK_MANAGER_CPU_COLUMN:
      return ValueCompare(GetCPUUsage(GetResource(row1)),
                          GetCPUUsage(GetResource(row2)));

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:
      return ValueCompareMember(
          this, &TaskManagerModel::GetPrivateMemory, row1, row2);

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:
      return ValueCompareMember(
          this, &TaskManagerModel::GetSharedMemory, row1, row2);

    case IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN:
      return ValueCompareMember(
          this, &TaskManagerModel::GetPhysicalMemory, row1, row2);

    case IDS_TASK_MANAGER_NACL_DEBUG_STUB_PORT_COLUMN:
      return ValueCompare(GetNaClDebugStubPort(row1),
                          GetNaClDebugStubPort(row2));

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      return ValueCompare(GetProcessId(row1), GetProcessId(row2));

    case IDS_TASK_MANAGER_GDI_HANDLES_COLUMN: {
      size_t current1, peak1;
      size_t current2, peak2;
      GetGDIHandles(row1, &current1, &peak1);
      GetGDIHandles(row2, &current2, &peak2);
      return ValueCompare(current1, current2);
    }

    case IDS_TASK_MANAGER_USER_HANDLES_COLUMN: {
      size_t current1, peak1;
      size_t current2, peak2;
      GetUSERHandles(row1, &current1, &peak1);
      GetUSERHandles(row2, &current2, &peak2);
      return ValueCompare(current1, current2);
    }

    case IDS_TASK_MANAGER_IDLE_WAKEUPS_COLUMN:
      return ValueCompare(GetIdleWakeupsPerSecond(row1),
                          GetIdleWakeupsPerSecond(row2));

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN: {
      bool row1_stats_valid = CacheWebCoreStats(row1);
      bool row2_stats_valid = CacheWebCoreStats(row2);
      if (row1_stats_valid && row2_stats_valid) {
        const blink::WebCache::ResourceTypeStats& stats1(
            GetPerResourceValues(row1).webcore_stats);
        const blink::WebCache::ResourceTypeStats& stats2(
            GetPerResourceValues(row2).webcore_stats);
        switch (col_id) {
          case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
            return ValueCompare(stats1.images.size, stats2.images.size);
          case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
            return ValueCompare(stats1.scripts.size, stats2.scripts.size);
          case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:
            return ValueCompare(stats1.cssStyleSheets.size,
                                stats2.cssStyleSheets.size);
          default:
            NOTREACHED();
            return 0;
        }
      }
      return OrderUnavailableValue(row1_stats_valid, row2_stats_valid);
    }

    case IDS_TASK_MANAGER_VIDEO_MEMORY_COLUMN: {
      size_t value1;
      size_t value2;
      bool has_duplicates;
      bool value1_valid = GetVideoMemory(row1, &value1, &has_duplicates);
      bool value2_valid = GetVideoMemory(row2, &value2, &has_duplicates);
      return value1_valid && value2_valid ? ValueCompare(value1, value2) :
          OrderUnavailableValue(value1_valid, value2_valid);
    }

    case IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN:
      return ValueCompareMember(
          this, &TaskManagerModel::GetV8Memory, row1, row2);

    case IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN:
      return ValueCompareMember(
          this, &TaskManagerModel::GetSqliteMemoryUsedBytes, row1, row2);

    default:
      NOTREACHED();
      break;
  }
  return 0;
}

int TaskManagerModel::GetUniqueChildProcessId(int index) const {
  return GetResource(index)->GetUniqueChildProcessId();
}

Resource::Type TaskManagerModel::GetResourceType(int index) const {
  return GetResource(index)->GetType();
}

WebContents* TaskManagerModel::GetResourceWebContents(int index) const {
  return GetResource(index)->GetWebContents();
}

void TaskManagerModel::AddResource(Resource* resource) {
  base::ProcessHandle process = resource->GetProcess();
  DCHECK(process);

  GroupMap::iterator group_iter = group_map_.find(process);
  int new_entry_index = 0;
  if (group_iter == group_map_.end()) {
    group_map_.insert(make_pair(process, ResourceList(1, resource)));

    // Not part of a group, just put at the end of the list.
    resources_.push_back(resource);
    new_entry_index = static_cast<int>(resources_.size() - 1);
  } else {
    ResourceList* group_entries = &(group_iter->second);
    group_entries->push_back(resource);

    // Insert the new entry right after the last entry of its group.
    ResourceList::iterator iter =
        std::find(resources_.begin(),
                  resources_.end(),
                  (*group_entries)[group_entries->size() - 2]);
    DCHECK(iter != resources_.end());
    new_entry_index = static_cast<int>(iter - resources_.begin()) + 1;
    resources_.insert(++iter, resource);
  }

  // Create the ProcessMetrics for this process if needed (not in map).
  if (metrics_map_.find(process) == metrics_map_.end()) {
    base::ProcessMetrics* pm =
#if !defined(OS_MACOSX)
        base::ProcessMetrics::CreateProcessMetrics(process);
#else
        base::ProcessMetrics::CreateProcessMetrics(
            process, content::BrowserChildProcessHost::GetPortProvider());
#endif

    metrics_map_[process] = pm;
  }

  // Notify the table that the contents have changed for it to redraw.
  FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                    OnItemsAdded(new_entry_index, 1));
}

void TaskManagerModel::RemoveResource(Resource* resource) {
  base::ProcessHandle process = resource->GetProcess();

  // Find the associated group.
  GroupMap::iterator group_iter = group_map_.find(process);
  DCHECK(group_iter != group_map_.end());
  if (group_iter == group_map_.end())
    return;
  ResourceList& group_entries = group_iter->second;

  // Remove the entry from the group map.
  ResourceList::iterator iter = std::find(group_entries.begin(),
                                          group_entries.end(),
                                          resource);
  DCHECK(iter != group_entries.end());
  if (iter != group_entries.end())
    group_entries.erase(iter);

  // If there are no more entries for that process, do the clean-up.
  if (group_entries.empty()) {
    group_map_.erase(group_iter);

    // Nobody is using this process, we don't need the process metrics anymore.
    MetricsMap::iterator pm_iter = metrics_map_.find(process);
    DCHECK(pm_iter != metrics_map_.end());
    if (pm_iter != metrics_map_.end()) {
      delete pm_iter->second;
      metrics_map_.erase(process);
    }
  }

  // Remove the entry from the model list.
  iter = std::find(resources_.begin(), resources_.end(), resource);
  DCHECK(iter != resources_.end());
  if (iter != resources_.end()) {
    int index = static_cast<int>(iter - resources_.begin());
    // Notify the observers that the contents will change.
    FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                      OnItemsToBeRemoved(index, 1));
    // Now actually remove the entry from the model list.
    resources_.erase(iter);
    // Notify the table that the contents have changed.
    FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                      OnItemsRemoved(index, 1));
  }

  // Remove the entry from the network maps.
  ResourceValueMap::iterator net_iter =
      current_byte_count_map_.find(resource);
  if (net_iter != current_byte_count_map_.end())
    current_byte_count_map_.erase(net_iter);
}

void TaskManagerModel::StartUpdating() {
  // Multiple StartUpdating requests may come in, and we only need to take
  // action the first time.
  update_requests_++;
  if (update_requests_ > 1)
    return;
  DCHECK_EQ(1, update_requests_);
  DCHECK_NE(TASK_PENDING, update_state_);

  // If update_state_ is STOPPING, it means a task is still pending.  Setting
  // it to TASK_PENDING ensures the tasks keep being posted (by Refresh()).
  if (update_state_ == IDLE) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&TaskManagerModel::RefreshCallback, this));
  }
  update_state_ = TASK_PENDING;

  // Notify resource providers that we are updating.
  StartListening();

  if (!resources_.empty()) {
    FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                      OnReadyPeriodicalUpdate());
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&TaskManagerModel::SetUpdatingByteCount, this, true));
}

void TaskManagerModel::StopUpdating() {
  // Don't actually stop updating until we have heard as many calls as those
  // to StartUpdating.
  update_requests_--;
  if (update_requests_ > 0)
    return;
  // Make sure that update_requests_ cannot go negative.
  CHECK_EQ(0, update_requests_);
  DCHECK_EQ(TASK_PENDING, update_state_);
  update_state_ = STOPPING;

  // Notify resource providers that we are done updating.
  StopListening();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&TaskManagerModel::SetUpdatingByteCount, this, false));
}

void TaskManagerModel::StartListening() {
  // Multiple StartListening requests may come in and we only need to take
  // action the first time.
  listen_requests_++;
  if (listen_requests_ > 1)
    return;
  DCHECK_EQ(1, listen_requests_);

  // Notify resource providers that we should start listening to events.
  for (ResourceProviderList::iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    (*iter)->StartUpdating();
  }
}

void TaskManagerModel::StopListening() {
  // Don't actually stop listening until we have heard as many calls as those
  // to StartListening.
  listen_requests_--;
  if (listen_requests_ > 0)
    return;

  DCHECK_EQ(0, listen_requests_);

  // Notify resource providers that we are done listening.
  for (ResourceProviderList::const_iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    (*iter)->StopUpdating();
  }

  // Must clear the resources before the next attempt to start listening.
  Clear();
}

void TaskManagerModel::Clear() {
  int size = ResourceCount();
  if (size > 0) {
    resources_.clear();

    // Clear the groups.
    group_map_.clear();

    // Clear the process related info.
    STLDeleteValues(&metrics_map_);

    // Clear the network maps.
    current_byte_count_map_.clear();

    per_resource_cache_.clear();
    per_process_cache_.clear();

    FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                      OnItemsRemoved(0, size));
  }
}

void TaskManagerModel::ModelChanged() {
  // Notify the table that the contents have changed for it to redraw.
  FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_, OnModelChanged());
}

void TaskManagerModel::RefreshPhysicalMemoryFromWorkingSetSnapshot() {
#if defined(OS_WIN)
  // Collect working-set data for all monitored processes in one operation, to
  // avoid the inefficiency of retrieving it one at a time.
  working_set_snapshot_->Sample();

  for (size_t i = 0; i < resources_.size(); ++i) {
    size_t private_working_set =
        working_set_snapshot_->GetPrivateWorkingSet(GetProcessId(i));

    // If working-set data is available then use it. If not then
    // GetWorkingSetKBytes will retrieve the data. This is rare except on
    // Windows XP where GetWorkingSetKBytes will always be used.
    if (private_working_set) {
      // Fill in the cache with the retrieved private working set value.
      base::ProcessHandle handle = GetResource(i)->GetProcess();
      PerProcessValues& values(per_process_cache_[handle]);
      values.is_physical_memory_valid = true;
      // Note that the other memory fields are *not* filled in.
      values.physical_memory = private_working_set;
    }
  }
#else
// This is a NOP on other platforms because they can efficiently retrieve
// the private working-set data on a per-process basis.
#endif
}

void TaskManagerModel::Refresh() {
  per_resource_cache_.clear();
  per_process_cache_.clear();
  RefreshPhysicalMemoryFromWorkingSetSnapshot();

#if !defined(DISABLE_NACL)
  nacl::NaClBrowser* nacl_browser = nacl::NaClBrowser::GetInstance();
#endif  // !defined(DISABLE_NACL)

  // Compute the CPU usage values and check if NaCl GDB debug stub port is
  // known.
  // Note that we compute the CPU usage for all resources (instead of doing it
  // lazily) as process_util::GetCPUUsage() returns the CPU usage since the last
  // time it was called, and not calling it everytime would skew the value the
  // next time it is retrieved (as it would be for more than 1 cycle).
  // The same is true for idle wakeups.
  for (ResourceList::iterator iter = resources_.begin();
       iter != resources_.end(); ++iter) {
    base::ProcessHandle process = (*iter)->GetProcess();
    PerProcessValues& values(per_process_cache_[process]);
#if !defined(DISABLE_NACL)
    // Debug stub port doesn't change once known.
    if (!values.is_nacl_debug_stub_port_valid) {
      values.nacl_debug_stub_port = nacl_browser->GetProcessGdbDebugStubPort(
          (*iter)->GetUniqueChildProcessId());
      if (values.nacl_debug_stub_port != nacl::kGdbDebugStubPortUnknown) {
        values.is_nacl_debug_stub_port_valid = true;
      }
    }
#endif  // !defined(DISABLE_NACL)
    if (values.is_cpu_usage_valid && values.is_idle_wakeups_valid)
      continue;
    MetricsMap::iterator metrics_iter = metrics_map_.find(process);
    DCHECK(metrics_iter != metrics_map_.end());
    if (!values.is_cpu_usage_valid) {
      values.is_cpu_usage_valid = true;
      values.cpu_usage = metrics_iter->second->GetCPUUsage();
    }
#if defined(OS_MACOSX) || defined(OS_LINUX)
    // TODO(port): Implement GetIdleWakeupsPerSecond() on other platforms,
    // crbug.com/120488
    if (!values.is_idle_wakeups_valid) {
      values.is_idle_wakeups_valid = true;
      values.idle_wakeups = metrics_iter->second->GetIdleWakeupsPerSecond();
    }
#endif  // defined(OS_MACOSX) || defined(OS_LINUX)
  }

  // Send a request to refresh GPU memory consumption values
  RefreshVideoMemoryUsageStats();

  // Compute the new network usage values.
  base::TimeDelta update_time =
      base::TimeDelta::FromMilliseconds(kUpdateTimeMs);
  for (ResourceValueMap::iterator iter = current_byte_count_map_.begin();
       iter != current_byte_count_map_.end(); ++iter) {
    PerResourceValues* values = &(per_resource_cache_[iter->first]);
    if (update_time > base::TimeDelta::FromSeconds(1))
      values->network_usage = iter->second / update_time.InSeconds();
    else
      values->network_usage = iter->second * (1 / update_time.InSeconds());

    // Then we reset the current byte count.
    iter->second = 0;
  }

  // Let resources update themselves if they need to.
  for (ResourceList::iterator iter = resources_.begin();
       iter != resources_.end(); ++iter) {
     (*iter)->Refresh();
  }

  if (!resources_.empty()) {
    FOR_EACH_OBSERVER(TaskManagerModelObserver, observer_list_,
                      OnItemsChanged(0, ResourceCount()));
  }
}

void TaskManagerModel::NotifyVideoMemoryUsageStats(
    const content::GPUVideoMemoryUsageStats& video_memory_usage_stats) {
  DCHECK(pending_video_memory_usage_stats_update_);
  video_memory_usage_stats_ = video_memory_usage_stats;
  pending_video_memory_usage_stats_update_ = false;
}

void TaskManagerModel::NotifyBytesRead(const net::URLRequest& request,
                                       int64_t byte_count) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!is_updating_byte_count_)
    return;

  // Only net::URLRequestJob instances created by the ResourceDispatcherHost
  // have an associated ResourceRequestInfo and a render frame associated.
  // All other jobs will have -1 returned for the render process child and
  // routing ids - the jobs may still match a resource based on their origin id,
  // otherwise BytesRead() will attribute the activity to the Browser resource.
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(&request);
  int child_id = -1, route_id = -1;
  if (info)
    info->GetAssociatedRenderFrame(&child_id, &route_id);

  // Get the origin PID of the request's originator.  This will only be set for
  // plugins - for renderer or browser initiated requests it will be zero.
  int origin_pid = 0;
  if (info)
    origin_pid = info->GetOriginPID();

  if (bytes_read_buffer_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&TaskManagerModel::NotifyMultipleBytesRead, this),
        base::TimeDelta::FromSeconds(1));
  }

  bytes_read_buffer_.push_back(
      BytesReadParam(origin_pid, child_id, route_id, byte_count));
}

// This is called on the UI thread.
void TaskManagerModel::NotifyDataReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (size_t i = 0; i < on_data_ready_callbacks_.size(); ++i) {
    if (!on_data_ready_callbacks_[i].is_null())
        on_data_ready_callbacks_[i].Run();
  }

  on_data_ready_callbacks_.clear();
}

void TaskManagerModel::RegisterOnDataReadyCallback(
    const base::Closure& callback) {
  on_data_ready_callbacks_.push_back(callback);
}

TaskManagerModel::~TaskManagerModel() {
  on_data_ready_callbacks_.clear();
}

void TaskManagerModel::RefreshCallback() {
  DCHECK_NE(IDLE, update_state_);

  if (update_state_ == STOPPING) {
    // We have been asked to stop.
    update_state_ = IDLE;
    return;
  }

  Refresh();

  // Schedule the next update.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&TaskManagerModel::RefreshCallback, this),
      base::TimeDelta::FromMilliseconds(kUpdateTimeMs));
}

void TaskManagerModel::RefreshVideoMemoryUsageStats() {
  if (pending_video_memory_usage_stats_update_)
    return;

  if (!video_memory_usage_stats_observer_.get()) {
    video_memory_usage_stats_observer_.reset(
        new TaskManagerModelGpuDataManagerObserver());
  }
  pending_video_memory_usage_stats_update_ = true;
  content::GpuDataManager::GetInstance()->RequestVideoMemoryUsageStatsUpdate();
}

int64_t TaskManagerModel::GetNetworkUsageForResource(Resource* resource) const {
  // Returns default of 0 if no network usage.
  return per_resource_cache_[resource].network_usage;
}

void TaskManagerModel::BytesRead(BytesReadParam param) {
  if (update_state_ != TASK_PENDING || listen_requests_ == 0) {
    // A notification sneaked in while we were stopping the updating, just
    // ignore it.
    return;
  }

  // TODO(jcampan): this should be improved once we have a better way of
  // linking a network notification back to the object that initiated it.
  Resource* resource = NULL;
  for (ResourceProviderList::iterator iter = providers_.begin();
       iter != providers_.end(); ++iter) {
    resource = (*iter)->GetResource(param.origin_pid,
                                    param.child_id,
                                    param.route_id);
    if (resource)
      break;
  }

  if (resource == NULL) {
    // We can't match a resource to the notification.  That might mean the
    // tab that started a download was closed, or the request may have had
    // no originating resource associated with it in the first place.
    // We attribute orphaned/unaccounted activity to the Browser process.
    CHECK(param.origin_pid || (param.child_id != -1));
    param.origin_pid = 0;
    param.child_id = param.route_id = -1;
    BytesRead(param);
    return;
  }

  // We do support network usage, mark the resource as such so it can report 0
  // instead of N/A.
  if (!resource->SupportNetworkUsage())
    resource->SetSupportNetworkUsage();

  ResourceValueMap::const_iterator iter_res =
      current_byte_count_map_.find(resource);
  if (iter_res == current_byte_count_map_.end())
    current_byte_count_map_[resource] = param.byte_count;
  else
    current_byte_count_map_[resource] = iter_res->second + param.byte_count;
}

void TaskManagerModel::MultipleBytesRead(
    const std::vector<BytesReadParam>* params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (std::vector<BytesReadParam>::const_iterator it = params->begin();
       it != params->end(); ++it) {
    BytesRead(*it);
  }
}

void TaskManagerModel::NotifyMultipleBytesRead() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!bytes_read_buffer_.empty());

  std::vector<BytesReadParam>* bytes_read_buffer =
      new std::vector<BytesReadParam>;
  bytes_read_buffer_.swap(*bytes_read_buffer);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TaskManagerModel::MultipleBytesRead, this,
                 base::Owned(bytes_read_buffer)));
}

void TaskManagerModel::SetUpdatingByteCount(bool is_updating) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  is_updating_byte_count_ = is_updating;
}

int64_t TaskManagerModel::GetNetworkUsage(Resource* resource) const {
  int64_t net_usage = GetNetworkUsageForResource(resource);
  if (net_usage == 0 && !resource->SupportNetworkUsage())
    return -1;
  return net_usage;
}

double TaskManagerModel::GetCPUUsage(Resource* resource) const {
  const PerProcessValues& values(per_process_cache_[resource->GetProcess()]);
  // Returns 0 if not valid, which is fine.
  return values.cpu_usage;
}

int TaskManagerModel::GetIdleWakeupsPerSecond(Resource* resource) const {
  const PerProcessValues& values(per_process_cache_[resource->GetProcess()]);
  // Returns 0 if not valid, which is fine.
  return values.idle_wakeups;
}

base::string16 TaskManagerModel::GetMemCellText(int64_t number) const {
#if !defined(OS_MACOSX)
  base::string16 str = base::FormatNumber(number / 1024);

  // Adjust number string if necessary.
  base::i18n::AdjustStringForLocaleDirection(&str);
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_MEM_CELL_TEXT, str);
#else
  // System expectation is to show "100 kB", "200 MB", etc.
  // TODO(thakis): Switch to metric units (as opposed to powers of two).
  return ui::FormatBytes(number);
#endif
}

bool TaskManagerModel::CachePrivateAndSharedMemory(
    base::ProcessHandle handle) const {
  PerProcessValues& values(per_process_cache_[handle]);
  if (values.is_private_and_shared_valid)
    return true;

  MetricsMap::const_iterator iter = metrics_map_.find(handle);
  if (iter == metrics_map_.end() ||
      !iter->second->GetMemoryBytes(&values.private_bytes,
                                    &values.shared_bytes)) {
    return false;
  }

  values.is_private_and_shared_valid = true;
  return true;
}

bool TaskManagerModel::CacheWebCoreStats(int index) const {
  PerResourceValues& values(GetPerResourceValues(index));
  if (!values.is_webcore_stats_valid) {
    if (!GetResource(index)->ReportsCacheStats())
      return false;
    values.is_webcore_stats_valid = true;
    values.webcore_stats = GetResource(index)->GetWebCoreCacheStats();
  }
  return true;
}

bool TaskManagerModel::CacheV8Memory(int index) const {
  PerResourceValues& values(GetPerResourceValues(index));
  if (!values.is_v8_memory_valid) {
    if (!GetResource(index)->ReportsV8MemoryStats())
      return false;
    values.is_v8_memory_valid = true;
    values.v8_memory_allocated = GetResource(index)->GetV8MemoryAllocated();
    values.v8_memory_used = GetResource(index)->GetV8MemoryUsed();
  }
  return true;
}

void TaskManagerModel::AddResourceProvider(ResourceProvider* provider) {
  DCHECK(provider);
  providers_.push_back(provider);
}

TaskManagerModel::PerResourceValues& TaskManagerModel::GetPerResourceValues(
    int index) const {
  return per_resource_cache_[GetResource(index)];
}

Resource* TaskManagerModel::GetResource(int index) const {
  CHECK_GE(index, 0);
  CHECK_LT(index, static_cast<int>(resources_.size()));
  return resources_[index];
}

////////////////////////////////////////////////////////////////////////////////
// TaskManager class
////////////////////////////////////////////////////////////////////////////////
// static
void TaskManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kTaskManagerWindowPlacement);
  registry->RegisterDictionaryPref(prefs::kTaskManagerColumnVisibility);
}

bool TaskManager::IsBrowserProcess(int index) const {
  // If some of the selection is out of bounds, ignore. This may happen when
  // killing a process that manages several pages.
  return index < model_->ResourceCount() &&
      model_->GetProcess(index) == base::GetCurrentProcessHandle();
}

void TaskManager::KillProcess(int index) {
  base::ProcessHandle process_handle = model_->GetProcess(index);
  DCHECK(process_handle);
  if (process_handle != base::GetCurrentProcessHandle()) {
    base::Process process =
        base::Process::DeprecatedGetProcessFromHandle(process_handle);
    process.Terminate(content::RESULT_CODE_KILLED, false);
  }
}

void TaskManager::ActivateProcess(int index) {
  // GetResourceWebContents returns a pointer to the relevant web contents for
  // the resource.  If the index doesn't correspond to any web contents
  // (i.e. refers to the Browser process or a plugin), GetWebContents will
  // return NULL.
  WebContents* chosen_web_contents = model_->GetResourceWebContents(index);
  if (chosen_web_contents && chosen_web_contents->GetDelegate())
    chosen_web_contents->GetDelegate()->ActivateContents(chosen_web_contents);
}

void TaskManager::AddResource(Resource* resource) {
  model_->AddResource(resource);
}

void TaskManager::RemoveResource(Resource* resource) {
  model_->RemoveResource(resource);
}

void TaskManager::OnWindowClosed() {
  model_->StopUpdating();
}

void TaskManager::ModelChanged() {
  model_->ModelChanged();
}

// static
TaskManager* TaskManager::GetInstance() {
  return base::Singleton<TaskManager>::get();
}

void TaskManager::OpenAboutMemory(chrome::HostDesktopType desktop_type) {
  Profile* profile = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  if (profile->IsGuestSession() && !g_browser_process->local_state()->
      GetBoolean(prefs::kBrowserGuestModeEnabled)) {
    UserManager::Show(base::FilePath(),
                      profiles::USER_MANAGER_NO_TUTORIAL,
                      profiles::USER_MANAGER_SELECT_PROFILE_CHROME_MEMORY);
    return;
  }

  chrome::NavigateParams params(
      profile, GURL(chrome::kChromeUIMemoryURL), ui::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  params.host_desktop_type = desktop_type;
  chrome::Navigate(&params);
}

TaskManager::TaskManager()
    : model_(new TaskManagerModel(this)) {
}

TaskManager::~TaskManager() {
}
