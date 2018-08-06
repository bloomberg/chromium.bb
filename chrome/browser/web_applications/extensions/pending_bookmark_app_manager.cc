// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_shortcut_installation_task.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/navigation_controller.h"

namespace extensions {

namespace {

std::unique_ptr<content::WebContents> WebContentsCreateWrapper(
    Profile* profile) {
  return content::WebContents::Create(
      content::WebContents::CreateParams(profile));
}

std::unique_ptr<BookmarkAppInstallationTask> InstallationTaskCreateWrapper(
    Profile* profile) {
  // TODO(crbug.com/864904): Use an installation task that can install Web Apps
  // and Web Shortcuts.
  return std::make_unique<BookmarkAppShortcutInstallationTask>(profile);
}

}  // namespace

PendingBookmarkAppManager::PendingBookmarkAppManager(Profile* profile)
    : profile_(profile),
      web_contents_factory_(base::BindRepeating(&WebContentsCreateWrapper)),
      task_factory_(base::BindRepeating(&InstallationTaskCreateWrapper)) {}

PendingBookmarkAppManager::~PendingBookmarkAppManager() = default;

void PendingBookmarkAppManager::Install(AppInfo app_to_install,
                                        InstallCallback callback) {
  // The app is already being installed.
  if (current_install_info_ && *current_install_info_ == app_to_install) {
    std::move(callback).Run(false);
    return;
  }

  current_install_info_ = std::make_unique<AppInfo>(std::move(app_to_install));
  current_install_callback_ = std::move(callback);

  CreateWebContentsIfNecessary();
  Observe(web_contents_.get());

  content::NavigationController::LoadURLParams load_params(
      current_install_info_->url);
  load_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
  web_contents_->GetController().LoadURLWithParams(load_params);
}

void PendingBookmarkAppManager::ProcessAppOperations(
    std::vector<AppInfo> apps_to_install) {}

void PendingBookmarkAppManager::SetFactoriesForTesting(
    WebContentsFactory web_contents_factory,
    TaskFactory task_factory) {
  web_contents_factory_ = std::move(web_contents_factory);
  task_factory_ = std::move(task_factory);
}

void PendingBookmarkAppManager::CreateWebContentsIfNecessary() {
  if (!web_contents_)
    web_contents_ = web_contents_factory_.Run(profile_);
}

void PendingBookmarkAppManager::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (web_contents_->GetMainFrame() != render_frame_host) {
    return;
  }

  if (validated_url != current_install_info_->url) {
    std::move(current_install_callback_).Run(false);
    return;
  }

  Observe(nullptr);
  current_installation_task_ = task_factory_.Run(profile_);
  static_cast<BookmarkAppShortcutInstallationTask*>(
      current_installation_task_.get())
      ->InstallFromWebContents(
          web_contents_.get(),
          base::BindOnce(&PendingBookmarkAppManager::OnInstalled,
                         // Safe because the installation task will not run its
                         // callback after being deleted and this class owns the
                         // task.
                         base::Unretained(this)));
}

void PendingBookmarkAppManager::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  if (web_contents_->GetMainFrame() != render_frame_host) {
    return;
  }

  Observe(nullptr);
  // TODO(crbug.com/864904): Only destroy the WebContents if there are no
  // queued installation requests.
  web_contents_.reset();
  current_install_info_.reset();

  std::move(current_install_callback_).Run(false);
}

void PendingBookmarkAppManager::OnInstalled(
    BookmarkAppInstallationTask::Result result) {
  // TODO(crbug.com/864904): Only destroy the WebContents if there are no
  // queued installation requests.
  web_contents_.reset();
  current_install_info_.reset();
  std::move(current_install_callback_)
      .Run(result == BookmarkAppInstallationTask::Result::kSuccess);
}

}  // namespace extensions
