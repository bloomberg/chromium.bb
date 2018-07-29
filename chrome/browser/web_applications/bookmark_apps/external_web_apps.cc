// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/external_web_apps.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/json/json_file_value_serializer.h"
#include "base/threading/thread_restrictions.h"
#include "url/gurl.h"

namespace web_app {

static constexpr char kWebAppManifestUrl[] = "web_app_manifest_url";
static constexpr char kWebAppStartUrl[] = "web_app_start_url";

void ScanDirForExternalWebApps(base::FilePath dir,
                               ScanDirForExternalWebAppsCallback callback) {
  base::AssertBlockingAllowed();
  base::FilePath::StringType extension(FILE_PATH_LITERAL(".json"));
  base::FileEnumerator json_files(dir,
                                  false,  // Recursive.
                                  base::FileEnumerator::FILES);

  std::vector<web_app::PendingAppManager::AppInfo> app_infos;

  for (base::FilePath file = json_files.Next(); !file.empty();
       file = json_files.Next()) {
    if (!file.MatchesExtension(extension)) {
      continue;
    }

    JSONFileValueDeserializer deserializer(file);
    std::string error_msg;
    std::unique_ptr<base::Value> value =
        deserializer.Deserialize(nullptr, &error_msg);
    if (!value) {
      VLOG(2) << file.value() << " was not valid JSON: " << error_msg;
      continue;
    }
    if (value->type() != base::Value::Type::DICTIONARY) {
      VLOG(2) << file.value() << " was not a dictionary as the top level";
      continue;
    }
    std::unique_ptr<base::DictionaryValue> dict_value =
        base::DictionaryValue::From(std::move(value));

    std::string manifest_url_str;
    if (!dict_value->GetString(kWebAppStartUrl, &manifest_url_str) ||
        manifest_url_str.empty() || !GURL(manifest_url_str).is_valid()) {
      VLOG(2) << file.value() << " had an invalid " << kWebAppManifestUrl;
      continue;
    }

    std::string start_url_str;
    if (!dict_value->GetString(kWebAppStartUrl, &start_url_str) ||
        start_url_str.empty()) {
      VLOG(2) << file.value() << " had an invalid " << kWebAppStartUrl;
      continue;
    }
    GURL start_url(start_url_str);
    if (!start_url.is_valid()) {
      VLOG(2) << file.value() << " had an invalid " << kWebAppStartUrl;
      continue;
    }

    app_infos.emplace_back(
        std::move(start_url),
        web_app::PendingAppManager::LaunchContainer::kWindow);
  }

  std::move(callback).Run(std::move(app_infos));
}

}  //  namespace web_app
