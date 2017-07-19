// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/discardable_memory/service/discardable_shared_memory_manager.h"

#include <algorithm>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/macros.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/memory_coordinator_client_registry.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_math.h"
#include "base/process/memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/discardable_memory/common/discardable_shared_memory_heap.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/platform_handle.h"

#if defined(OS_LINUX)
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#endif

namespace discardable_memory {
namespace {

const char kSingleProcess[] = "single-process";

const int kInvalidUniqueClientID = -1;

const uint64_t kBrowserTracingProcessId = std::numeric_limits<uint64_t>::max();

uint64_t ClientProcessUniqueIdToTracingProcessId(int client_id) {
  // TODO(penghuang): Move this function to right place.
  // https://crbug.com/661257
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kSingleProcess))
    return kBrowserTracingProcessId;
  // The hash value is incremented so that the tracing id is never equal to
  // MemoryDumpManager::kInvalidTracingProcessId.
  return static_cast<uint64_t>(base::Hash(
             reinterpret_cast<const char*>(&client_id), sizeof(client_id))) +
         1;
}

// mojom::DiscardableSharedMemoryManager implementation. It contains the
// |client_id_| which is not visible to client. We associate allocations with a
// given mojo instance, so if the instance is closed, we can release the
// allocations associated with that instance.
class MojoDiscardableSharedMemoryManagerImpl
    : public mojom::DiscardableSharedMemoryManager {
 public:
  MojoDiscardableSharedMemoryManagerImpl(
      int32_t client_id,
      ::discardable_memory::DiscardableSharedMemoryManager* manager)
      : client_id_(client_id), manager_(manager) {}

  ~MojoDiscardableSharedMemoryManagerImpl() override {
    // Remove this client from the |manager_|, so all allocated discardable
    // memory belong to this client will be released.
    manager_->ClientRemoved(client_id_);
  }

  // mojom::DiscardableSharedMemoryManager overrides:
  void AllocateLockedDiscardableSharedMemory(
      uint32_t size,
      int32_t id,
      AllocateLockedDiscardableSharedMemoryCallback callback) override {
    base::SharedMemoryHandle handle;
    manager_->AllocateLockedDiscardableSharedMemoryForClient(client_id_, size,
                                                             id, &handle);
    mojo::ScopedSharedBufferHandle memory =
        mojo::WrapSharedMemoryHandle(handle, size, false /* read_only */);
    std::move(callback).Run(std::move(memory));
  }

  void DeletedDiscardableSharedMemory(int32_t id) override {
    manager_->ClientDeletedDiscardableSharedMemory(id, client_id_);
  }

 private:
  const int32_t client_id_;
  ::discardable_memory::DiscardableSharedMemoryManager* const manager_;

  DISALLOW_COPY_AND_ASSIGN(MojoDiscardableSharedMemoryManagerImpl);
};

class DiscardableMemoryImpl : public base::DiscardableMemory {
 public:
  DiscardableMemoryImpl(
      std::unique_ptr<base::DiscardableSharedMemory> shared_memory,
      const base::Closure& deleted_callback)
      : shared_memory_(std::move(shared_memory)),
        deleted_callback_(deleted_callback),
        is_locked_(true) {}

  ~DiscardableMemoryImpl() override {
    if (is_locked_)
      shared_memory_->Unlock(0, 0);

    deleted_callback_.Run();
  }

  // Overridden from base::DiscardableMemory:
  bool Lock() override {
    DCHECK(!is_locked_);

    if (shared_memory_->Lock(0, 0) != base::DiscardableSharedMemory::SUCCESS)
      return false;

    is_locked_ = true;
    return true;
  }
  void Unlock() override {
    DCHECK(is_locked_);

    shared_memory_->Unlock(0, 0);
    is_locked_ = false;
  }
  void* data() const override {
    DCHECK(is_locked_);
    return shared_memory_->memory();
  }

  base::trace_event::MemoryAllocatorDump* CreateMemoryAllocatorDump(
      const char* name,
      base::trace_event::ProcessMemoryDump* pmd) const override {
    // The memory could have been purged, but we still create a dump with
    // mapped_size. So, the size can be inaccurate.
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    shared_memory_->mapped_size());
    return dump;
  }

 private:
  std::unique_ptr<base::DiscardableSharedMemory> shared_memory_;
  const base::Closure deleted_callback_;
  bool is_locked_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryImpl);
};

