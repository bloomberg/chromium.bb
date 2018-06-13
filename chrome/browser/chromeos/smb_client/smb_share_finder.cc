// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_share_finder.h"

namespace chromeos {
namespace smb_client {

void SmbShareFinder::GatherSharesInNetwork(GatherSharesResponse callback) {
  NOTREACHED();
}

void SmbShareFinder::RegisterHostLocator(std::unique_ptr<HostLocator> locator) {
  NOTREACHED();
}

void SmbShareFinder::OnHostsFound(GatherSharesResponse callback,
                                  bool success,
                                  const HostMap& hosts) {
  NOTREACHED();
}

void SmbShareFinder::OnSharesFound(
    const std::string& host_name,
    GatherSharesResponse callback,
    smbprovider::ErrorType error,
    const smbprovider::DirectoryEntryListProto& entries) {
  NOTREACHED();
}

}  // namespace smb_client
}  // namespace chromeos
