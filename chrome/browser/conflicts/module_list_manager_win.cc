// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_list_manager_win.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"

const wchar_t ModuleListManager::kModuleListRegistryKeyPath[] =
    L"ThirdPartyModuleList"
#ifdef _WIN64
    "64";
#else
    "32";
#endif

const wchar_t ModuleListManager::kModuleListPathKeyName[] = L"Path";

const wchar_t ModuleListManager::kModuleListVersionKeyName[] = L"Version";

ModuleListManager::Observer::Observer() = default;

ModuleListManager::Observer::~Observer() = default;

ModuleListManager::ModuleListManager(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : registry_key_root_(GetRegistryPath()),
      task_runner_(task_runner),
      observer_(nullptr) {
  // Read the cached path and version.
  std::wstring path;
  std::wstring version;
  base::win::RegKey reg_key(GetRegistryHive(), registry_key_root_.c_str(),
                            KEY_READ);
  if (reg_key.Valid() &&
      reg_key.ReadValue(kModuleListPathKeyName, &path) == ERROR_SUCCESS &&
      reg_key.ReadValue(kModuleListVersionKeyName, &version)) {
    base::Version parsed_version(base::WideToUTF8(version));
    if (parsed_version.IsValid()) {
      module_list_path_ = base::FilePath(path);
      module_list_version_ = parsed_version;
    }
  }
}

ModuleListManager::~ModuleListManager() = default;

base::FilePath ModuleListManager::module_list_path() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return module_list_path_;
}

base::Version ModuleListManager::module_list_version() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return module_list_version_;
}

void ModuleListManager::SetObserver(Observer* observer) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  observer_ = observer;
}

// static
HKEY ModuleListManager::GetRegistryHive() {
  return HKEY_CURRENT_USER;
}

// static
std::wstring ModuleListManager::GetRegistryPath() {
  std::wstring path = install_static::GetRegistryPath();
  path.append(1, '\\');
  path.append(kModuleListRegistryKeyPath);
  return path;
}

void ModuleListManager::LoadModuleList(const base::Version& version,
                                       const base::FilePath& path) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(version.IsValid());
  DCHECK(!path.empty());

  if (path == module_list_path_) {
    // If the path hasn't changed the version should not have either.
    DCHECK(version == module_list_version_);
    return;
  }

  // If a new list is being provided and it's not more recent, then bail.
  if (!module_list_path_.empty()) {
    DCHECK(module_list_version_.IsValid());
    if (module_list_version_ >= version)
      return;
  }

  // Update the path and version.
  module_list_path_ = path;
  module_list_version_ = version;

  // Cache the new path and version.
  base::win::RegKey reg_key(GetRegistryHive(), registry_key_root_.c_str(),
                            KEY_WRITE);
  if (reg_key.Valid()) {
    std::wstring version_wstr = base::UTF8ToWide(version.GetString());
    reg_key.WriteValue(kModuleListPathKeyName, path.value().c_str());
    reg_key.WriteValue(kModuleListVersionKeyName, version_wstr.c_str());
  }

  // Notify the observer if it exists.
  if (observer_)
    observer_->OnNewModuleList(version, path);
}
