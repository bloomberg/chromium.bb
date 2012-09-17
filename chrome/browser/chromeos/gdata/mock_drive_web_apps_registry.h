// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_MOCK_DRIVE_WEB_APPS_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_MOCK_DRIVE_WEB_APPS_REGISTRY_H_

#include "chrome/browser/chromeos/gdata/drive_file_system.h"
#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gdata {

class MockDriveWebAppsRegistry : public DriveWebAppsRegistryInterface {
 public:
  MockDriveWebAppsRegistry();
  virtual ~MockDriveWebAppsRegistry();

  MOCK_METHOD3(GetWebAppsForFile, void(const FilePath& file,
                                       const std::string& mime_type,
                                       ScopedVector<DriveWebAppInfo>* apps));
  MOCK_METHOD1(GetExtensionsForWebStoreApp,
               std::set<std::string>(const std::string& web_store_id));
  MOCK_METHOD1(UpdateFromFeed, void(const AccountMetadataFeed& metadata));
  MOCK_METHOD1(UpdateFromApplicationList, void(const AppList& applist));
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_MOCK_DRIVE_WEB_APPS_REGISTRY_H_
