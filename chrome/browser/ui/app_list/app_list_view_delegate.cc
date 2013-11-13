// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_view_delegate.h"

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/feedback/feedback_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/extension_app_model_builder.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/start_page_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/user_metrics.h"
#include "ui/app_list/search_box_model.h"

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

void PopulateUsers(const ProfileInfoCache& profile_info,
                   const base::FilePath& active_profile_path,
                   app_list::AppListViewDelegate::Users* users) {
  const size_t count = profile_info.GetNumberOfProfiles();
  for (size_t i = 0; i < count; ++i) {
    // Don't display managed users.
    if (profile_info.ProfileIsManagedAtIndex(i))
      continue;

    app_list::AppListViewDelegate::User user;
    user.name = profile_info.GetNameOfProfileAtIndex(i);
    user.email = profile_info.GetUserNameOfProfileAtIndex(i);
    user.profile_path = profile_info.GetPathOfProfileAtIndex(i);
    user.active = active_profile_path == user.profile_path;
    users->push_back(user);
  }
}

}  // namespace

AppListViewDelegate::AppListViewDelegate(
    scoped_ptr<AppListControllerDelegate> controller,
    Profile* profile)
    : controller_(controller.Pass()),
      profile_(profile),
      model_(NULL) {
  CHECK(controller_);
  RegisterForNotifications();
  g_browser_process->profile_manager()->GetProfileInfoCache().AddObserver(this);
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (service)
    service->AddObserver(this);
}

AppListViewDelegate::~AppListViewDelegate() {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (service)
    service->RemoveObserver(this);
  g_browser_process->
      profile_manager()->GetProfileInfoCache().RemoveObserver(this);
}

void AppListViewDelegate::RegisterForNotifications() {
  registrar_.RemoveAll();
  DCHECK(profile_);

  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                 content::NotificationService::AllSources());
}

void AppListViewDelegate::OnProfileChanged() {
  CHECK(controller_);
  search_controller_.reset(new app_list::SearchController(
      profile_, model_->search_box(), model_->results(), controller_.get()));

  signin_delegate_.SetProfile(profile_);

#if defined(USE_ASH)
  app_sync_ui_state_watcher_.reset(new AppSyncUIStateWatcher(profile_, model_));
#endif

  model_->SetSignedIn(!GetSigninDelegate()->NeedSignin());

  // Don't populate the app list users if we are on the ash desktop.
  chrome::HostDesktopType desktop = chrome::GetHostDesktopTypeForNativeWindow(
      controller_->GetAppListWindow());
  if (desktop == chrome::HOST_DESKTOP_TYPE_ASH)
    return;

  // Populate the app list users.
  PopulateUsers(g_browser_process->profile_manager()->GetProfileInfoCache(),
                profile_->GetPath(), &users_);
}

bool AppListViewDelegate::ForceNativeDesktop() const {
  return controller_->ForceNativeDesktop();
}

void AppListViewDelegate::SetProfileByPath(const base::FilePath& profile_path) {
  DCHECK(model_);

  // The profile must be loaded before this is called.
  profile_ =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
  DCHECK(profile_);

  RegisterForNotifications();

  apps_builder_->SwitchProfile(profile_);

  OnProfileChanged();

  // Clear search query.
  model_->search_box()->SetText(base::string16());
}

void AppListViewDelegate::InitModel(app_list::AppListModel* model) {
  DCHECK(!model_);
  DCHECK(model);
  model_ = model;

  // Initialize apps model.
  apps_builder_.reset(new ExtensionAppModelBuilder(profile_,
                                                   model,
                                                   controller_.get()));

  // Initialize the profile information in the app list menu.
  OnProfileChanged();
}

app_list::SigninDelegate* AppListViewDelegate::GetSigninDelegate() {
  return &signin_delegate_;
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
  controller_->ActivateApp(profile_,
                           extension,
                           AppListControllerDelegate::LAUNCH_FROM_UNKNOWN,
                           0);
}

void AppListViewDelegate::OpenHelp() {
  chrome::HostDesktopType desktop = chrome::GetHostDesktopTypeForNativeWindow(
      controller_->GetAppListWindow());
  chrome::ScopedTabbedBrowserDisplayer displayer(profile_, desktop);
  content::OpenURLParams params(GURL(chrome::kAppLauncherHelpURL),
                                content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                content::PAGE_TRANSITION_LINK,
                                false);
  displayer.browser()->OpenURL(params);
}

void AppListViewDelegate::OpenFeedback() {
  chrome::HostDesktopType desktop = chrome::GetHostDesktopTypeForNativeWindow(
      controller_->GetAppListWindow());
  Browser* browser = chrome::FindTabbedBrowser(profile_, false, desktop);
  chrome::ShowFeedbackPage(browser, std::string(),
                           chrome::kAppLauncherCategoryTag);
}

void AppListViewDelegate::ToggleSpeechRecognition() {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (service)
    service->ToggleSpeechRecognition();
}

void AppListViewDelegate::ShowForProfileByPath(
    const base::FilePath& profile_path) {
  controller_->ShowForProfileByPath(profile_path);
}

void AppListViewDelegate::OnSearch(const base::string16& query) {
  model_->search_box()->SetText(query);
}

void AppListViewDelegate::OnSpeechRecognitionStateChanged(bool recognizing) {
  model_->search_box()->SetSpeechRecognitionButtonState(recognizing);
}

void AppListViewDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  OnProfileChanged();
}

void AppListViewDelegate::OnProfileAdded(const base::FilePath& profile_path) {
  OnProfileChanged();
}

void AppListViewDelegate::OnProfileWasRemoved(
    const base::FilePath& profile_path, const base::string16& profile_name) {
  OnProfileChanged();
}

void AppListViewDelegate::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  OnProfileChanged();
}

content::WebContents* AppListViewDelegate::GetStartPageContents() {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (!service)
    return NULL;

  return service->contents();
}

const app_list::AppListViewDelegate::Users&
AppListViewDelegate::GetUsers() const {
  return users_;
}
