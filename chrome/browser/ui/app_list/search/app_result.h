// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_RESULT_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/app_list/search_result.h"

class AppListControllerDelegate;
class ExtensionEnableFlow;
class Profile;

namespace base {
class Time;
}

namespace extensions {
class ExtensionRegistry;
}

namespace app_list {

class ExtensionAppContextMenu;

class AppResult : public SearchResult,
                  public extensions::IconImage::Observer,
                  public AppContextMenuDelegate,
                  public ExtensionEnableFlowDelegate,
                  public extensions::ExtensionRegistryObserver {
 public:
  AppResult(Profile* profile,
            const std::string& app_id,
            AppListControllerDelegate* controller,
            bool is_recommendation);
  ~AppResult() override;

  void UpdateFromLastLaunched(const base::Time& current_time,
                              const base::Time& last_launched);

  // SearchResult overrides:
  void Open(int event_flags) override;
  scoped_ptr<SearchResult> Duplicate() const override;
  ui::MenuModel* GetContextMenuModel() override;

 private:
  void StartObservingExtensionRegistry();
  void StopObservingExtensionRegistry();

  // Checks if extension is disabled and if enable flow should be started.
  // Returns true if extension enable flow is started or there is already one
  // running.
  bool RunExtensionEnableFlow();

  // Updates the app item's icon, if necessary making it gray.
  void UpdateIcon();

  // extensions::IconImage::Observer overrides:
  void OnExtensionIconImageChanged(extensions::IconImage* image) override;

  // AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override;

  // ExtensionEnableFlowDelegate overrides:
  void ExtensionEnableFlowFinished() override;
  void ExtensionEnableFlowAborted(bool user_initiated) override;

  // extensions::ExtensionRegistryObserver override:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  Profile* profile_;
  const std::string app_id_;
  AppListControllerDelegate* controller_;

  bool is_platform_app_;
  scoped_ptr<extensions::IconImage> icon_;
  scoped_ptr<ExtensionAppContextMenu> context_menu_;
  scoped_ptr<ExtensionEnableFlow> extension_enable_flow_;

  extensions::ExtensionRegistry* extension_registry_;

  DISALLOW_COPY_AND_ASSIGN(AppResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_RESULT_H_
