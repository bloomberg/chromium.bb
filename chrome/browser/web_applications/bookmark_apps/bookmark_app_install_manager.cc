// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/bookmark_app_install_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/install_manager_observer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

// A single installation task. It is possible to have many concurrent
// installations per one |web_contents|.
// This class is a simple holder of BookmarkAppHelper and limits its lifetime to
// match WebContents.
class InstallTask : public content::WebContentsObserver,
                    public web_app::InstallManagerObserver {
 public:
  InstallTask(BookmarkAppInstallManager* install_manager,
              std::unique_ptr<BookmarkAppHelper> bookmark_app_helper,
              web_app::InstallManager::OnceInstallCallback callback)
      : WebContentsObserver(),
        bookmark_app_helper_(std::move(bookmark_app_helper)),
        callback_(std::move(callback)) {
    manager_observer_.Add(install_manager);
  }

  ~InstallTask() override { DCHECK(!callback_); }

  void AttachLifetimeTo(content::WebContents* web_contents) {
    Observe(web_contents);
  }

  // WebContentsObserver:
  void WebContentsDestroyed() override {
    DCHECK(callback_);

    CallInstallCallback(web_app::AppId(),
                        web_app::InstallResultCode::kWebContentsDestroyed);
    // BookmarkAppHelper should not outsurvive |web_contents|.
    // This |reset| invalidates all weak references to BookmarkAppHelper and
    // cancels BookmarkAppHelper-related tasks posted to message loops.
    // This |reset| also destroys |this| InstallTask since it is owned by the
    // callback bind state.
    bookmark_app_helper_.reset();
  }

  // InstallManagerObserver:
  void InstallManagerReset() override {
    DCHECK(callback_);

    CallInstallCallback(web_app::AppId(),
                        web_app::InstallResultCode::kInstallManagerDestroyed);
    // BookmarkAppHelper should not outsurvive profile and its keyed services.
    // This |reset| also destroys |this| InstallTask since it is owned by the
    // callback bind state.
    bookmark_app_helper_.reset();
  }

  void CallInstallCallback(const web_app::AppId& app_id,
                           web_app::InstallResultCode code) {
    if (callback_)
      std::move(callback_).Run(app_id, code);

    Observe(nullptr);
    manager_observer_.RemoveAll();
  }

 private:
  std::unique_ptr<BookmarkAppHelper> bookmark_app_helper_;
  web_app::InstallManager::OnceInstallCallback callback_;
  ScopedObserver<web_app::InstallManager, InstallTask> manager_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(InstallTask);
};

void DestroyInstallTask(std::unique_ptr<InstallTask> install_task) {
  install_task.reset();
}

void OnBookmarkAppInstalled(std::unique_ptr<InstallTask> install_task,
                            const Extension* app,
                            const WebApplicationInfo& web_app_info) {
  if (app) {
    install_task->CallInstallCallback(app->id(),
                                      web_app::InstallResultCode::kSuccess);
  } else {
    install_task->CallInstallCallback(
        web_app::AppId(), web_app::InstallResultCode::kFailedUnknownReason);
  }

  // OnBookmarkAppInstalled is called synchronously by BookmarkAppHelper.
  // Post async task to destroy InstallTask with BookmarkAppHelper later.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(DestroyInstallTask, std::move(install_task)));
}

}  // namespace

BookmarkAppInstallManager::BookmarkAppInstallManager(Profile* profile)
    : profile_(profile) {}

BookmarkAppInstallManager::~BookmarkAppInstallManager() = default;

bool BookmarkAppInstallManager::CanInstallWebApp(
    content::WebContents* web_contents) {
  return extensions::TabHelper::FromWebContents(web_contents)
      ->CanCreateBookmarkApp();
}

void BookmarkAppInstallManager::InstallWebApp(
    content::WebContents* web_contents,
    bool force_shortcut_app,
    WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback) {
  // |dialog_callback| is ignored here: BookmarkAppHelper directly uses UI's
  // chrome::ShowPWAInstallDialog.
  // |install_source| is also ignored here: extensions::TabHelper specifies it.
  extensions::TabHelper::FromWebContents(web_contents)
      ->CreateHostedAppFromWebContents(
          force_shortcut_app,
          base::BindOnce(
              [](OnceInstallCallback callback, const ExtensionId& app_id,
                 bool success) {
                std::move(callback).Run(
                    app_id,
                    success ? web_app::InstallResultCode::kSuccess
                            : web_app::InstallResultCode::kFailedUnknownReason);
              },
              std::move(callback)));
}

void BookmarkAppInstallManager::InstallWebAppFromBanner(
    content::WebContents* web_contents,
    WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback) {
  // |dialog_callback| is ignored here:
  // BookmarkAppHelper directly uses UI's chrome::ShowPWAInstallDialog.

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  WebApplicationInfo web_app_info;

  auto bookmark_app_helper = std::make_unique<BookmarkAppHelper>(
      profile, web_app_info, web_contents, install_source);

  BookmarkAppHelper* helper = bookmark_app_helper.get();

  auto install_task = std::make_unique<InstallTask>(
      this, std::move(bookmark_app_helper), std::move(callback));
  install_task->AttachLifetimeTo(web_contents);

  // InstallTask is owned by the bind state and will be disposed in
  // DestroyInstallTask.
  helper->Create(base::BindRepeating(OnBookmarkAppInstalled,
                                     base::Passed(std::move(install_task))));
}

void BookmarkAppInstallManager::InstallWebAppFromInfo(
    std::unique_ptr<WebApplicationInfo> web_application_info,
    bool no_network_install,
    WebappInstallSource install_source,
    OnceInstallCallback callback) {
  auto bookmark_app_helper = std::make_unique<BookmarkAppHelper>(
      profile_, *web_application_info, /*web_contents=*/nullptr,
      install_source);

  if (no_network_install) {
    // We should only install windowed apps via this method.
    bookmark_app_helper->set_forced_launch_type(extensions::LAUNCH_TYPE_WINDOW);
    bookmark_app_helper->set_is_no_network_install();
  }

  BookmarkAppHelper* helper = bookmark_app_helper.get();

  auto install_task = std::make_unique<InstallTask>(
      this, std::move(bookmark_app_helper), std::move(callback));

  // InstallTask is owned by the bind state and will be disposed in
  // DestroyInstallTask.
  helper->Create(base::BindRepeating(OnBookmarkAppInstalled,
                                     base::Passed(std::move(install_task))));
}

}  // namespace extensions
