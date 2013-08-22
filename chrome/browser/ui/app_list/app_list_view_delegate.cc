// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_view_delegate.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/feedback/feedback_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/apps_model_builder.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/chrome_signin_delegate.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/user_metrics.h"

#if defined(USE_ASH)
#include "chrome/browser/ui/ash/app_list/app_sync_ui_state_watcher.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/web_applications/web_app_win.h"
#endif

namespace {

#if defined(OS_WIN)
void CreateShortcutInWebAppDir(
    const base::FilePath& app_data_dir,
    base::Callback<void(const base::FilePath&)> callback,
    const ShellIntegration::ShortcutInfo& info) {
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(web_app::CreateShortcutInWebAppDir, app_data_dir, info),
      callback);
}
#endif

}  // namespace

AppListViewDelegate::AppListViewDelegate(AppListControllerDelegate* controller,
                                         Profile* profile)
    : controller_(controller),
      profile_(profile),
      model_(NULL) {
  DCHECK(profile_);
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                 content::Source<Profile>(profile_));
  g_browser_process->profile_manager()->GetProfileInfoCache().AddObserver(this);
}

AppListViewDelegate::~AppListViewDelegate() {
  g_browser_process->
      profile_manager()->GetProfileInfoCache().RemoveObserver(this);
}

void AppListViewDelegate::OnProfileChanged() {
  model_->SetSignedIn(!signin_delegate_->NeedSignin());
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  // Populate the current user details.
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_->GetPath());
  // The profile won't exist in the cache if the current app list profile is
  // being deleted.
  if (profile_index == std::string::npos)
    return;

  model_->SetCurrentUser(cache.GetNameOfProfileAtIndex(profile_index),
                         cache.GetUserNameOfProfileAtIndex(profile_index));
}

void AppListViewDelegate::SetModel(app_list::AppListModel* model) {
  if (model) {
    model_ = model;
    apps_builder_.reset(new AppsModelBuilder(profile_,
                                             model->apps(),
                                             controller_.get()));
    apps_builder_->Build();

    search_controller_.reset(new app_list::SearchController(
        profile_, model->search_box(), model->results(), controller_.get()));

    signin_delegate_.reset(new ChromeSigninDelegate(profile_));

#if defined(USE_ASH)
    app_sync_ui_state_watcher_.reset(new AppSyncUIStateWatcher(profile_,
                                                               model));
#endif
    OnProfileChanged();
  } else {
    model_ = NULL;
    apps_builder_.reset();
    search_controller_.reset();
    signin_delegate_.reset();
#if defined(USE_ASH)
    app_sync_ui_state_watcher_.reset();
#endif
  }
}

app_list::SigninDelegate* AppListViewDelegate::GetSigninDelegate() {
  return signin_delegate_.get();
}

void AppListViewDelegate::ActivateAppListItem(
    app_list::AppListItemModel* item,
    int event_flags) {
  content::RecordAction(content::UserMetricsAction("AppList_ClickOnApp"));
  static_cast<ChromeAppListItem*>(item)->Activate(event_flags);
}

void AppListViewDelegate::GetShortcutPathForApp(
    const std::string& app_id,
    const base::Callback<void(const base::FilePath&)>& callback) {
#if defined(OS_WIN)
  ExtensionService* service = profile_->GetExtensionService();
  DCHECK(service);
  const extensions::Extension* extension =
      service->GetInstalledExtension(app_id);
  if (!extension) {
    callback.Run(base::FilePath());
    return;
  }

  base::FilePath app_data_dir(
      web_app::GetWebAppDataDirectory(profile_->GetPath(),
                                      extension->id(),
                                      GURL()));

  web_app::UpdateShortcutInfoAndIconForApp(
      *extension,
      profile_,
      base::Bind(CreateShortcutInWebAppDir, app_data_dir, callback));
#else
  callback.Run(base::FilePath());
#endif
}

void AppListViewDelegate::StartSearch() {
  if (search_controller_)
    search_controller_->Start();
}

void AppListViewDelegate::StopSearch() {
  if (search_controller_)
    search_controller_->Stop();
}

void AppListViewDelegate::OpenSearchResult(
    app_list::SearchResult* result,
    int event_flags) {
  search_controller_->OpenResult(result, event_flags);
}

void AppListViewDelegate::InvokeSearchResultAction(
    app_list::SearchResult* result,
    int action_index,
    int event_flags) {
  search_controller_->InvokeResultAction(result, action_index, event_flags);
}

void AppListViewDelegate::Dismiss()  {
  controller_->DismissView();
}

void AppListViewDelegate::ViewClosing() {
  controller_->ViewClosing();
}

gfx::ImageSkia AppListViewDelegate::GetWindowIcon() {
  return controller_->GetWindowIcon();
}

void AppListViewDelegate::OpenSettings() {
  ExtensionService* service = profile_->GetExtensionService();
  DCHECK(service);
  const extensions::Extension* extension = service->GetInstalledExtension(
      extension_misc::kSettingsAppId);
  DCHECK(extension);
  controller_->ActivateApp(profile_, extension, 0);
}

void AppListViewDelegate::OpenHelp() {
  chrome::HostDesktopType desktop = chrome::GetHostDesktopTypeForNativeWindow(
      controller_->GetAppListWindow());
  Browser* browser = chrome::FindOrCreateTabbedBrowser(
      profile_, desktop);
  browser->OpenURL(content::OpenURLParams(GURL(chrome::kAppLauncherHelpURL),
                                          content::Referrer(),
                                          NEW_FOREGROUND_TAB,
                                          content::PAGE_TRANSITION_LINK,
                                          false));
}

void AppListViewDelegate::OpenFeedback() {
  chrome::HostDesktopType desktop = chrome::GetHostDesktopTypeForNativeWindow(
      controller_->GetAppListWindow());
  Browser* browser = chrome::FindOrCreateTabbedBrowser(
      profile_, desktop);
  chrome::ShowFeedbackPage(browser, std::string(),
                           chrome::kAppLauncherCategoryTag);
}

void AppListViewDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  OnProfileChanged();
}

void AppListViewDelegate::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  if (profile_->GetPath() != profile_path)
    return;

  OnProfileChanged();
}
