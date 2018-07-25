// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_shortcut_installation_task.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_data_retriever.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

BookmarkAppShortcutInstallationTask::BookmarkAppShortcutInstallationTask() =
    default;

BookmarkAppShortcutInstallationTask::~BookmarkAppShortcutInstallationTask() =
    default;

void BookmarkAppShortcutInstallationTask::InstallFromWebContents(
    content::WebContents* web_contents,
    ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  data_retriever().GetWebApplicationInfo(
      web_contents,
      base::BindOnce(
          &BookmarkAppShortcutInstallationTask::OnGetWebApplicationInfo,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BookmarkAppShortcutInstallationTask::OnGetWebApplicationInfo(
    ResultCallback result_callback,
    base::Optional<WebApplicationInfo> web_app_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(crbug.com/864904): Continue the installation process.
  std::move(result_callback)
      .Run(web_app_info ? Result::kSuccess
                        : Result::kGetWebApplicationInfoFailed);
}

}  // namespace extensions