// Returns the default memory limit to use for discardable memory, taking
// the amount physical memory available and other platform specific constraints
// into account.
int64_t GetDefaultMemoryLimit() {
  const int kMegabyte = 1024 * 1024;

#if defined(OS_ANDROID)
  // Limits the number of FDs used to 32, assuming a 4MB allocation size.
  int64_t max_default_memory_limit = 128 * kMegabyte;
#else
  int64_t max_default_memory_limit = 512 * kMegabyte;
#endif

  // Use 1/8th of discardable memory on low-end devices.
  if (base::SysInfo::IsLowEndDevice())
    max_default_memory_limit /= 8;

#if defined(OS_LINUX)
  base::FilePath shmem_dir;
  if (base::GetShmemTempDir(false, &shmem_dir)) {
    int64_t shmem_dir_amount_of_free_space =
        base::SysInfo::AmountOfFreeDiskSpace(shmem_dir);
    DCHECK_GT(shmem_dir_amount_of_free_space, 0);
    int64_t shmem_dir_amount_of_free_space_mb =
        shmem_dir_amount_of_free_space / kMegabyte;

    UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.ShmemDir.AmountOfFreeSpace",
                                shmem_dir_amount_of_free_space_mb, 1,
                                4 * 1024,  // 4 GB
                                50);

    if (shmem_dir_amount_of_free_space_mb < 64) {
      LOG(WARNING) << "Less than 64MB of free space in temporary directory for "
                      "shared memory files: "
                   << shmem_dir_amount_of_free_space_mb;
    }

    // Allow 1/2 of available shmem dir space to be used for discardable memory.
    max_default_memory_limit =
        std::min(max_default_memory_limit, shmem_dir_amount_of_free_space / 2);
  }
#endif

  // Allow 25% of physical memory to be used for discardable memory.
  return std::min(max_default_memory_limit,
                  base::SysInfo::AmountOfPhysicalMemory() / 4);
}

const int kEnforceMemoryPolicyDelayMs = 1000;

// Global atomic to generate unique discardable shared memory IDs.
base::AtomicSequenceNumber g_next_discardable_shared_memory_id;

}  // namespace

DiscardableSharedMemoryManager::MemorySegment::MemorySegment(
    std::unique_ptr<base::DiscardableSharedMemory> memory)
    : memory_(std::move(memory)) {}

DiscardableSharedMemoryManager::MemorySegment::~MemorySegment() {}

DiscardableSharedMemoryManager::DiscardableSharedMemoryManager()
    : next_client_id_(1),
      default_memory_limit_(GetDefaultMemoryLimit()),
      memory_limit_(default_memory_limit_),
      bytes_allocated_(0),
      memory_pressure_listener_(new base::MemoryPressureListener(
          base::Bind(&DiscardableSharedMemoryManager::OnMemoryPressure,
                     base::Unretained(this)))),
      // Current thread might not have a task runner in tests.
      enforce_memory_policy_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      enforce_memory_policy_pending_(false),
      weak_ptr_factory_(this) {
  DCHECK_NE(memory_limit_, 0u);
  enforce_memory_policy_callback_ =
      base::Bind(&DiscardableSharedMemoryManager::EnforceMemoryPolicy,
                 weak_ptr_factory_.GetWeakPtr());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "DiscardableSharedMemoryManager",
      base::ThreadTaskRunnerHandle::Get());
  base::MemoryCoordinatorClientRegistry::GetInstance()->Register(this);
}

DiscardableSharedMemoryManager::~DiscardableSharedMemoryManager() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void DiscardableSharedMemoryManager::Bind(
    mojom::DiscardableSharedMemoryManagerRequest request,
    const service_manager::BindSourceInfo& source_info) {
  mojo::MakeStrongBinding(
      base::MakeUnique<MojoDiscardableSharedMemoryManagerImpl>(
          next_client_id_++, this),
      std::move(request));
}

