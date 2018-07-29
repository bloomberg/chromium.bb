// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include "base/bind.h"
#include "base/path_service.h"
#include "base/task_scheduler/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/bookmark_apps/external_web_apps.h"
#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/common/chrome_paths.h"

namespace web_app {

// static
WebAppProvider* WebAppProvider::Get(Profile* profile) {
  return WebAppProviderFactory::GetForProfile(profile);
}

WebAppProvider::WebAppProvider(PrefService* pref_service)
    : web_app_policy_manager_(
          std::make_unique<WebAppPolicyManager>(pref_service)) {
#if defined(OS_CHROMEOS)
  // As of mid 2018, only Chrome OS has default web apps or external web apps.
  // In the future, we might open external web apps to other flavors.
  ScanForExternalWebApps();
#endif  // defined(OS_CHROMEOS)
}

WebAppProvider::~WebAppProvider() = default;

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

  auto callback =
      base::BindOnce(&WebAppProvider::ScanForExternalWebAppsCallback,
                     weak_ptr_factory_.GetWeakPtr());

  base::PostTaskWithTraits(FROM_HERE,
                           {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
                           base::BindOnce(&web_app::ScanDirForExternalWebApps,
                                          dir, std::move(callback)));
#endif  // !defined(OS_CHROMEOS)
}

void WebAppProvider::ScanForExternalWebAppsCallback(
    std::vector<web_app::PendingAppManager::AppInfo> app_infos) {
  // TODO(nigeltao/ortuno): we shouldn't need a *policy* manager to get to a
  // PendingAppManager. As ortuno@ says, "We should refactor PendingAppManager
  // to no longer be owned by WebAppPolicyManager. I made it that way since at
  // the time, WebAppPolicyManager was its only client."
  //
  // TODO(nigeltao/ortuno): drop the const_cast. Either
  // WebAppPolicyManager::pending_app_manager should lose the const or we
  // should acquire a (non-const) PendingAppManager by other means.
  auto& pending_app_manager = const_cast<web_app::PendingAppManager&>(
      web_app_policy_manager_->pending_app_manager());

  // TODO(nigeltao/ortuno): confirm that the PendingAppManager callee is
  // responsible for filtering out already-installed apps.
  //
  // TODO(nigeltao/ortuno): does the PendingAppManager care which thread we're
  // on (e.g. "the UI thread", "the File thread") when we call into it? Do we
  // need to bounce to another thread from here? Note that, in the long term,
  // we might not be in the browser process, so there might not be the concept
  // of "the UI thread".
  pending_app_manager.ProcessAppOperations(std::move(app_infos));
}

}  // namespace web_app
