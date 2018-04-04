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
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/app_list_view_delegate_observer.h"

class AppListClientImpl;

namespace app_list {
class SearchController;
class SearchResourceManager;
}

namespace content {
struct SpeechRecognitionSessionPreamble;
}

class AppListControllerDelegate;
class AppSyncUIStateWatcher;
class Profile;

class AppListViewDelegate : public app_list::AppListViewDelegate,
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

  // Gets the model updater.
  AppListModelUpdater* GetModelUpdater();

  // Overridden from app_list::AppListViewDelegate:
  app_list::AppListModel* GetModel() override;
  app_list::SearchModel* GetSearchModel() override;
  void StartSearch(const base::string16& raw_query) override;
  void OpenSearchResult(const std::string& result_id, int event_flags) override;
  void InvokeSearchResultAction(const std::string& result_id,
                                int action_index,
                                int event_flags) override;
  void ViewShown(int64_t display_id) override;
  void Dismiss() override;
  void ViewClosing() override;
  void GetWallpaperProminentColors(
      GetWallpaperProminentColorsCallback callback) override;
  void ActivateItem(const std::string& id, int event_flags) override;
  void GetContextMenuModel(const std::string& id,
                           GetContextMenuModelCallback callback) override;
  void ContextMenuItemSelected(const std::string& id,
                               int command_id,
                               int event_flags) override;
  void AddObserver(app_list::AppListViewDelegateObserver* observer) override;
  void RemoveObserver(app_list::AppListViewDelegateObserver* observer) override;

  // Overridden from TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;

 private:
  // TODO(hejq): We'll merge AppListClientImpl and AppListViewDelegate, but not
  //             now, since that'll change all interface calls.
  friend AppListClientImpl;

  // Callback for ash::mojom::GetWallpaperColors.
  void OnGetWallpaperColorsCallback(const std::vector<SkColor>& colors);

  // Updates the speech webview and start page for the current |profile_|.
  void SetUpSearchUI();

  // Overridden from ash::mojom::WallpaperObserver:
  void OnWallpaperChanged(uint32_t image_id) override;
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
  // Unowned pointer to the model updater owned by AppListSyncableService.
  // Will change if |profile_| changes.
  AppListModelUpdater* model_updater_;

  std::unique_ptr<app_list::SearchResourceManager> search_resource_manager_;
  std::unique_ptr<app_list::SearchController> search_controller_;

  std::unique_ptr<AppSyncUIStateWatcher> app_sync_ui_state_watcher_;

  ScopedObserver<TemplateURLService, AppListViewDelegate>
      template_url_service_observer_;

  // Registers for NOTIFICATION_APP_TERMINATING to unload custom launcher pages.
  content::NotificationRegistrar registrar_;

  // The binding this instance uses to implement mojom::WallpaperObserver.
  mojo::AssociatedBinding<ash::mojom::WallpaperObserver> observer_binding_;

  std::vector<SkColor> wallpaper_prominent_colors_;

  base::ObserverList<app_list::AppListViewDelegateObserver> observers_;

  base::WeakPtrFactory<AppListViewDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewDelegate);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
