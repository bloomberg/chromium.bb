// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/external_web_apps.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

static constexpr char kWebAppDefaultApps[] = "web_app_default_apps";

// Returns the chrome/test/data/web_app_default_apps/sub_dir directory that
// holds the *.json data files from which ScanDirForExternalWebApps should
// extract URLs from.
static base::FilePath test_dir(const char* sub_dir) {
  base::FilePath dir;
  if (!base::PathService::Get(chrome::DIR_TEST_DATA, &dir)) {
    ADD_FAILURE()
        << "base::PathService::Get could not resolve chrome::DIR_TEST_DATA";
  }
  return dir.AppendASCII(kWebAppDefaultApps).AppendASCII(sub_dir);
}

using AppInfos = std::vector<web_app::PendingAppManager::AppInfo>;

}  // namespace

class ScanDirForExternalWebAppsTest : public testing::Test {};

TEST_F(ScanDirForExternalWebAppsTest, GoodJson) {
  // The good_json directory contains two good JSON files:
  // chrome_platform_status.json and google_io_2016.json.
  web_app::ScanDirForExternalWebApps(
      test_dir("good_json"), base::BindOnce([](AppInfos app_infos) {
        static std::string urls[] = {
            "https://www.chromestatus.com/features",
            "https://events.google.com/io2016/?utm_source=web_app_manifest",
        };

        EXPECT_EQ(2u, app_infos.size());

        for (const auto& url : urls) {
          EXPECT_NE(
              app_infos.end(),
              std::find(
                  app_infos.begin(), app_infos.end(),
                  web_app::PendingAppManager::AppInfo(
                      GURL(url),
                      web_app::PendingAppManager::LaunchContainer::kWindow)));
        }
      }));
}

TEST_F(ScanDirForExternalWebAppsTest, BadJson) {
  // The bad_json directory contains one (malformed) JSON file.
  web_app::ScanDirForExternalWebApps(test_dir("bad_json"),
                                     base::BindOnce([](AppInfos app_infos) {
                                       EXPECT_EQ(0u, app_infos.size());
                                     }));
}

TEST_F(ScanDirForExternalWebAppsTest, TxtButNoJson) {
  // The txt_but_no_json directory contains one file, and the contents of that
  // file is valid JSON, but that file's name does not end with ".json".
  web_app::ScanDirForExternalWebApps(test_dir("txt_but_no_json"),
                                     base::BindOnce([](AppInfos app_infos) {
                                       EXPECT_EQ(0u, app_infos.size());
                                     }));
}

TEST_F(ScanDirForExternalWebAppsTest, MixedJson) {
  // The mixed_json directory contains one empty JSON file, one malformed JSON
  // file and one good JSON file. ScanDirForExternalWebApps should still pick
  // up that one good JSON file: polytimer.json.
  web_app::ScanDirForExternalWebApps(
      test_dir("mixed_json"), base::BindOnce([](AppInfos app_infos) {
        EXPECT_EQ(1u, app_infos.size());
        if (app_infos.size() == 1) {
          EXPECT_EQ(app_infos[0].url.spec(),
                    std::string("https://polytimer.rocks/?homescreen=1"));
        }
      }));
}
