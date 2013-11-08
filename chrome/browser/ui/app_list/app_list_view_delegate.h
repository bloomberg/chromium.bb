// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/ui/app_list/chrome_signin_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/app_list/app_list_view_delegate.h"

class AppListControllerDelegate;
class ExtensionAppModelBuilder;
class Profile;

namespace app_list {
class SearchController;
}

namespace base {
class FilePath;
}

namespace content {
class NotificationDetails;
class NotificationSource;
}

namespace gfx {
class ImageSkia;
}

#if defined(USE_ASH)
class AppSyncUIStateWatcher;
#endif

class AppListViewDelegate : public app_list::AppListViewDelegate,
                            public content::NotificationObserver,
                            public ProfileInfoCacheObserver {
 public:
  // The delegate will take ownership of the controller.
  AppListViewDelegate(scoped_ptr<AppListControllerDelegate> controller,
                      Profile* profile);
  virtual ~AppListViewDelegate();

 private:
  // Registers the current profile for notifications.
  void RegisterForNotifications();
  // Updates the app list's current profile and ProfileMenuItems.
  void OnProfileChanged();

  // Overridden from app_list::AppListViewDelegate:
  virtual bool ForceNativeDesktop() const OVERRIDE;
  virtual void SetProfileByPath(const base::FilePath& profile_path) OVERRIDE;
  virtual void InitModel(app_list::AppListModel* model) OVERRIDE;
  virtual app_list::SigninDelegate* GetSigninDelegate() OVERRIDE;
  virtual void GetShortcutPathForApp(
      const std::string& app_id,
      const base::Callback<void(const base::FilePath&)>& callback) OVERRIDE;
  virtual void StartSearch() OVERRIDE;
  virtual void StopSearch() OVERRIDE;
  virtual void OpenSearchResult(app_list::SearchResult* result,
                                int event_flags) OVERRIDE;
  virtual void InvokeSearchResultAction(app_list::SearchResult* result,
                                        int action_index,
                                        int event_flags) OVERRIDE;
  virtual void Dismiss() OVERRIDE;
  virtual void ViewClosing() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual void OpenSettings() OVERRIDE;
  virtual void OpenHelp() OVERRIDE;
  virtual void OpenFeedback() OVERRIDE;
  virtual void ShowForProfileByPath(
      const base::FilePath& profile_path) OVERRIDE;
  virtual content::WebContents* GetStartPageContents() OVERRIDE;
  virtual const Users& GetUsers() const OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from ProfileInfoCacheObserver:
  virtual void OnProfileAdded(const base::FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWasRemoved(const base::FilePath& profile_path,
                                   const base::string16& profile_name) OVERRIDE;
  virtual void OnProfileNameChanged(
      const base::FilePath& profile_path,
      const base::string16& old_profile_name) OVERRIDE;

  scoped_ptr<ExtensionAppModelBuilder> apps_builder_;
  scoped_ptr<app_list::SearchController> search_controller_;
  scoped_ptr<AppListControllerDelegate> controller_;
  Profile* profile_;
  Users users_;
  app_list::AppListModel* model_;  // Weak. Owned by AppListView.

  content::NotificationRegistrar registrar_;
  ChromeSigninDelegate signin_delegate_;
#if defined(USE_ASH)
  scoped_ptr<AppSyncUIStateWatcher> app_sync_ui_state_watcher_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AppListViewDelegate);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
