// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

namespace {

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

PendingBookmarkAppManager::PendingBookmarkAppManager(
    Profile* profile,
    web_app::AppRegistrar* registrar)
    : profile_(profile),
      registrar_(registrar),
      uninstaller_(
          std::make_unique<BookmarkAppUninstaller>(profile_, registrar_)),
      extension_ids_map_(profile->GetPrefs()),
      url_loader_(std::make_unique<web_app::WebAppUrlLoader>()),
      task_factory_(base::BindRepeating(&InstallationTaskCreateWrapper)) {}

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

void PendingBookmarkAppManager::UninstallApps(
    std::vector<GURL> apps_to_uninstall,
    const UninstallCallback& callback) {
  for (auto& app_to_uninstall : apps_to_uninstall) {
    callback.Run(app_to_uninstall,
                 uninstaller_->UninstallApp(app_to_uninstall));
  }
}

std::vector<GURL> PendingBookmarkAppManager::GetInstalledAppUrls(
    web_app::InstallSource install_source) const {
  return web_app::ExtensionIdsMap::GetInstalledAppUrls(profile_,
                                                       install_source);
}

base::Optional<std::string> PendingBookmarkAppManager::LookupAppId(
    const GURL& url) const {
  return extension_ids_map_.LookupExtensionId(url);
}

void PendingBookmarkAppManager::SetTaskFactoryForTesting(
    TaskFactory task_factory) {
  task_factory_ = std::move(task_factory);
}

void PendingBookmarkAppManager::SetUninstallerForTesting(
    std::unique_ptr<BookmarkAppUninstaller> uninstaller) {
  uninstaller_ = std::move(uninstaller);
}

void PendingBookmarkAppManager::SetUrlLoaderForTesting(
    std::unique_ptr<web_app::WebAppUrlLoader> url_loader) {
  url_loader_ = std::move(url_loader);
}

base::Optional<bool> PendingBookmarkAppManager::IsExtensionPresentAndInstalled(
    const std::string& extension_id) {
  if (registrar_->IsInstalled(extension_id)) {
    return base::Optional<bool>(true);
  }

  if (registrar_->WasExternalAppUninstalledByUser(extension_id)) {
    return base::Optional<bool>(false);
  }

  return base::nullopt;
}

void PendingBookmarkAppManager::MaybeStartNextInstallation() {
  if (current_task_and_callback_)
    return;

  while (!pending_tasks_and_callbacks_.empty()) {
    std::unique_ptr<TaskAndCallback> front =
        std::move(pending_tasks_and_callbacks_.front());
    pending_tasks_and_callbacks_.pop_front();

    const web_app::PendingAppManager::AppInfo& app_info =
        front->task->app_info();

    if (app_info.always_update) {
      StartInstallationTask(std::move(front));
      return;
    }

    base::Optional<std::string> extension_id =
        extension_ids_map_.LookupExtensionId(app_info.url);

    if (extension_id) {
      base::Optional<bool> opt =
          IsExtensionPresentAndInstalled(extension_id.value());
      if (opt.has_value()) {
        bool installed = opt.value();
        if (installed || !app_info.override_previous_user_uninstall) {
          // TODO(crbug.com/878262): Handle the case where the app is already
          // installed but from a different source.
          std::move(front->callback)
              .Run(app_info.url,
                   installed
                       ? web_app::InstallResultCode::kAlreadyInstalled
                       : web_app::InstallResultCode::kPreviouslyUninstalled);
          continue;
        }
      }
    }
    StartInstallationTask(std::move(front));
    return;
  }

  web_contents_.reset();
}

void PendingBookmarkAppManager::StartInstallationTask(
    std::unique_ptr<TaskAndCallback> task) {
  DCHECK(!current_task_and_callback_);
  current_task_and_callback_ = std::move(task);

  CreateWebContentsIfNecessary();

  url_loader_->LoadUrl(current_task_and_callback_->task->app_info().url,
                       web_contents_.get(),
                       base::BindOnce(&PendingBookmarkAppManager::OnUrlLoaded,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void PendingBookmarkAppManager::CreateWebContentsIfNecessary() {
  if (web_contents_)
    return;

  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  BookmarkAppInstallationTask::CreateTabHelpers(web_contents_.get());
}

void PendingBookmarkAppManager::OnUrlLoaded(
    web_app::WebAppUrlLoader::Result result) {
  if (result != web_app::WebAppUrlLoader::Result::kUrlLoaded) {
    CurrentInstallationFinished(base::nullopt);
    return;
  }

  current_task_and_callback_->task->Install(
      web_contents_.get(),
      base::BindOnce(&PendingBookmarkAppManager::OnInstalled,
                     // Safe because the installation task will not run its
                     // callback after being deleted and this class owns the
                     // task.
                     base::Unretained(this)));
}

void PendingBookmarkAppManager::OnInstalled(
    BookmarkAppInstallationTask::Result result) {
  CurrentInstallationFinished(result.app_id);
}

void PendingBookmarkAppManager::CurrentInstallationFinished(
    const base::Optional<std::string>& app_id) {
  // Post a task to avoid InstallableManager crashing and do so before
  // running the callback in case the callback tries to install another
  // app.
  // TODO(crbug.com/943848): Run next installation synchronously once
  // InstallableManager is fixed.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PendingBookmarkAppManager::MaybeStartNextInstallation,
                     weak_ptr_factory_.GetWeakPtr()));

  auto install_result_code =
      app_id ? web_app::InstallResultCode::kSuccess
             : web_app::InstallResultCode::kFailedUnknownReason;

  std::unique_ptr<TaskAndCallback> task_and_callback;
  task_and_callback.swap(current_task_and_callback_);
  std::move(task_and_callback->callback)
      .Run(task_and_callback->task->app_info().url, install_result_code);
}

}  // namespace extensions
