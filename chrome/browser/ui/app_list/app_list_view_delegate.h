// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_

#include <stdint.h>

#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/search/hotword_client.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/app_list/start_page_observer.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/app_list/app_list_view_delegate.h"

class AppListControllerDelegate;
class Profile;

namespace apps {
class CustomLauncherPageContents;
}

namespace app_list {
class LauncherPageEventDispatcher;
class SearchController;
class SearchResourceManager;
class SpeechUIModel;
}

namespace base {
class FilePath;
}

namespace content {
struct SpeechRecognitionSessionPreamble;
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
                            public ProfileAttributesStorage::Observer,
                            public SigninManagerBase::Observer,
                            public SigninManagerFactory::Observer,
                            public content::NotificationObserver,
                            public TemplateURLServiceObserver {
 public:
  // Constructs Chrome's AppListViewDelegate with a NULL Profile.
  // Does not take ownership of |controller|. TODO(tapted): It should.
  explicit AppListViewDelegate(AppListControllerDelegate* controller);
  ~AppListViewDelegate() override;

  // Configure the AppList for the given |profile|.
  void SetProfile(Profile* profile);
  Profile* profile() { return profile_; }

  // Invoked to start speech recognition based on a hotword trigger.
  void StartSpeechRecognitionForHotword(
      const scoped_refptr<content::SpeechRecognitionSessionPreamble>& preamble);

  // Overridden from app_list::AppListViewDelegate:
  bool ForceNativeDesktop() const override;
  void SetProfileByPath(const base::FilePath& profile_path) override;
  app_list::AppListModel* GetModel() override;
  app_list::SpeechUIModel* GetSpeechUI() override;
  void GetShortcutPathForApp(
      const std::string& app_id,
      const base::Callback<void(const base::FilePath&)>& callback) override;
  void StartSearch() override;
  void StopSearch() override;
  void OpenSearchResult(app_list::SearchResult* result,
                        bool auto_launch,
                        int event_flags) override;
  void InvokeSearchResultAction(app_list::SearchResult* result,
                                int action_index,
                                int event_flags) override;
  base::TimeDelta GetAutoLaunchTimeout() override;
  void AutoLaunchCanceled() override;
  void ViewInitialized() override;
  void Dismiss() override;
  void ViewClosing() override;
  gfx::ImageSkia GetWindowIcon() override;
  void OpenSettings() override;
  void OpenHelp() override;
  void OpenFeedback() override;
  void StartSpeechRecognition() override;
  void StopSpeechRecognition() override;
  void ShowForProfileByPath(const base::FilePath& profile_path) override;
#if defined(TOOLKIT_VIEWS)
  views::View* CreateStartPageWebView(const gfx::Size& size) override;
  std::vector<views::View*> CreateCustomPageWebViews(
      const gfx::Size& size) override;
  void CustomLauncherPageAnimationChanged(double progress) override;
  void CustomLauncherPagePopSubpage() override;
#endif
  bool IsSpeechRecognitionEnabled() override;
  const Users& GetUsers() const override;
  bool ShouldCenterWindow() const override;
  void AddObserver(app_list::AppListViewDelegateObserver* observer) override;
  void RemoveObserver(app_list::AppListViewDelegateObserver* observer) override;

  // Overridden from TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;

 private:
  // Updates the speech webview and start page for the current |profile_|.
  void SetUpSearchUI();

  // Updates the app list's ProfileMenuItems for the current |profile_|.
  void SetUpProfileSwitcher();

  // Updates the app list's custom launcher pages for the current |profile_|.
  void SetUpCustomLauncherPages();

  // Overridden from app_list::StartPageObserver:
  void OnSpeechResult(const base::string16& result, bool is_final) override;
  void OnSpeechSoundLevelChanged(int16_t level) override;
  void OnSpeechRecognitionStateChanged(
      app_list::SpeechRecognitionState new_state) override;

  // Overridden from HotwordClient:
  void OnHotwordStateChanged(bool started) override;
  void OnHotwordRecognized(
      const scoped_refptr<content::SpeechRecognitionSessionPreamble>& preamble)
      override;

  // Overridden from SigninManagerFactory::Observer:
  void SigninManagerCreated(SigninManagerBase* manager) override;
  void SigninManagerShutdown(SigninManagerBase* manager) override;

  // Overridden from SigninManagerBase::Observer:
  void GoogleSigninFailed(const GoogleServiceAuthError& error) override;
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username,
                             const std::string& password) override;
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

  // Overridden from ProfileAttributesStorage::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override;
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override;

  // Overridden from content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Unowned pointer to the controller.
  AppListControllerDelegate* controller_;
  // Unowned pointer to the associated profile. May change if SetProfileByPath
  // is called.
  Profile* profile_;
  // Unowned pointer to the model owned by AppListSyncableService. Will change
  // if |profile_| changes.
  app_list::AppListModel* model_;

  // Note: order ensures |search_resource_manager_| is destroyed before
  // |speech_ui_|.
  scoped_ptr<app_list::SpeechUIModel> speech_ui_;
  scoped_ptr<app_list::SearchResourceManager> search_resource_manager_;
  scoped_ptr<app_list::SearchController> search_controller_;

  scoped_ptr<app_list::LauncherPageEventDispatcher>
      launcher_page_event_dispatcher_;

  base::TimeDelta auto_launch_timeout_;
  // Determines whether the current search was initiated by speech.
  bool is_voice_query_;

  Users users_;

#if defined(USE_ASH)
  scoped_ptr<AppSyncUIStateWatcher> app_sync_ui_state_watcher_;
#endif

  base::ObserverList<app_list::AppListViewDelegateObserver> observers_;

  ScopedObserver<TemplateURLService, AppListViewDelegate>
      template_url_service_observer_;

  // Used to track the SigninManagers that this instance is observing so that
  // this instance can be removed as an observer on its destruction.
  ScopedObserver<SigninManagerBase, AppListViewDelegate> scoped_observer_;

  // Window contents of additional custom launcher pages.
  ScopedVector<apps::CustomLauncherPageContents> custom_page_contents_;

  // Registers for NOTIFICATION_APP_TERMINATING to unload custom launcher pages.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewDelegate);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
