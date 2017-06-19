// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_database_win.h"

#include <algorithm>
#include <tuple>

#include "base/bind.h"
#include "chrome/browser/conflicts/module_database_observer_win.h"

namespace {

// Document the assumptions made on the ProcessType enum in order to convert
// them to bits.
static_assert(content::PROCESS_TYPE_UNKNOWN == 1,
              "assumes unknown process type has value 1");
static_assert(content::PROCESS_TYPE_BROWSER == 2,
              "assumes browser process type has value 2");
constexpr uint32_t kFirstValidProcessType = content::PROCESS_TYPE_BROWSER;

ModuleDatabase* g_instance = nullptr;

}  // namespace

// static
constexpr base::TimeDelta ModuleDatabase::kIdleTimeout;

ModuleDatabase::ModuleDatabase(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      // ModuleDatabase owns |module_inspector_|, so it is safe to use
      // base::Unretained().
      module_inspector_(base::Bind(&ModuleDatabase::OnModuleInspected,
                                   base::Unretained(this))),
      third_party_metrics_(this),
      has_started_processing_(false),
      idle_timer_(
          FROM_HERE,
          kIdleTimeout,
          base::Bind(&ModuleDatabase::OnDelayExpired, base::Unretained(this)),
          false),
      weak_ptr_factory_(this) {}

ModuleDatabase::~ModuleDatabase() {
  if (this == g_instance)
    g_instance = nullptr;
}

// static
ModuleDatabase* ModuleDatabase::GetInstance() {
  return g_instance;
}

// static
void ModuleDatabase::SetInstance(
    std::unique_ptr<ModuleDatabase> module_database) {
  DCHECK_EQ(nullptr, g_instance);
  // This is deliberately leaked. It can be cleaned up by manually deleting the
  // ModuleDatabase.
  g_instance = module_database.release();
}

bool ModuleDatabase::IsIdle() {
  return has_started_processing_ && !idle_timer_.IsRunning() &&
         module_inspector_.IsIdle();
}

void ModuleDatabase::OnProcessStarted(uint32_t process_id,
                                      uint64_t creation_time,
                                      content::ProcessType process_type) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CreateProcessInfo(process_id, creation_time, process_type);
}

void ModuleDatabase::OnShellExtensionEnumerated(const base::FilePath& path,
                                                uint32_t size_of_image,
                                                uint32_t time_date_stamp) {
  idle_timer_.Reset();

  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto* module_info =
      FindOrCreateModuleInfo(path, size_of_image, time_date_stamp);
  module_info->second.module_types |= ModuleInfoData::kTypeShellExtension;
}

void ModuleDatabase::OnModuleLoad(uint32_t process_id,
                                  uint64_t creation_time,
                                  const base::FilePath& module_path,
                                  uint32_t module_size,
                                  uint32_t module_time_date_stamp,
                                  uintptr_t module_load_address) {
  // Messages can arrive from any thread (UI thread for calls over IPC, and
  // anywhere at all for calls from ModuleWatcher), so bounce if necessary.
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&ModuleDatabase::OnModuleLoad,
                              weak_ptr_factory_.GetWeakPtr(), process_id,
                              creation_time, module_path, module_size,
                              module_time_date_stamp, module_load_address));
    return;
  }

  has_started_processing_ = true;
  idle_timer_.Reset();

  // In theory this should always succeed. However, it is possible for a client
  // to misbehave and send out-of-order messages. It is easy to be tolerant of
  // this by simply not updating the process info in this case. It's not worth
  // crashing if this data is slightly out of sync as this is purely
  // informational.
  auto* process_info = GetProcessInfo(process_id, creation_time);
  if (!process_info)
    return;

  auto* module_info =
      FindOrCreateModuleInfo(module_path, module_size, module_time_date_stamp);

  module_info->second.module_types |= ModuleInfoData::kTypeLoadedModule;

  // Update the list of process types that this module has been seen in.
  module_info->second.process_types |=
      ProcessTypeToBit(process_info->first.process_type);

  // Update the load address maps.
  InsertLoadAddress(module_info->first.module_id, module_load_address,
                    &process_info->second.loaded_modules);
  RemoveLoadAddressById(module_info->first.module_id,
                        &process_info->second.unloaded_modules);
}

