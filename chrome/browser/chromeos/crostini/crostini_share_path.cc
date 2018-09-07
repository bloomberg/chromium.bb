// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_share_path.h"

#include "base/bind.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/concierge/service.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/seneschal_client.h"

namespace crostini {

// static
CrostiniSharePath* CrostiniSharePath::GetInstance() {
  return base::Singleton<CrostiniSharePath>::get();
}

CrostiniSharePath::CrostiniSharePath() : weak_ptr_factory_(this) {}
CrostiniSharePath::~CrostiniSharePath() {}

void CrostiniSharePath::SharePath(
    Profile* profile,
    std::string vm_name,
    std::string path,
    base::OnceCallback<void(bool, std::string)> callback) {
  base::Optional<vm_tools::concierge::VmInfo> vm_info =
      crostini::CrostiniManager::GetForProfile(profile)->GetVmInfo(
          std::move(vm_name));
  if (!vm_info) {
    std::move(callback).Run(false, "Cannot share, VM not running");
    return;
  }

  vm_tools::seneschal::SharePathRequest request;
  request.set_handle(vm_info->seneschal_server_handle());
  request.mutable_shared_path()->set_path(path);
  request.mutable_shared_path()->set_writable(true);
  request.set_storage_location(
      vm_tools::seneschal::SharePathRequest::DOWNLOADS);
  request.set_owner_id(CryptohomeIdForProfile(profile));
  chromeos::DBusThreadManager::Get()->GetSeneschalClient()->SharePath(
      request, base::BindOnce(&CrostiniSharePath::OnSharePathResponse,
                              weak_ptr_factory_.GetWeakPtr(), std::move(path),
                              std::move(callback)));
}

void CrostiniSharePath::OnSharePathResponse(
    std::string path,
    base::OnceCallback<void(bool, std::string)> callback,
    base::Optional<vm_tools::seneschal::SharePathResponse> response) const {
  if (!response.has_value()) {
    std::move(callback).Run(false, "Error sharing " + path);
    return;
  }
  std::move(callback).Run(response.value().success(),
                          response.value().failure_reason());
}

}  // namespace crostini