std::unique_ptr<base::DiscardableMemory>
DiscardableSharedMemoryManager::AllocateLockedDiscardableMemory(size_t size) {
  DCHECK_NE(size, 0u);

  int32_t new_id = g_next_discardable_shared_memory_id.GetNext();

  // Note: Use DiscardableSharedMemoryHeap for in-process allocation
  // of discardable memory if the cost of each allocation is too high.
  base::SharedMemoryHandle handle;
  AllocateLockedDiscardableSharedMemory(kInvalidUniqueClientID, size, new_id,
                                        &handle);
  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory(handle));
  if (!memory->Map(size))
    base::TerminateBecauseOutOfMemory(size);
  // Close file descriptor to avoid running out.
  memory->Close();
  return base::MakeUnique<DiscardableMemoryImpl>(
      std::move(memory),
      base::Bind(
          &DiscardableSharedMemoryManager::DeletedDiscardableSharedMemory,
          base::Unretained(this), new_id, kInvalidUniqueClientID));
}

bool DiscardableSharedMemoryManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND) {
    base::trace_event::MemoryAllocatorDump* total_dump =
        pmd->CreateAllocatorDump("discardable");
    total_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          GetBytesAllocated());
    return true;
  }

  base::AutoLock lock(lock_);
  for (const auto& client_entry : clients_) {
    const int client_id = client_entry.first;
    const MemorySegmentMap& client_segments = client_entry.second;
    for (const auto& segment_entry : client_segments) {
      const int segment_id = segment_entry.first;
      const MemorySegment* segment = segment_entry.second.get();
      if (!segment->memory()->mapped_size())
        continue;

      // The "size" will be inherited form the shared global dump.
      std::string dump_name = base::StringPrintf(
          "discardable/process_%x/segment_%d", client_id, segment_id);
      base::trace_event::MemoryAllocatorDump* dump =
          pmd->CreateAllocatorDump(dump_name);

      dump->AddScalar("virtual_size",
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      segment->memory()->mapped_size());

      // Host can only tell if whole segment is locked or not.
      dump->AddScalar(
          "locked_size", base::trace_event::MemoryAllocatorDump::kUnitsBytes,
          segment->memory()->IsMemoryLocked() ? segment->memory()->mapped_size()
                                              : 0u);

      // Create the cross-process ownership edge. If the client creates a
      // corresponding dump for the same segment, this will avoid to
      // double-count them in tracing. If, instead, no other process will emit a
      // dump with the same guid, the segment will be accounted to the browser.
      const uint64_t client_tracing_id =
          ClientProcessUniqueIdToTracingProcessId(client_id);
      base::trace_event::MemoryAllocatorDumpGuid shared_segment_guid =
          DiscardableSharedMemoryHeap::GetSegmentGUIDForTracing(
              client_tracing_id, segment_id);
      pmd->CreateSharedGlobalAllocatorDump(shared_segment_guid);
      pmd->AddOwnershipEdge(dump->guid(), shared_segment_guid);

#if defined(COUNT_RESIDENT_BYTES_SUPPORTED)
      if (args.level_of_detail ==
          base::trace_event::MemoryDumpLevelOfDetail::DETAILED) {
        size_t resident_size =
            base::trace_event::ProcessMemoryDump::CountResidentBytes(
                segment->memory()->memory(), segment->memory()->mapped_size());

        // This is added to the global dump since it has to be attributed to
        // both the allocator dumps involved.
        pmd->GetSharedGlobalAllocatorDump(shared_segment_guid)
            ->AddScalar("resident_size",
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        static_cast<uint64_t>(resident_size));
      }
#endif  // defined(COUNT_RESIDENT_BYTES_SUPPORTED)
    }
  }
  return true;
}

void DiscardableSharedMemoryManager::
    AllocateLockedDiscardableSharedMemoryForClient(
        int client_id,
        size_t size,
        int32_t id,
        base::SharedMemoryHandle* shared_memory_handle) {
  AllocateLockedDiscardableSharedMemory(client_id, size, id,
                                        shared_memory_handle);
}

void DiscardableSharedMemoryManager::ClientDeletedDiscardableSharedMemory(
    int32_t id,
    int client_id) {
  DeletedDiscardableSharedMemory(id, client_id);
}

void DiscardableSharedMemoryManager::ClientRemoved(int client_id) {
  base::AutoLock lock(lock_);

  auto it = clients_.find(client_id);
  if (it == clients_.end())
    return;

  size_t bytes_allocated_before_releasing_memory = bytes_allocated_;

  for (auto& segment_it : it->second)
    ReleaseMemory(segment_it.second->memory());

  clients_.erase(it);

  if (bytes_allocated_ != bytes_allocated_before_releasing_memory)
    BytesAllocatedChanged(bytes_allocated_);
}