void ModuleDatabase::OnModuleUnload(uint32_t process_id,
                                    uint64_t creation_time,
                                    uintptr_t module_load_address) {
  // Messages can arrive from any thread (UI thread for calls over IPC, and
  // anywhere at all for calls from ModuleWatcher), so bounce if necessary.
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&ModuleDatabase::OnModuleUnload,
                              weak_ptr_factory_.GetWeakPtr(), process_id,
                              creation_time, module_load_address));
    return;
  }

  idle_timer_.Reset();

  // See the long-winded comment in OnModuleLoad about reasons why this can
  // fail (but shouldn't normally).
  auto* process_info = GetProcessInfo(process_id, creation_time);
  if (!process_info)
    return;

  // Find the module corresponding to this load address. This is O(1) in the
  // common case of removing a recently removed module, but O(n) worst case.
  // Thankfully, unload events occur far less often and n is quite small.
  size_t i = FindLoadAddressIndexByAddress(module_load_address,
                                           process_info->second.loaded_modules);

  // No such module found. This shouldn't happen either, unless messages are
  // malformed or out of order. Gracefully fail in this case.
  if (i == kInvalidIndex)
    return;

  ModuleId module_id = process_info->second.loaded_modules[i].first;

  // Remove from the loaded module list and insert into the unloaded module
  // list.
  RemoveLoadAddressByIndex(i, &process_info->second.loaded_modules);
  InsertLoadAddress(module_id, module_load_address,
                    &process_info->second.unloaded_modules);
}

void ModuleDatabase::OnProcessEnded(uint32_t process_id,
                                    uint64_t creation_time) {
  // Messages can arrive from any thread (UI thread for calls over IPC, and
  // anywhere at all for calls from ModuleWatcher), so bounce if necessary.
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ModuleDatabase::OnProcessEnded,
                   weak_ptr_factory_.GetWeakPtr(), process_id, creation_time));
    return;
  }

  DeleteProcessInfo(process_id, creation_time);
}

void ModuleDatabase::AddObserver(ModuleDatabaseObserver* observer) {
  observer_list_.AddObserver(observer);
  for (const auto& module : modules_) {
    if (module.second.inspection_result)
      observer->OnNewModuleFound(module.first, module.second);
  }

  if (IsIdle())
    observer->OnModuleDatabaseIdle();
}

void ModuleDatabase::RemoveObserver(ModuleDatabaseObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

// static
uint32_t ModuleDatabase::ProcessTypeToBit(content::ProcessType process_type) {
  uint32_t bit_index =
      static_cast<uint32_t>(process_type) - kFirstValidProcessType;
  DCHECK_GE(31u, bit_index);
  uint32_t bit = (1 << bit_index);
  return bit;
}

// static
content::ProcessType ModuleDatabase::BitIndexToProcessType(uint32_t bit_index) {
  DCHECK_GE(31u, bit_index);
  return static_cast<content::ProcessType>(bit_index + kFirstValidProcessType);
}

// static
size_t ModuleDatabase::FindLoadAddressIndexById(
    ModuleId module_id,
    const ModuleLoadAddresses& load_addresses) {
  // Process elements in reverse order so that RemoveLoadAddressById can handle
  // the more common case of removing the maximum element in O(1).
  for (size_t i = load_addresses.size() - 1; i < load_addresses.size(); --i) {
    if (load_addresses[i].first == module_id)
      return i;
  }
  return kInvalidIndex;
}

// static
size_t ModuleDatabase::FindLoadAddressIndexByAddress(
    uintptr_t load_address,
    const ModuleLoadAddresses& load_addresses) {
  for (size_t i = 0; i < load_addresses.size(); ++i) {
    if (load_addresses[i].second == load_address)
      return i;
  }
  return kInvalidIndex;
}

// static
void ModuleDatabase::InsertLoadAddress(ModuleId module_id,
                                       uintptr_t load_address,
                                       ModuleLoadAddresses* load_addresses) {
  // A very small optimization: the largest module_id is always placed at the
  // end of the array. This is the most common case, and allows O(1)
  // determination that a |module_id| isn't present when it's bigger than the
  // maximum already in the array. This keeps insertions to O(1) in the usual
  // case.
  if (load_addresses->empty() || module_id > load_addresses->back().first) {
    load_addresses->emplace_back(module_id, load_address);
    return;
  }

  // If the module exists in the collection then update the load address and
  // return. This should never really occur, unless the client is deliberately
  // misbehaving or a race causes a reload event (at a different address) to be
  // processed before the corresponding unload. This is very unlikely.
  size_t i = FindLoadAddressIndexById(module_id, *load_addresses);
  if (i != kInvalidIndex) {
    (*load_addresses)[i].second = load_address;
    return;
  }

  // The module does not exist, and by definition is smaller in value than
  // the largest module ID already present. Add it, ensuring that the largest
  // module ID stays at the end.
  load_addresses->emplace(--load_addresses->end(), module_id, load_address);
}

// static
void ModuleDatabase::RemoveLoadAddressById(
    ModuleId module_id,
    ModuleLoadAddresses* load_addresses) {
  if (load_addresses->empty())
    return;

  // This handles the special case of removing the max element in O(1), as
  // FindLoadAddressIndexById processes the elements in reverse order.
  size_t i = FindLoadAddressIndexById(module_id, *load_addresses);
  if (i != kInvalidIndex)
    RemoveLoadAddressByIndex(i, load_addresses);
}

// static
void ModuleDatabase::RemoveLoadAddressByIndex(
    size_t index,
    ModuleLoadAddresses* load_addresses) {
  DCHECK_LT(index, load_addresses->size());

  // Special case: removing the only remaining element.
  if (load_addresses->size() == 1) {
    load_addresses->clear();
    return;
  }

  // Special case: removing the last module (with maximum id). Need to find the
  // new maximum element and ensure it goes to the end.
  if (load_addresses->size() > 2 && index + 1 == load_addresses->size()) {
    // Note that |index| == load_addresses->size() - 1, and is the last
    // indexable element in the vector.

    // Find the index of the new maximum element.
    ModuleId max_id = -1;  // These start at zero.
    size_t max_index = kInvalidIndex;
    for (size_t i = 0; i < load_addresses->size() - 1; ++i) {
      if ((*load_addresses)[i].first > max_id) {
        max_id = (*load_addresses)[i].first;
        max_index = i;
      }
    }

    // Remove the last (max) element.
    load_addresses->resize(index);

    // If the new max element isn't in the last position, then swap it so it is.
    size_t last_index = load_addresses->size() - 1;
    if (max_index != last_index)
      std::swap((*load_addresses)[max_index], (*load_addresses)[last_index]);

    return;
  }

  // If the element to be removed is second last then a single copy is
  // sufficient.
  if (index + 2 == load_addresses->size()) {
    (*load_addresses)[index] = (*load_addresses)[index + 1];
  } else {
    // In the general case two copies are necessary.
    int max_index = load_addresses->size() - 1;
    (*load_addresses)[index] = (*load_addresses)[max_index - 1];
    (*load_addresses)[max_index - 1] = (*load_addresses)[max_index];
  }

  // Remove the last element, which is now duplicated.
  load_addresses->resize(load_addresses->size() - 1);
}

ModuleDatabase::ModuleInfo* ModuleDatabase::FindOrCreateModuleInfo(
    const base::FilePath& module_path,
    uint32_t module_size,
    uint32_t module_time_date_stamp) {
  auto result = modules_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(module_path, module_size, module_time_date_stamp,
                            modules_.size()),
      std::forward_as_tuple());

  // New modules must be inspected.
  if (result.second)
    module_inspector_.AddModule(result.first->first);

  return &(*result.first);
}

