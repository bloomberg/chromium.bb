// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_CLOUD_EXTERNAL_DATA_MANAGER_H_
#define CHROME_BROWSER_POLICY_CLOUD_CLOUD_EXTERNAL_DATA_MANAGER_H_

#include <map>
#include <string>

#include "chrome/browser/policy/external_data_manager.h"

namespace policy {

// Downloads, verifies, caches and retrieves external data referenced by
// policies.
// This a common base class used by cloud policy implementations and mocks.
class CloudExternalDataManager : public ExternalDataManager {
 public:
  struct MetadataEntry {
    MetadataEntry();
    MetadataEntry(const std::string& url, const std::string& hash);

    std::string url;
    std::string hash;
  };
  // Maps from policy names to the metadata specifying the external data that
  // each of the policies references.
  typedef std::map<std::string, MetadataEntry> Metadata;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CLOUD_EXTERNAL_DATA_MANAGER_H_
