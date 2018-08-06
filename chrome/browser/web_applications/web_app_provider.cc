// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/path_service.h"
#include "base/task/post_task_forward.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/bookmark_apps/external_web_apps.h"
#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

// static
WebAppProvider* WebAppProvider::Get(Profile* profile) {
  return WebAppProviderFactory::GetForProfile(profile);
}

WebAppProvider::WebAppProvider(Profile* profile)
    : pending_app_manager_(
          std::make_unique<extensions::PendingBookmarkAppManager>(profile)),
      web_app_policy_manager_(
          std::make_unique<WebAppPolicyManager>(profile->GetPrefs(),
                                                pending_app_manager_.get())) {
#if defined(OS_CHROMEOS)
  // As of mid 2018, only Chrome OS has default web apps or external web apps.
  // In the future, we might open external web apps to other flavors.

  // Do a three-part callback dance, across different TaskRunners.
  //
  // 1. Start here, on the UI thread. We don't do any work straight away, in
  // the constructor, because browser startup is already a busy time, and this
  // scan is low priority. Also, some tests, for better or worse, assume that
  // there are no pending tasks on the UI TaskRunner queue after all the
  // KeyedService instances are spun up. Instead, we schedule
  // ScanForExternalWebApps to happen as an after-startup task, explicitly on
  // the UI thread.
  //
  // 2. In ScanForExternalWebApps, schedule web_app::ScanDirForExternalWebApps
  // to happen on a background thread, so that we don't block the UI thread.
  // When that's done, base::PostTaskWithTraitsAndReplyWithResult will bounce
  // us back to the originating thread (the UI thread).
  //
  // 3. In ScanForExternalWebAppsCallback, forward the vector of AppInfo's on
  // to the pending_app_manager_, which can only be called on the UI thread.
  content::BrowserThread::PostAfterStartupTask(
      FROM_HERE,
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::UI),
      base::BindOnce(&WebAppProvider::ScanForExternalWebApps,
                     weak_ptr_factory_.GetWeakPtr()));
#endif  // defined(OS_CHROMEOS)
}

WebAppProvider::~WebAppProvider() = default;

// static
void WebAppProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  WebAppPolicyManager::RegisterProfilePrefs(registry);
}

void WebAppProvider::ScanForExternalWebApps() {
#if !defined(OS_CHROMEOS)
// chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS is only defined for OS_LINUX.
// OS_CHROMEOS is a variant of OS_LINUX.
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

  auto task = base::BindOnce(&web_app::ScanDirForExternalWebApps, dir);
  auto reply = base::BindOnce(&WebAppProvider::ScanForExternalWebAppsCallback,
                              weak_ptr_factory_.GetWeakPtr());

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      std::move(task), std::move(reply));
#endif  // !defined(OS_CHROMEOS)
}

void WebAppProvider::ScanForExternalWebAppsCallback(
    std::vector<web_app::PendingAppManager::AppInfo> app_infos) {
#if defined(OS_CHROMEOS)
  pending_app_manager_->ProcessAppOperations(std::move(app_infos));
#endif
}

}  // namespace web_app