ModuleDatabase::ProcessInfo* ModuleDatabase::GetProcessInfo(
    uint32_t process_id,
    uint64_t creation_time) {
  ProcessInfoKey key(process_id, creation_time, content::PROCESS_TYPE_UNKNOWN);
  auto it = processes_.find(key);
  if (it == processes_.end())
    return nullptr;
  return &(*it);
}

void ModuleDatabase::CreateProcessInfo(uint32_t process_id,
                                       uint64_t creation_time,
                                       content::ProcessType process_type) {
  processes_.emplace(std::piecewise_construct,
                     std::forward_as_tuple(ProcessInfoKey(
                         process_id, creation_time, process_type)),
                     std::forward_as_tuple(ProcessInfoData()));
}

void ModuleDatabase::DeleteProcessInfo(uint32_t process_id,
                                       uint64_t creation_time) {
  ProcessInfoKey key(process_id, creation_time, content::PROCESS_TYPE_UNKNOWN);
  processes_.erase(key);
}

void ModuleDatabase::OnModuleInspected(
    const ModuleInfoKey& module_key,
    std::unique_ptr<ModuleInspectionResult> inspection_result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  auto it = modules_.find(module_key);
  if (it == modules_.end())
    return;

  it->second.inspection_result = std::move(inspection_result);

  for (auto& observer : observer_list_)
    observer.OnNewModuleFound(it->first, it->second);

  // Notify the observers if this was the last outstanding module inspection and
  // the delay has already expired.
  if (IsIdle())
    EnterIdleState();
}

void ModuleDatabase::OnDelayExpired() {
  // Notify the observers if there are no outstanding module inspections.
  if (IsIdle())
    EnterIdleState();
}

void ModuleDatabase::EnterIdleState() {
  for (auto& observer : observer_list_)
    observer.OnModuleDatabaseIdle();
}

// ModuleDatabase::ProcessInfoKey ----------------------------------------------

ModuleDatabase::ProcessInfoKey::ProcessInfoKey(
    uint32_t process_id,
    uint64_t creation_time,
    content::ProcessType process_type)
    : process_id(process_id),
      creation_time(creation_time),
      process_type(process_type) {}

ModuleDatabase::ProcessInfoKey::~ProcessInfoKey() = default;

bool ModuleDatabase::ProcessInfoKey::operator<(
    const ProcessInfoKey& pik) const {
  // The key consists of the pair of (process_id, creation_time).
  // Use the std::tuple lexicographic comparison operator.
  return std::make_tuple(process_id, creation_time) <
         std::make_tuple(pik.process_id, pik.creation_time);
}

// ModuleDatabase::ProcessInfoData ---------------------------------------------

ModuleDatabase::ProcessInfoData::ProcessInfoData() = default;

ModuleDatabase::ProcessInfoData::ProcessInfoData(const ProcessInfoData& other) =
    default;

ModuleDatabase::ProcessInfoData::~ProcessInfoData() = default;