void DiscardableSharedMemoryManager::SetMemoryLimit(size_t limit) {
  base::AutoLock lock(lock_);

  memory_limit_ = limit;
  ReduceMemoryUsageUntilWithinMemoryLimit();
}

void DiscardableSharedMemoryManager::EnforceMemoryPolicy() {
  base::AutoLock lock(lock_);

  enforce_memory_policy_pending_ = false;
  ReduceMemoryUsageUntilWithinMemoryLimit();
}

size_t DiscardableSharedMemoryManager::GetBytesAllocated() {
  base::AutoLock lock(lock_);

  return bytes_allocated_;
}

void DiscardableSharedMemoryManager::OnMemoryStateChange(
    base::MemoryState state) {
  // Don't use SetMemoryLimit() as it frees up existing allocations.
  // OnPurgeMemory() is called to actually free up memory.
  base::AutoLock lock(lock_);
  switch (state) {
    case base::MemoryState::NORMAL:
      memory_limit_ = default_memory_limit_;
      break;
    case base::MemoryState::THROTTLED:
      memory_limit_ = 0;
      break;
    case base::MemoryState::SUSPENDED:
    // Note that SUSPENDED never occurs in the main browser process so far.
    // Fall through.
    case base::MemoryState::UNKNOWN:
      NOTREACHED();
      break;
  }
}

void DiscardableSharedMemoryManager::OnPurgeMemory() {
  base::AutoLock lock(lock_);
  ReduceMemoryUsageUntilWithinLimit(0);
}

void DiscardableSharedMemoryManager::AllocateLockedDiscardableSharedMemory(
    int client_id,
    size_t size,
    int32_t id,
    base::SharedMemoryHandle* shared_memory_handle) {
  base::AutoLock lock(lock_);

  // Make sure |id| is not already in use.
  MemorySegmentMap& client_segments = clients_[client_id];
  if (client_segments.find(id) != client_segments.end()) {
    LOG(ERROR) << "Invalid discardable shared memory ID";
    *shared_memory_handle = base::SharedMemoryHandle();
    return;
  }

  // Memory usage must be reduced to prevent the addition of |size| from
  // taking usage above the limit. Usage should be reduced to 0 in cases
  // where |size| is greater than the limit.
  size_t limit = 0;
  // Note: the actual mapped size can be larger than requested and cause
  // |bytes_allocated_| to temporarily be larger than |memory_limit_|. The
  // error is minimized by incrementing |bytes_allocated_| with the actual
  // mapped size rather than |size| below.
  if (size < memory_limit_)
    limit = memory_limit_ - size;

  if (bytes_allocated_ > limit)
    ReduceMemoryUsageUntilWithinLimit(limit);

  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory);
  if (!memory->CreateAndMap(size)) {
    *shared_memory_handle = base::SharedMemoryHandle();
    return;
  }

  base::CheckedNumeric<size_t> checked_bytes_allocated = bytes_allocated_;
  checked_bytes_allocated += memory->mapped_size();
  if (!checked_bytes_allocated.IsValid()) {
    *shared_memory_handle = base::SharedMemoryHandle();
    return;
  }

  bytes_allocated_ = checked_bytes_allocated.ValueOrDie();
  BytesAllocatedChanged(bytes_allocated_);

  *shared_memory_handle = base::SharedMemory::DuplicateHandle(memory->handle());
  // Close file descriptor to avoid running out.
  memory->Close();

  scoped_refptr<MemorySegment> segment(new MemorySegment(std::move(memory)));
  client_segments[id] = segment.get();
  segments_.push_back(segment.get());
  std::push_heap(segments_.begin(), segments_.end(), CompareMemoryUsageTime);

  if (bytes_allocated_ > memory_limit_)
    ScheduleEnforceMemoryPolicy();
}

void DiscardableSharedMemoryManager::DeletedDiscardableSharedMemory(
    int32_t id,
    int client_id) {
  base::AutoLock lock(lock_);

  MemorySegmentMap& client_segments = clients_[client_id];

  MemorySegmentMap::iterator segment_it = client_segments.find(id);
  if (segment_it == client_segments.end()) {
    LOG(ERROR) << "Invalid discardable shared memory ID";
    return;
  }

  size_t bytes_allocated_before_releasing_memory = bytes_allocated_;

  ReleaseMemory(segment_it->second->memory());

  client_segments.erase(segment_it);

  if (bytes_allocated_ != bytes_allocated_before_releasing_memory)
    BytesAllocatedChanged(bytes_allocated_);
}

