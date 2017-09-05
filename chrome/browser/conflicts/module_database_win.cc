// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_database_win.h"

#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
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
    : task_runner_(task_runner),
      shell_extensions_enumerated_(false),
      ime_enumerated_(false),
      // ModuleDatabase owns |module_inspector_|, so it is safe to use
      // base::Unretained().
      module_inspector_(base::Bind(&ModuleDatabase::OnModuleInspected,
                                   base::Unretained(this))),
      module_list_manager_(std::move(task_runner)),
      third_party_metrics_(this),
      has_started_processing_(false),
      idle_timer_(
          FROM_HERE,
          kIdleTimeout,
          base::Bind(&ModuleDatabase::OnDelayExpired, base::Unretained(this)),
          false),
      weak_ptr_factory_(this) {
  // TODO(pmonette): Wire up the module list manager observer.
}

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
  return has_started_processing_ && RegisteredModulesEnumerated() &&
         !idle_timer_.IsRunning() && module_inspector_.IsIdle();
}

void ModuleDatabase::OnShellExtensionEnumerated(const base::FilePath& path,
                                                uint32_t size_of_image,
                                                uint32_t time_date_stamp) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  idle_timer_.Reset();

  auto* module_info =
      FindOrCreateModuleInfo(path, size_of_image, time_date_stamp);
  module_info->second.module_types |= ModuleInfoData::kTypeShellExtension;
}

void ModuleDatabase::OnShellExtensionEnumerationFinished() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!shell_extensions_enumerated_);

  shell_extensions_enumerated_ = true;

  if (RegisteredModulesEnumerated())
    OnRegisteredModulesEnumerated();
}

void ModuleDatabase::OnImeEnumerated(const base::FilePath& path,
                                     uint32_t size_of_image,
                                     uint32_t time_date_stamp) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  idle_timer_.Reset();

  auto* module_info =
      FindOrCreateModuleInfo(path, size_of_image, time_date_stamp);
  module_info->second.module_types |= ModuleInfoData::kTypeIme;
}

void ModuleDatabase::OnImeEnumerationFinished() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!ime_enumerated_);

  ime_enumerated_ = true;

  if (RegisteredModulesEnumerated())
    OnRegisteredModulesEnumerated();
}

void ModuleDatabase::OnModuleLoad(content::ProcessType process_type,
                                  const base::FilePath& module_path,
                                  uint32_t module_size,
                                  uint32_t module_time_date_stamp,
                                  uintptr_t module_load_address) {
  // Messages can arrive from any thread (UI thread for calls over IPC, and
  // anywhere at all for calls from ModuleWatcher), so bounce if necessary.
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ModuleDatabase::OnModuleLoad,
                   weak_ptr_factory_.GetWeakPtr(), process_type, module_path,
                   module_size, module_time_date_stamp, module_load_address));
    return;
  }

  has_started_processing_ = true;
  idle_timer_.Reset();

  auto* module_info =
      FindOrCreateModuleInfo(module_path, module_size, module_time_date_stamp);

  module_info->second.module_types |= ModuleInfoData::kTypeLoadedModule;

  // Update the list of process types that this module has been seen in.
  module_info->second.process_types |= ProcessTypeToBit(process_type);
}

void ModuleDatabase::AddObserver(ModuleDatabaseObserver* observer) {
  observer_list_.AddObserver(observer);

  // If the registered modules enumeration is not finished yet, the |observer|
  // will be notified later in OnRegisteredModulesEnumerated().
  if (!RegisteredModulesEnumerated())
    return;

  NotifyLoadedModules(observer);

  if (IsIdle())
    observer->OnModuleDatabaseIdle();
}

void ModuleDatabase::RemoveObserver(ModuleDatabaseObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ModuleDatabase::IncreaseInspectionPriority() {
  module_inspector_.IncreaseInspectionPriority();
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

bool ModuleDatabase::RegisteredModulesEnumerated() {
  return shell_extensions_enumerated_ && ime_enumerated_;
}

void ModuleDatabase::OnRegisteredModulesEnumerated() {
  for (auto& observer : observer_list_)
    NotifyLoadedModules(&observer);

  if (IsIdle())
    EnterIdleState();
}

void ModuleDatabase::OnModuleInspected(
    const ModuleInfoKey& module_key,
    std::unique_ptr<ModuleInspectionResult> inspection_result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  auto it = modules_.find(module_key);
  if (it == modules_.end())
    return;

  it->second.inspection_result = std::move(inspection_result);

  if (RegisteredModulesEnumerated())
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

void ModuleDatabase::NotifyLoadedModules(ModuleDatabaseObserver* observer) {
  for (const auto& module : modules_) {
    if (module.second.inspection_result)
      observer->OnNewModuleFound(module.first, module.second);
  }
}
