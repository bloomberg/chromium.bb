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
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/search/hotword_client.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/app_list/start_page_observer.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "ui/app_list/app_list_view_delegate.h"

class AppListControllerDelegate;
class Profile;

namespace apps {
class CustomLauncherPageContents;
}

namespace app_list {
class SearchController;
class SpeechUIModel;
}

namespace base {
class FilePath;
}

namespace gfx {
class ImageSkia;
}

#if defined(USE_ASH)
class AppSyncUIStateWatcher;
#endif

class AppListViewDelegate : public app_list::AppListViewDelegate,
                            public app_list::StartPageObserver,
                            public HotwordClient,
                            public ProfileInfoCacheObserver,
                            public SigninManagerBase::Observer,
                            public SigninManagerFactory::Observer {
 public:
  AppListViewDelegate(Profile* profile,
                      AppListControllerDelegate* controller);
  virtual ~AppListViewDelegate();

 private:
  // Updates the app list's current profile and ProfileMenuItems.
  void OnProfileChanged();

  // Overridden from app_list::AppListViewDelegate:
  virtual bool ForceNativeDesktop() const OVERRIDE;
  virtual void SetProfileByPath(const base::FilePath& profile_path) OVERRIDE;
  virtual app_list::AppListModel* GetModel() OVERRIDE;
  virtual app_list::SpeechUIModel* GetSpeechUI() OVERRIDE;
  virtual void GetShortcutPathForApp(
      const std::string& app_id,
      const base::Callback<void(const base::FilePath&)>& callback) OVERRIDE;
  virtual void StartSearch() OVERRIDE;
  virtual void StopSearch() OVERRIDE;
  virtual void OpenSearchResult(app_list::SearchResult* result,
                                bool auto_launch,
                                int event_flags) OVERRIDE;
  virtual void InvokeSearchResultAction(app_list::SearchResult* result,
                                        int action_index,
                                        int event_flags) OVERRIDE;
  virtual base::TimeDelta GetAutoLaunchTimeout() OVERRIDE;
  virtual void AutoLaunchCanceled() OVERRIDE;
  virtual void ViewInitialized() OVERRIDE;
  virtual void Dismiss() OVERRIDE;
  virtual void ViewClosing() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual void OpenSettings() OVERRIDE;
  virtual void OpenHelp() OVERRIDE;
  virtual void OpenFeedback() OVERRIDE;
  virtual void ToggleSpeechRecognition() OVERRIDE;
  virtual void ShowForProfileByPath(
      const base::FilePath& profile_path) OVERRIDE;
#if defined(TOOLKIT_VIEWS)
  virtual views::View* CreateStartPageWebView(const gfx::Size& size) OVERRIDE;
  virtual views::View* CreateCustomPageWebView(const gfx::Size& size) OVERRIDE;
#endif
  virtual bool IsSpeechRecognitionEnabled() OVERRIDE;
  virtual const Users& GetUsers() const OVERRIDE;
  virtual bool ShouldCenterWindow() const OVERRIDE;
  virtual void AddObserver(
      app_list::AppListViewDelegateObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      app_list::AppListViewDelegateObserver* observer) OVERRIDE;

  // Overridden from app_list::StartPageObserver:
  virtual void OnSpeechResult(const base::string16& result,
                              bool is_final) OVERRIDE;
  virtual void OnSpeechSoundLevelChanged(int16 level) OVERRIDE;
  virtual void OnSpeechRecognitionStateChanged(
      app_list::SpeechRecognitionState new_state) OVERRIDE;

  // Overridden from HotwordClient:
  virtual void OnHotwordStateChanged(bool started) OVERRIDE;
  virtual void OnHotwordRecognized() OVERRIDE;

  // Overridden from SigninManagerFactory::Observer:
  virtual void SigninManagerCreated(SigninManagerBase* manager) OVERRIDE;
  virtual void SigninManagerShutdown(SigninManagerBase* manager) OVERRIDE;

  // Overridden from SigninManagerBase::Observer:
  virtual void GoogleSigninFailed(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void GoogleSigninSucceeded(const std::string& username,
                                     const std::string& password) OVERRIDE;
  virtual void GoogleSignedOut(const std::string& username) OVERRIDE;

  // Overridden from ProfileInfoCacheObserver:
  virtual void OnProfileAdded(const base::FilePath& profile_path) OVERRIDE;
  virtual void OnProfileWasRemoved(const base::FilePath& profile_path,
                                   const base::string16& profile_name) OVERRIDE;
  virtual void OnProfileNameChanged(
      const base::FilePath& profile_path,
      const base::string16& old_profile_name) OVERRIDE;

  scoped_ptr<app_list::SearchController> search_controller_;
  // Unowned pointer to the controller.
  AppListControllerDelegate* controller_;
  // Unowned pointer to the associated profile. May change if SetProfileByPath
  // is called.
  Profile* profile_;
  // Unowned pointer to the model owned by AppListSyncableService. Will change
  // if |profile_| changes.
  app_list::AppListModel* model_;

  scoped_ptr<app_list::SpeechUIModel> speech_ui_;

  base::TimeDelta auto_launch_timeout_;

  Users users_;

#if defined(USE_ASH)
  scoped_ptr<AppSyncUIStateWatcher> app_sync_ui_state_watcher_;
#endif

  ObserverList<app_list::AppListViewDelegateObserver> observers_;

  // Used to track the SigninManagers that this instance is observing so that
  // this instance can be removed as an observer on its destruction.
  ScopedObserver<SigninManagerBase, AppListViewDelegate> scoped_observer_;

  // Window contents of the additional custom launcher page. May be NULL.
  scoped_ptr<apps::CustomLauncherPageContents> custom_page_contents_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewDelegate);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
