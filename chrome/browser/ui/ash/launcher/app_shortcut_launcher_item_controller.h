// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SHORTCUT_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SHORTCUT_LAUNCHER_ITEM_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "url/gurl.h"

class Browser;
class URLPattern;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

class ChromeLauncherController;

// Item controller for an app shortcut. Shortcuts track app and launcher ids,
// but do not have any associated windows (opening a shortcut will replace the
// item with the appropriate LauncherItemController type).
class AppShortcutLauncherItemController : public LauncherItemController {
 public:
  ~AppShortcutLauncherItemController() override;

  static AppShortcutLauncherItemController* Create(
      const std::string& app_id,
      const std::string& launch_id,
      ChromeLauncherController* controller);

  std::vector<content::WebContents*> GetRunningApplications();

  // LauncherItemController overrides:
  bool IsVisible() const override;
  void Launch(ash::LaunchSource source, int event_flags) override;
  ash::ShelfItemDelegate::PerformedAction Activate(
      ash::LaunchSource source) override;
  ChromeLauncherAppMenuItems GetApplicationList(int event_flags) override;
  ash::ShelfItemDelegate::PerformedAction ItemSelected(
      const ui::Event& event) override;
  base::string16 GetTitle() override;
  ash::ShelfMenuModel* CreateApplicationMenu(int event_flags) override;
  bool IsDraggable() override;
  bool CanPin() const override;
  bool ShouldShowTooltip() override;
  void Close() override;

  // Get the refocus url pattern, which can be used to identify this application
  // from a URL link.
  const GURL& refocus_url() const { return refocus_url_; }
  // Set the refocus url pattern. Used by unit tests.
  void set_refocus_url(const GURL& refocus_url) { refocus_url_ = refocus_url; }

  ChromeLauncherController* controller() { return chrome_launcher_controller_; }

 protected:
  AppShortcutLauncherItemController(const std::string& app_id,
                                    const std::string& launch_id,
                                    ChromeLauncherController* controller);

 private:
  // Get the last running application.
  content::WebContents* GetLRUApplication();

  // Returns true if this app matches the given |web_contents|. To accelerate
  // the matching, the app managing |extension| as well as the parsed
  // |refocus_pattern| get passed. If |is_app| is true, the application gets
  // first checked against its original URL since a windowed app might have
  // navigated away from its app domain.
  bool WebContentMatchesApp(const extensions::Extension* extension,
                            const URLPattern& refocus_pattern,
                            content::WebContents* web_contents,
                            Browser* browser);

  // Activate the browser with the given |content| and show the associated tab.
  // Returns the action performed by activating the content.
  ash::ShelfItemDelegate::PerformedAction ActivateContent(
      content::WebContents* content);

  // Advance to the next item if an owned item is already active. The function
  // will return true if it has successfully advanced.
  bool AdvanceToNextApp();

  // Returns true if the application is a V2 app.
  bool IsV2App();

  // Returns true if it is allowed to try starting a V2 app again.
  bool AllowNextLaunchAttempt();

  GURL refocus_url_;

  // Since V2 applications can be undetectable after launching, this timer is
  // keeping track of the last launch attempt.
  base::Time last_launch_attempt_;

  ChromeLauncherController* chrome_launcher_controller_;

  DISALLOW_COPY_AND_ASSIGN(AppShortcutLauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SHORTCUT_LAUNCHER_ITEM_CONTROLLER_H_
