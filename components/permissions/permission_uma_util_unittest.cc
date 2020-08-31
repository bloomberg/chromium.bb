// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_uma_util.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

class PermissionUmaUtilTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
  TestPermissionsClient permissions_client_;
};

TEST_F(PermissionUmaUtilTest, ScopedevocationReporter) {
  content::TestBrowserContext browser_context;

  // TODO(tsergeant): Add more comprehensive tests of PermissionUmaUtil.
  base::HistogramTester histograms;
  HostContentSettingsMap* map =
      PermissionsClient::Get()->GetSettingsMap(&browser_context);
  GURL host("https://example.com");
  ContentSettingsPattern host_pattern =
      ContentSettingsPattern::FromURLNoWildcard(host);
  ContentSettingsPattern host_containing_wildcards_pattern =
      ContentSettingsPattern::FromString("https://[*.]example.com/");
  ContentSettingsType type = ContentSettingsType::GEOLOCATION;
  PermissionSourceUI source_ui = PermissionSourceUI::SITE_SETTINGS;

  // Allow->Block triggers a revocation.
  map->SetContentSettingDefaultScope(host, host, type, std::string(),
                                     CONTENT_SETTING_ALLOW);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type, std::string(),
                                       CONTENT_SETTING_BLOCK);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 1);

  // Block->Allow does not trigger a revocation.
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type, std::string(),
                                       CONTENT_SETTING_ALLOW);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 1);

  // Allow->Default triggers a revocation when default is 'ask'.
  map->SetDefaultContentSetting(type, CONTENT_SETTING_ASK);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type, std::string(),
                                       CONTENT_SETTING_DEFAULT);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 2);

  // Allow->Default does not trigger a revocation when default is 'allow'.
  map->SetDefaultContentSetting(type, CONTENT_SETTING_ALLOW);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host, host, type, source_ui);
    map->SetContentSettingDefaultScope(host, host, type, std::string(),
                                       CONTENT_SETTING_DEFAULT);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 2);

  // Allow->Block with url pattern string triggers a revocation.
  map->SetContentSettingDefaultScope(host, host, type, std::string(),
                                     CONTENT_SETTING_ALLOW);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host_pattern, host_pattern, type, source_ui);
    map->SetContentSettingCustomScope(host_pattern, host_pattern, type,
                                      std::string(), CONTENT_SETTING_BLOCK);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 3);

  // Allow->Block with non url pattern string does not trigger a revocation.
  map->SetContentSettingDefaultScope(host, host, type, std::string(),
                                     CONTENT_SETTING_ALLOW);
  {
    PermissionUmaUtil::ScopedRevocationReporter scoped_revocation_reporter(
        &browser_context, host_containing_wildcards_pattern, host_pattern, type,
        source_ui);
    map->SetContentSettingCustomScope(host_containing_wildcards_pattern,
                                      host_pattern, type, std::string(),
                                      CONTENT_SETTING_BLOCK);
  }
  histograms.ExpectBucketCount("Permissions.Action.Geolocation",
                               static_cast<int>(PermissionAction::REVOKED), 3);
}

}  // namespace permissions
