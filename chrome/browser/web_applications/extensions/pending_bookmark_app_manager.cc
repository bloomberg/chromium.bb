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
    Profile* profile,
    web_app::PendingAppManager::AppInfo app_info) {
  return std::make_unique<BookmarkAppInstallationTask>(profile,
                                                       std::move(app_info));
}

}  // namespace

struct PendingBookmarkAppManager::TaskAndCallback {
  TaskAndCallback(std::unique_ptr<BookmarkAppInstallationTask> task,
                  OnceInstallCallback callback)
      : task(std::move(task)), callback(std::move(callback)) {}
  ~TaskAndCallback() = default;

  std::unique_ptr<BookmarkAppInstallationTask> task;
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
  pending_tasks_and_callbacks_.push_front(std::make_unique<TaskAndCallback>(
      task_factory_.Run(profile_, std::move(app_to_install)),
      std::move(callback)));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PendingBookmarkAppManager::MaybeStartNextInstallation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PendingBookmarkAppManager::InstallApps(
    std::vector<AppInfo> apps_to_install,
    const RepeatingInstallCallback& callback) {
  for (auto& app_to_install : apps_to_install) {
    pending_tasks_and_callbacks_.push_back(std::make_unique<TaskAndCallback>(
        task_factory_.Run(profile_, std::move(app_to_install)), callback));
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
  if (current_task_and_callback_)
    return;

  if (pending_tasks_and_callbacks_.empty()) {
    web_contents_.reset();
    return;
  }

  current_task_and_callback_ = std::move(pending_tasks_and_callbacks_.front());
  pending_tasks_and_callbacks_.pop_front();

  CreateWebContentsIfNecessary();
  Observe(web_contents_.get());

  content::NavigationController::LoadURLParams load_params(
      current_task_and_callback_->task->app_info().url);
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

  std::unique_ptr<TaskAndCallback> task_and_callback;
  task_and_callback.swap(current_task_and_callback_);
  std::move(task_and_callback->callback)
      .Run(task_and_callback->task->app_info().url, app_id);
}

void PendingBookmarkAppManager::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  timer_->Stop();
  if (web_contents_->GetMainFrame() != render_frame_host) {
    return;
  }

  if (validated_url != current_task_and_callback_->task->app_info().url) {
    CurrentInstallationFinished(std::string());
    return;
  }

  Observe(nullptr);
  current_task_and_callback_->task->InstallWebAppOrShortcutFromWebContents(
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
