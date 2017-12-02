// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/public/interfaces/wallpaper.mojom.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/app_list/start_page_observer.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/app_list_view_delegate_observer.h"
#include "ui/app_list/views/app_list_view.h"

namespace app_list {
class CustomLauncherPageContents;
class LauncherPageEventDispatcher;
class SearchController;
class SearchResourceManager;
class SpeechUIModel;
}

namespace content {
struct SpeechRecognitionSessionPreamble;
}

class AppListControllerDelegate;
class AppSyncUIStateWatcher;
class Profile;

class AppListViewDelegate : public app_list::AppListViewDelegate,
                            public app_list::StartPageObserver,
                            public ash::mojom::WallpaperObserver,
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
  app_list::AppListModel* GetModel() override;
  app_list::SearchModel* GetSearchModel() override;
  app_list::SpeechUIModel* GetSpeechUI() override;
  void StartSearch() override;
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
  void StartSpeechRecognition() override;
  void StopSpeechRecognition() override;
  views::View* CreateStartPageWebView(const gfx::Size& size) override;
  std::vector<views::View*> CreateCustomPageWebViews(
      const gfx::Size& size) override;
  void CustomLauncherPageAnimationChanged(double progress) override;
  void CustomLauncherPagePopSubpage() override;
  bool IsSpeechRecognitionEnabled() override;
  void GetWallpaperProminentColors(std::vector<SkColor>* colors) override;
  void AddObserver(app_list::AppListViewDelegateObserver* observer) override;
  void RemoveObserver(app_list::AppListViewDelegateObserver* observer) override;

  // Overridden from TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;

 private:
  // Callback for ash::mojom::GetWallpaperColors.
  void OnGetWallpaperColorsCallback(const std::vector<SkColor>& colors);

  // Updates the speech webview and start page for the current |profile_|.
  void SetUpSearchUI();

  // Overridden from app_list::StartPageObserver:
  void OnSpeechResult(const base::string16& result, bool is_final) override;
  void OnSpeechSoundLevelChanged(int16_t level) override;
  void OnSpeechRecognitionStateChanged(
      app_list::SpeechRecognitionState new_state) override;

  // Overridden from ash::mojom::WallpaperObserver:
  void OnWallpaperColorsChanged(
      const std::vector<SkColor>& prominent_colors) override;

  // Overridden from content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Unowned pointer to the controller.
  AppListControllerDelegate* controller_;
  // Unowned pointer to the associated profile. May change if SetProfileByPath
  // is called.
  Profile* profile_;
  // Unowned pointer to the models owned by AppListSyncableService. Will change
  // if |profile_| changes.
  app_list::AppListModel* model_;
  app_list::SearchModel* search_model_;

  // Note: order ensures |search_resource_manager_| is destroyed before
  // |speech_ui_|.
  std::unique_ptr<app_list::SpeechUIModel> speech_ui_;
  std::unique_ptr<app_list::SearchResourceManager> search_resource_manager_;
  std::unique_ptr<app_list::SearchController> search_controller_;

  std::unique_ptr<app_list::LauncherPageEventDispatcher>
      launcher_page_event_dispatcher_;

  base::TimeDelta auto_launch_timeout_;

  std::unique_ptr<AppSyncUIStateWatcher> app_sync_ui_state_watcher_;

  ScopedObserver<TemplateURLService, AppListViewDelegate>
      template_url_service_observer_;

  // Window contents of additional custom launcher pages.
  std::vector<std::unique_ptr<app_list::CustomLauncherPageContents>>
      custom_page_contents_;

  // Registers for NOTIFICATION_APP_TERMINATING to unload custom launcher pages.
  content::NotificationRegistrar registrar_;

  // The binding this instance uses to implement mojom::WallpaperObserver.
  mojo::AssociatedBinding<ash::mojom::WallpaperObserver> observer_binding_;

  // Ash's mojom::WallpaperController.
  ash::mojom::WallpaperControllerPtr wallpaper_controller_ptr_;

  std::vector<SkColor> wallpaper_prominent_colors_;

  base::ObserverList<app_list::AppListViewDelegateObserver> observers_;

  base::WeakPtrFactory<AppListViewDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewDelegate);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
