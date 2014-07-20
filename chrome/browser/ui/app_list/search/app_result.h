// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_RESULT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_registry_observer.h"

class AppListControllerDelegate;
class ExtensionEnableFlow;
class Profile;

namespace extensions {
class ExtensionRegistry;
}

namespace app_list {

class AppContextMenu;
class TokenizedString;
class TokenizedStringMatch;

class AppResult : public ChromeSearchResult,
                  public extensions::IconImage::Observer,
                  public AppContextMenuDelegate,
                  public ExtensionEnableFlowDelegate,
                  public extensions::ExtensionRegistryObserver {
 public:
  AppResult(Profile* profile,
            const std::string& app_id,
            AppListControllerDelegate* controller);
  virtual ~AppResult();

  void UpdateFromMatch(const TokenizedString& title,
                       const TokenizedStringMatch& match);

  // ChromeSearchResult overides:
  virtual void Open(int event_flags) OVERRIDE;
  virtual void InvokeAction(int action_index, int event_flags) OVERRIDE;
  virtual scoped_ptr<ChromeSearchResult> Duplicate() OVERRIDE;
  virtual ui::MenuModel* GetContextMenuModel() OVERRIDE;
  virtual ChromeSearchResultType GetType() OVERRIDE;

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
  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* image) OVERRIDE;

  // AppContextMenuDelegate overrides:
  virtual void ExecuteLaunchCommand(int event_flags) OVERRIDE;

  // ExtensionEnableFlowDelegate overrides:
  virtual void ExtensionEnableFlowFinished() OVERRIDE;
  virtual void ExtensionEnableFlowAborted(bool user_initiated) OVERRIDE;

  // extensions::ExtensionRegistryObserver override:
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;
  virtual void OnShutdown(extensions::ExtensionRegistry* registry) OVERRIDE;

  Profile* profile_;
  const std::string app_id_;
  AppListControllerDelegate* controller_;

  bool is_platform_app_;
  scoped_ptr<extensions::IconImage> icon_;
  scoped_ptr<AppContextMenu> context_menu_;
  scoped_ptr<ExtensionEnableFlow> extension_enable_flow_;

  extensions::ExtensionRegistry* extension_registry_;

  DISALLOW_COPY_AND_ASSIGN(AppResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_RESULT_H_
