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
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace {

constexpr char kWebAppManifestUrl[] = "web_app_manifest_url";
constexpr char kWebAppStartUrl[] = "web_app_start_url";

std::vector<web_app::PendingAppManager::AppInfo> ScanDir(base::FilePath dir) {
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

  return app_infos;
}

}  // namespace

namespace web_app {

std::vector<web_app::PendingAppManager::AppInfo>
ScanDirForExternalWebAppsForTesting(base::FilePath dir) {
  return ScanDir(dir);
}

void ScanForExternalWebApps(ScanForExternalWebAppsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if !defined(OS_CHROMEOS)
  // As of mid 2018, only Chrome OS has default/external web apps, and
  // chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS is only defined for OS_LINUX,
  // which includes OS_CHROMEOS.
  //
  // In the future we may have default/external web apps for other platforms,
  // but for now run the callback with an empty vector.
  std::move(callback).Run(std::vector<web_app::PendingAppManager::AppInfo>());
#else
  // For manual testing, it can be useful to s/STANDALONE/USER/, as writing to
  // "$HOME/.config/chromium/test-user/.config/chromium/External Extensions"
  // does not require root ACLs, unlike "/usr/share/chromium/extensions".
  //
  // TODO(nigeltao): do we want to append a sub-directory name, analogous to
  // the "arc" in "/usr/share/chromium/extensions/arc" as per
  // chrome/browser/ui/app_list/arc/arc_default_app_list.cc? Or should we not
  // sort "system apps" into directories based on their platform (e.g. ARC,
  // PWA, etc.), and instead examine the JSON contents (e.g. an "activity"
  // key means ARC, "web_app_start_url" key means PWA, etc.)?
  base::FilePath dir;
  if (!base::PathService::Get(chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS,
                              &dir)) {
    LOG(ERROR) << "ScanForExternalWebApps: base::PathService::Get failed";
    return;
  }

  // Do a two-part callback dance, across different TaskRunners.
  //
  // 1. Schedule ScanDir to happen on a background thread, so that we don't
  // block the UI thread. When that's done
  // , base::PostTaskWithTraitsAndReplyWithResult will bounce us back to the
  // originating thread (the UI thread).
  //
  // 2. In |callback|, forward the vector of AppInfo's on to the
  // pending_app_manager_, which can only be called on the UI thread.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ScanDir, dir), std::move(callback));
#endif
}

}  //  namespace web_app
