// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/mtp_manager_client_chromeos.h"

#include <utility>

namespace storage_monitor {

MtpManagerClientChromeOS::MtpManagerClientChromeOS(
    device::mojom::MtpManager* mtp_manager)
    : mtp_manager_(mtp_manager), binding_(this), weak_ptr_factory_(this) {
  device::mojom::MtpManagerClientAssociatedPtrInfo client;
  binding_.Bind(mojo::MakeRequest(&client));
  mtp_manager_->EnumerateStoragesAndSetClient(
      std::move(client),
      base::BindOnce(&MtpManagerClientChromeOS::OnReceivedStorages,
                     weak_ptr_factory_.GetWeakPtr()));
}

MtpManagerClientChromeOS::~MtpManagerClientChromeOS() {}

// device::mojom::MtpManagerClient override.
void MtpManagerClientChromeOS::StorageAttached(
    device::mojom::MtpStorageInfoPtr mtp_storage_info) {
  // TODO(donna.wu@intel.com) implement this when replacing mojo methods.
  //   1. adjust local storage_info map
  //   2. notify higher StorageMonitor observers about the event.
}

// device::mojom::MtpManagerClient override.
void MtpManagerClientChromeOS::StorageDetached(
    const std::string& storage_name) {
  // TODO(donna.wu@intel.com) implement this when replacing mojo methods.
  //   1. adjust local storage_info map
  //   2. notify higher StorageMonitor observers about the event.
}

void MtpManagerClientChromeOS::OnReceivedStorages(
    std::vector<device::mojom::MtpStorageInfoPtr> storage_info_list) {
  for (auto& storage_info : storage_info_list)
    StorageAttached(std::move(storage_info));
}

}  // namespace storage_monitor
