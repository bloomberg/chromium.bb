// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_share_path.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/concierge/service.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/seneschal_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

void OnSeneschalSharePathResponse(
    base::FilePath path,
    base::OnceCallback<void(bool, std::string)> callback,
    base::Optional<vm_tools::seneschal::SharePathResponse> response) {
  if (!response) {
    std::move(callback).Run(false, "System error");
    return;
  }
  std::move(callback).Run(response.value().success(),
                          response.value().failure_reason());
}

void CallSeneschalSharePath(
    Profile* profile,
    std::string vm_name,
    const base::FilePath path,
    base::OnceCallback<void(bool, std::string)> callback) {
  // Verify path is in one of the allowable mount points.
  // This logic is similar to DownloadPrefs::SanitizeDownloadTargetPath().
  // TODO(joelhockey):  Seneschal currently only support sharing directories
  // under Downloads.
  if (!path.IsAbsolute()) {
    std::move(callback).Run(false, "Path must be absolute");
    return;
  }
  base::FilePath downloads =
      file_manager::util::GetDownloadsFolderForProfile(profile);
  base::FilePath relative_path;
  if (!downloads.AppendRelativePath(path, &relative_path)) {
    std::move(callback).Run(false, "Path must be under Downloads");
    return;
  }

  // VM must be running.
  base::Optional<vm_tools::concierge::VmInfo> vm_info =
      crostini::CrostiniManager::GetForProfile(profile)->GetVmInfo(
          std::move(vm_name));
  if (!vm_info) {
    std::move(callback).Run(false, "VM not running");
    return;
  }

  // Path must be a valid directory.
  if (!base::DirectoryExists(path)) {
    std::move(callback).Run(false, "Path is not a valid directory");
    return;
  }

  vm_tools::seneschal::SharePathRequest request;
  request.set_handle(vm_info->seneschal_server_handle());
  request.mutable_shared_path()->set_path(relative_path.value());
  request.mutable_shared_path()->set_writable(true);
  request.set_storage_location(
      vm_tools::seneschal::SharePathRequest::DOWNLOADS);
  request.set_owner_id(CryptohomeIdForProfile(profile));
  chromeos::DBusThreadManager::Get()->GetSeneschalClient()->SharePath(
      request, base::BindOnce(&OnSeneschalSharePathResponse, std::move(path),
                              std::move(callback)));
}

void SharePathLogErrorCallback(std::string path,
                               base::RepeatingClosure barrier,
                               bool success,
                               std::string failure_reason) {
  barrier.Run();
  if (!success) {
    LOG(ERROR) << "Error SharePath=" << path
               << ", FailureReason=" << failure_reason;
  }
}

}  // namespace

namespace crostini {

void SharePath(Profile* profile,
               std::string vm_name,
               const base::FilePath& path,
               base::OnceCallback<void(bool, std::string)> callback) {
  DCHECK(profile);
  DCHECK(callback);
  // TODO(joelhockey): Save new path into prefs once management UI is ready.
  CallSeneschalSharePath(profile, vm_name, path, std::move(callback));
}

std::vector<std::string> GetSharedPaths(Profile* profile) {
  DCHECK(profile);
  std::vector<std::string> result;
  const base::ListValue* shared_paths =
      profile->GetPrefs()->GetList(prefs::kCrostiniSharedPaths);
  for (const auto& path : *shared_paths) {
    result.emplace_back(path.GetString());
  }
  return result;
}

void ShareAllPaths(Profile* profile, base::OnceCallback<void()> callback) {
  DCHECK(profile);
  std::vector<std::string> paths = GetSharedPaths(profile);
  base::RepeatingClosure barrier =
      base::BarrierClosure(paths.size(), std::move(callback));
  for (const auto& path : paths) {
    CallSeneschalSharePath(
        profile, kCrostiniDefaultVmName, base::FilePath(path),
        base::BindOnce(&SharePathLogErrorCallback, std::move(path), barrier));
  }
}

}  // namespace crostini
