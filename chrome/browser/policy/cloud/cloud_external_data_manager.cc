// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/cloud_external_data_manager.h"

namespace policy {

CloudExternalDataManager::MetadataEntry::MetadataEntry() {
}

CloudExternalDataManager::MetadataEntry::MetadataEntry(const std::string& url,
                                                       const std::string& hash)
    : url(url),
      hash(hash) {
}

}  // namespace policy
