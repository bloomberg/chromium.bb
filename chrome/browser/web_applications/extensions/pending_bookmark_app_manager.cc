// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_shortcut_installation_task.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

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

struct PendingBookmarkAppManager::Installation {
  Installation(AppInfo info, OnceInstallCallback callback)
      : info(std::move(info)), callback(std::move(callback)) {}
  ~Installation() = default;

  AppInfo info;
  OnceInstallCallback callback;
};

PendingBookmarkAppManager::PendingBookmarkAppManager(Profile* profile)
    : profile_(profile),
      web_contents_factory_(base::BindRepeating(&WebContentsCreateWrapper)),
      task_factory_(base::BindRepeating(&InstallationTaskCreateWrapper)) {}

PendingBookmarkAppManager::~PendingBookmarkAppManager() = default;

void PendingBookmarkAppManager::Install(AppInfo app_to_install,
                                        OnceInstallCallback callback) {
  // Check that we are not already installing the same app.
  if (current_installation_ && current_installation_->info == app_to_install) {
    std::move(callback).Run(app_to_install.url, std::string());
    return;
  }
  for (const auto& installation : installation_queue_) {
    if (installation->info == app_to_install) {
      std::move(callback).Run(app_to_install.url, std::string());
      return;
    }
  }

  installation_queue_.push_back(std::make_unique<Installation>(
      std::move(app_to_install), std::move(callback)));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PendingBookmarkAppManager::MaybeStartNextInstallation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PendingBookmarkAppManager::ProcessAppOperations(
    std::vector<AppInfo> apps_to_install) {}

void PendingBookmarkAppManager::SetFactoriesForTesting(
    WebContentsFactory web_contents_factory,
    TaskFactory task_factory) {
  web_contents_factory_ = std::move(web_contents_factory);
  task_factory_ = std::move(task_factory);
}

void PendingBookmarkAppManager::MaybeStartNextInstallation() {
  if (current_installation_)
    return;

  if (installation_queue_.empty()) {
    web_contents_.reset();
    return;
  }

  current_installation_ = std::move(installation_queue_.front());
  installation_queue_.pop_front();

  CreateWebContentsIfNecessary();
  Observe(web_contents_.get());

  content::NavigationController::LoadURLParams load_params(
      current_installation_->info.url);
  load_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
  web_contents_->GetController().LoadURLWithParams(load_params);
}

void PendingBookmarkAppManager::CreateWebContentsIfNecessary() {
  if (!web_contents_)
    web_contents_ = web_contents_factory_.Run(profile_);
}

void PendingBookmarkAppManager::OnInstalled(
    BookmarkAppInstallationTask::Result result) {
  CurrentInstallationFinished(result.app_id);
}

void PendingBookmarkAppManager::CurrentInstallationFinished(
    const std::string& app_id) {
  // Post a task to avoid reentrancy issues e.g. adding a WebContentsObserver
  // while a previous observer call is being executed. Post a task before
  // running the callback in case the callback tries to install another
  // app.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PendingBookmarkAppManager::MaybeStartNextInstallation,
                     weak_ptr_factory_.GetWeakPtr()));

  std::unique_ptr<Installation> installation;
  installation.swap(current_installation_);
  std::move(installation->callback).Run(installation->info.url, app_id);
}

void PendingBookmarkAppManager::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (web_contents_->GetMainFrame() != render_frame_host) {
    return;
  }

  if (validated_url != current_installation_->info.url) {
    CurrentInstallationFinished(std::string());
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
  CurrentInstallationFinished(std::string());
}

}  // namespace extensions