void DiscardableSharedMemoryManager::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  base::AutoLock lock(lock_);

  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      // Purge memory until usage is within half of |memory_limit_|.
      ReduceMemoryUsageUntilWithinLimit(memory_limit_ / 2);
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // Purge everything possible when pressure is critical.
      ReduceMemoryUsageUntilWithinLimit(0);
      break;
  }
}

void DiscardableSharedMemoryManager::ReduceMemoryUsageUntilWithinMemoryLimit() {
  lock_.AssertAcquired();

  if (bytes_allocated_ <= memory_limit_)
    return;

  ReduceMemoryUsageUntilWithinLimit(memory_limit_);
  if (bytes_allocated_ > memory_limit_)
    ScheduleEnforceMemoryPolicy();
}

void DiscardableSharedMemoryManager::ReduceMemoryUsageUntilWithinLimit(
    size_t limit) {
  TRACE_EVENT1("renderer_host",
               "DiscardableSharedMemoryManager::"
               "ReduceMemoryUsageUntilWithinLimit",
               "bytes_allocated", bytes_allocated_);

  // Usage time of currently locked segments are updated to this time and
  // we stop eviction attempts as soon as we come across a segment that we've
  // previously tried to evict but was locked.
  base::Time current_time = Now();

  lock_.AssertAcquired();
  size_t bytes_allocated_before_purging = bytes_allocated_;
  while (!segments_.empty()) {
    if (bytes_allocated_ <= limit)
      break;

    // Stop eviction attempts when the LRU segment is currently in use.
    if (segments_.front()->memory()->last_known_usage() >= current_time)
      break;

    std::pop_heap(segments_.begin(), segments_.end(), CompareMemoryUsageTime);
    scoped_refptr<MemorySegment> segment = segments_.back();
    segments_.pop_back();

    // Simply drop the reference and continue if memory has already been
    // unmapped. This happens when a memory segment has been deleted by
    // the client.
    if (!segment->memory()->mapped_size())
      continue;

    // Attempt to purge LRU segment. When successful, released the memory.
    if (segment->memory()->Purge(current_time)) {
      ReleaseMemory(segment->memory());
      continue;
    }

    // Add memory segment (with updated usage timestamp) back on heap after
    // failed attempt to purge it.
    segments_.push_back(segment.get());
    std::push_heap(segments_.begin(), segments_.end(), CompareMemoryUsageTime);
  }

  if (bytes_allocated_ != bytes_allocated_before_purging)
    BytesAllocatedChanged(bytes_allocated_);
}

void DiscardableSharedMemoryManager::ReleaseMemory(
    base::DiscardableSharedMemory* memory) {
  lock_.AssertAcquired();

  size_t size = memory->mapped_size();
  DCHECK_GE(bytes_allocated_, size);
  bytes_allocated_ -= size;

  // This will unmap the memory segment and drop our reference. The result
  // is that the memory will be released to the OS if the client is no longer
  // referencing it.
  // Note: We intentionally leave the segment in the |segments| vector to
  // avoid reconstructing the heap. The element will be removed from the heap
  // when its last usage time is older than all other segments.
  memory->Unmap();
  memory->Close();
}

void DiscardableSharedMemoryManager::BytesAllocatedChanged(
    size_t new_bytes_allocated) const {
  static const char kTotalDiscardableMemoryAllocatedKey[] =
      "total-discardable-memory-allocated";
  base::debug::SetCrashKeyValue(kTotalDiscardableMemoryAllocatedKey,
                                base::Uint64ToString(new_bytes_allocated));
}

base::Time DiscardableSharedMemoryManager::Now() const {
  return base::Time::Now();
}

void DiscardableSharedMemoryManager::ScheduleEnforceMemoryPolicy() {
  lock_.AssertAcquired();

  if (enforce_memory_policy_pending_)
    return;

  enforce_memory_policy_pending_ = true;
  DCHECK(enforce_memory_policy_task_runner_);
  enforce_memory_policy_task_runner_->PostDelayedTask(
      FROM_HERE, enforce_memory_policy_callback_,
      base::TimeDelta::FromMilliseconds(kEnforceMemoryPolicyDelayMs));
}

}  // namespace discardable_memory
