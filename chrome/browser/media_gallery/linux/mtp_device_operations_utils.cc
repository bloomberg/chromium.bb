// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/linux/mtp_device_operations_utils.h"

#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"

namespace chrome {

// Returns MediaTransferProtocolManager instance on success or NULL on failure.
device::MediaTransferProtocolManager* GetMediaTransferProtocolManager() {
  device::MediaTransferProtocolManager* mtp_device_mgr =
      device::MediaTransferProtocolManager::GetInstance();
  DCHECK(mtp_device_mgr);
  return mtp_device_mgr;
}

}  // namespace chrome
