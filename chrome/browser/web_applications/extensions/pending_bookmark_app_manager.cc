// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_ui_delegate.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

namespace {

std::unique_ptr<BookmarkAppInstallationTask> InstallationTaskCreateWrapper(
    Profile* profile,
    web_app::InstallFinalizer* install_finalizer,
    web_app::InstallOptions install_options) {
  return std::make_unique<BookmarkAppInstallationTask>(
      profile, install_finalizer, std::move(install_options));
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
    web_app::AppRegistrar* registrar,
    web_app::InstallFinalizer* install_finalizer)
    : profile_(profile),
      registrar_(registrar),
      install_finalizer_(install_finalizer),
      uninstaller_(
          std::make_unique<BookmarkAppUninstaller>(profile_, registrar_)),
      extension_ids_map_(profile->GetPrefs()),
      url_loader_(std::make_unique<web_app::WebAppUrlLoader>()),
      task_factory_(base::BindRepeating(&InstallationTaskCreateWrapper)) {}

PendingBookmarkAppManager::~PendingBookmarkAppManager() = default;

void PendingBookmarkAppManager::Install(web_app::InstallOptions install_options,
                                        OnceInstallCallback callback) {
  pending_tasks_and_callbacks_.push_front(std::make_unique<TaskAndCallback>(
      task_factory_.Run(profile_, install_finalizer_,
                        std::move(install_options)),
      std::move(callback)));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PendingBookmarkAppManager::MaybeStartNextInstallation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PendingBookmarkAppManager::InstallApps(
    std::vector<web_app::InstallOptions> install_options_list,
    const RepeatingInstallCallback& callback) {
  for (auto& install_options : install_options_list) {
    pending_tasks_and_callbacks_.push_back(std::make_unique<TaskAndCallback>(
        task_factory_.Run(profile_, install_finalizer_,
                          std::move(install_options)),
        callback));
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PendingBookmarkAppManager::MaybeStartNextInstallation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PendingBookmarkAppManager::UninstallApps(
    std::vector<GURL> uninstall_urls,
    const UninstallCallback& callback) {
  for (auto& url : uninstall_urls) {
    callback.Run(url, uninstaller_->UninstallApp(url));
  }
}

std::vector<GURL> PendingBookmarkAppManager::GetInstalledAppUrls(
    web_app::InstallSource install_source) const {
  return web_app::ExtensionIdsMap::GetInstalledAppUrls(profile_,
                                                       install_source);
}

base::Optional<web_app::AppId> PendingBookmarkAppManager::LookupAppId(
    const GURL& url) const {
  return extension_ids_map_.LookupExtensionId(url);
}

bool PendingBookmarkAppManager::HasAppIdWithInstallSource(
    const web_app::AppId& app_id,
    web_app::InstallSource install_source) const {
  return web_app::ExtensionIdsMap::HasExtensionIdWithInstallSource(
      profile_->GetPrefs(), app_id, install_source);
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

web_app::WebAppUiDelegate& PendingBookmarkAppManager::GetUiDelegate() {
  return web_app::WebAppProviderBase::GetProviderBase(profile_)->ui_delegate();
}

void PendingBookmarkAppManager::MaybeStartNextInstallation() {
  if (current_task_and_callback_)
    return;

  while (!pending_tasks_and_callbacks_.empty()) {
    std::unique_ptr<TaskAndCallback> front =
        std::move(pending_tasks_and_callbacks_.front());
    pending_tasks_and_callbacks_.pop_front();

    const web_app::InstallOptions& install_options =
        front->task->install_options();

    if (install_options.always_update) {
      StartInstallationTask(std::move(front));
      return;
    }

    base::Optional<std::string> extension_id =
        extension_ids_map_.LookupExtensionId(install_options.url);

    // If the URL is not in ExtensionIdsMap, we haven't installed it.
    if (!extension_id.has_value()) {
      StartInstallationTask(std::move(front));
      return;
    }

    if (registrar_->IsInstalled(extension_id.value())) {
      if (install_options.wait_for_windows_closed &&
          GetUiDelegate().GetNumWindowsForApp(extension_id.value()) != 0) {
        GetUiDelegate().NotifyOnAllAppWindowsClosed(
            extension_id.value(),
            base::BindOnce(&PendingBookmarkAppManager::Install,
                           weak_ptr_factory_.GetWeakPtr(), install_options,
                           std::move(front->callback)));
        continue;
      }

      // If the app is already installed, only reinstall it if the app is a
      // placeholder app and the client asked for it to be reinstalled.
      if (install_options.reinstall_placeholder &&
          extension_ids_map_.LookupPlaceholderAppId(install_options.url)
              .has_value()) {
        StartInstallationTask(std::move(front));
        return;
      }

      // Otherwise no need to do anything.
      std::move(front->callback)
          .Run(install_options.url,
               web_app::InstallResultCode::kAlreadyInstalled);
      continue;
    }

    // The app is not installed, but it might have been previously uninstalled
    // by the user. If that's the case, don't install it again unless
    // |override_previous_user_uninstall| is true.
    if (registrar_->WasExternalAppUninstalledByUser(extension_id.value()) &&
        !install_options.override_previous_user_uninstall) {
      std::move(front->callback)
          .Run(install_options.url,
               web_app::InstallResultCode::kPreviouslyUninstalled);
      continue;
    }

    // If neither of the above conditions applies, the app probably got
    // uninstalled but it wasn't been removed from the map. We should install
    // the app in this case.
    StartInstallationTask(std::move(front));
    return;
  }

  web_contents_.reset();
}

bool PendingBookmarkAppManager::UninstallPlaceholderIfNecessary(
    const web_app::InstallOptions install_options) {
  if (!install_options.reinstall_placeholder)
    return true;

  base::Optional<std::string> extension_id =
      extension_ids_map_.LookupPlaceholderAppId(install_options.url);

  if (extension_id.has_value() &&
      registrar_->IsInstalled(extension_id.value())) {
    return uninstaller_->UninstallApp(install_options.url);
  }

  return true;
}

void PendingBookmarkAppManager::StartInstallationTask(
    std::unique_ptr<TaskAndCallback> task) {
  DCHECK(!current_task_and_callback_);
  current_task_and_callback_ = std::move(task);

  CreateWebContentsIfNecessary();

  url_loader_->LoadUrl(current_task_and_callback_->task->install_options().url,
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
  const auto& install_options =
      current_task_and_callback_->task->install_options();

  if (result == web_app::WebAppUrlLoader::Result::kUrlLoaded) {
    if (!UninstallPlaceholderIfNecessary(install_options)) {
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
    return;
  }

  base::Optional<std::string> extension_id =
      extension_ids_map_.LookupPlaceholderAppId(install_options.url);
  if (extension_id.has_value() &&
      registrar_->IsInstalled(extension_id.value())) {
    // No need to install a placeholder app again.
    CurrentInstallationFinished(extension_id.value());
    return;
  }

  // TODO(ortuno): Move this into BookmarkAppInstallationTask::Install() once
  // loading the URL is part of Install().
  if (install_options.install_placeholder) {
    current_task_and_callback_->task->InstallPlaceholder(
        base::BindOnce(&PendingBookmarkAppManager::OnInstalled,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  CurrentInstallationFinished(base::nullopt);
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
      .Run(task_and_callback->task->install_options().url, install_result_code);
}

}  // namespace extensions
