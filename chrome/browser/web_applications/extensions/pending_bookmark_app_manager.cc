// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

namespace {

const int kSecondsToWaitForWebContentsLoad = 30;

std::unique_ptr<content::WebContents> WebContentsCreateWrapper(
    Profile* profile) {
  return content::WebContents::Create(
      content::WebContents::CreateParams(profile));
}

std::unique_ptr<BookmarkAppInstallationTask> InstallationTaskCreateWrapper(
    Profile* profile) {
  return std::make_unique<BookmarkAppInstallationTask>(profile);
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
      task_factory_(base::BindRepeating(&InstallationTaskCreateWrapper)),
      timer_(std::make_unique<base::OneShotTimer>()) {}

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

  installation_queue_.push_front(std::make_unique<Installation>(
      std::move(app_to_install), std::move(callback)));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PendingBookmarkAppManager::MaybeStartNextInstallation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PendingBookmarkAppManager::InstallApps(
    std::vector<AppInfo> apps_to_install,
    const RepeatingInstallCallback& callback) {
  for (auto& app_to_install : apps_to_install) {
    installation_queue_.push_back(
        std::make_unique<Installation>(std::move(app_to_install), callback));
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PendingBookmarkAppManager::MaybeStartNextInstallation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PendingBookmarkAppManager::SetFactoriesForTesting(
    WebContentsFactory web_contents_factory,
    TaskFactory task_factory) {
  web_contents_factory_ = std::move(web_contents_factory);
  task_factory_ = std::move(task_factory);
}

void PendingBookmarkAppManager::SetTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  timer_ = std::move(timer);
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
  timer_->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kSecondsToWaitForWebContentsLoad),
      base::BindOnce(&PendingBookmarkAppManager::OnWebContentsLoadTimedOut,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PendingBookmarkAppManager::CreateWebContentsIfNecessary() {
  if (web_contents_)
    return;

  web_contents_ = web_contents_factory_.Run(profile_);
  BookmarkAppInstallationTask::CreateTabHelpers(web_contents_.get());
}

void PendingBookmarkAppManager::OnInstalled(
    BookmarkAppInstallationTask::Result result) {
  CurrentInstallationFinished(result.app_id);
}

void PendingBookmarkAppManager::OnWebContentsLoadTimedOut() {
  web_contents_->Stop();
  Observe(nullptr);
  CurrentInstallationFinished(std::string());
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
  timer_->Stop();
  if (web_contents_->GetMainFrame() != render_frame_host) {
    return;
  }

  if (validated_url != current_installation_->info.url) {
    CurrentInstallationFinished(std::string());
    return;
  }

  Observe(nullptr);
  current_installation_task_ = task_factory_.Run(profile_);
  current_installation_task_->InstallWebAppOrShortcutFromWebContents(
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
  timer_->Stop();
  if (web_contents_->GetMainFrame() != render_frame_host) {
    return;
  }

  Observe(nullptr);
  CurrentInstallationFinished(std::string());
}

}  // namespace extensions
