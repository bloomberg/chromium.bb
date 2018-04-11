// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_MTP_MANAGER_CLIENT_CHROMEOS_H_
#define COMPONENTS_STORAGE_MONITOR_MTP_MANAGER_CLIENT_CHROMEOS_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"

namespace storage_monitor {

// This class will replace 'MediaTransferProtocolDeviceObserverChromeOS'.
// This client listens for MTP storage attachment and detachment events
// from MtpManager and forwards them to StorageMonitor.
class MtpManagerClientChromeOS : public device::mojom::MtpManagerClient {
 public:
  explicit MtpManagerClientChromeOS(device::mojom::MtpManager* mtp_manager);
  ~MtpManagerClientChromeOS() override;

 protected:
  // device::mojom::MtpManagerClient implementation.
  // Exposed for unit tests.
  void StorageAttached(device::mojom::MtpStorageInfoPtr storage_info) override;
  void StorageDetached(const std::string& storage_name) override;

 private:
  void OnReceivedStorages(
      std::vector<device::mojom::MtpStorageInfoPtr> storage_info_list);

  // Pointer to the MTP manager. Not owned. Client must ensure the MTP
  // manager outlives this object.
  device::mojom::MtpManager* const mtp_manager_;

  mojo::AssociatedBinding<device::mojom::MtpManagerClient> binding_;

  base::WeakPtrFactory<MtpManagerClientChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MtpManagerClientChromeOS);
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_MTP_MANAGER_CLIENT_CHROMEOS_H_
