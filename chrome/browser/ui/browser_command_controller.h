// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_COMMAND_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_COMMAND_CONTROLLER_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/command_updater_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class BrowserWindow;
class Profile;

namespace content {
struct NativeWebKeyboardEvent;
}

namespace chrome {

class BrowserCommandController : public CommandUpdaterDelegate,
                                 public TabStripModelObserver,
                                 public sessions::TabRestoreServiceObserver {
 public:
  explicit BrowserCommandController(Browser* browser);
  ~BrowserCommandController() override;

  CommandUpdater* command_updater() { return &command_updater_; }

  // Returns true if |command_id| is a reserved command whose keyboard shortcuts
  // should not be sent to the renderer or |event| was triggered by a key that
  // we never want to send to the renderer.
  bool IsReservedCommandOrKey(int command_id,
                              const content::NativeWebKeyboardEvent& event);

  // Notifies the controller that state has changed in one of the following
  // areas and it should update command states.
  void TabStateChanged();
  void ZoomStateChanged();
  void ContentRestrictionsChanged();
  void FullscreenStateChanged();
  void PrintingStateChanged();
  void LoadingStateChanged(bool is_loading, bool force);
  void ExtensionStateChanged();

  // Shared state updating: these functions are static and public to share with
  // outside code.

  // Updates the open-file state.
  static void UpdateOpenFileState(CommandUpdater* command_updater);

  // Update commands whose state depends on incognito mode availability and that
  // only depend on the profile.
  static void UpdateSharedCommandsForIncognitoAvailability(
      CommandUpdater* command_updater,
      Profile* profile);

 private:
  class InterstitialObserver;

  // Overridden from CommandUpdaterDelegate:
  void ExecuteCommandWithDisposition(int id, WindowOpenDisposition disposition)
      override;

  // Overridden from TabStripModelObserver:
  void TabInsertedAt(TabStripModel* tab_strip_model,
                     content::WebContents* contents,
                     int index,
                     bool foreground) override;
  void TabDetachedAt(content::WebContents* contents, int index) override;
  void TabReplacedAt(TabStripModel* tab_strip_model,
                     content::WebContents* old_contents,
                     content::WebContents* new_contents,
                     int index) override;
  void TabBlockedStateChanged(content::WebContents* contents,
                              int index) override;

  // Overridden from TabRestoreServiceObserver:
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;
  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override;

  // Returns true if the regular Chrome UI (not the fullscreen one and
  // not the single-tab one) is shown. Used for updating window command states
  // only. Consider using SupportsWindowFeature if you need the mentioned
  // functionality anywhere else.
  bool IsShowingMainUI();

  // Initialize state for all browser commands.
  void InitCommandState();

  // Update commands whose state depends on incognito mode availability.
  void UpdateCommandsForIncognitoAvailability();

  // Update commands whose state depends on the tab's state.
  void UpdateCommandsForTabState();

  // Update Zoom commands based on zoom state.
  void UpdateCommandsForZoomState();

  // Updates commands when the content's restrictions change.
  void UpdateCommandsForContentRestrictionState();

  // Updates commands for enabling developer tools.
  void UpdateCommandsForDevTools();

  // Updates commands for bookmark editing.
  void UpdateCommandsForBookmarkEditing();

  // Updates commands that affect the bookmark bar.
  void UpdateCommandsForBookmarkBar();

  // Updates commands that affect file selection dialogs in aggregate,
  // namely the save-page-as state and the open-file state.
  void UpdateCommandsForFileSelectionDialogs();

  // Update commands whose state depends on the type of fullscreen mode the
  // window is in.
  void UpdateCommandsForFullscreenMode();

  // Updates the printing command state.
  void UpdatePrintingState();

  // Updates the SHOW_SYNC_SETUP menu entry.
  void OnSigninAllowedPrefChange();

  // Updates the save-page-as command state.
  void UpdateSaveAsState();

  // Updates the show-sync command state.
  void UpdateShowSyncState(bool show_main_ui);

  // Ask the Reload/Stop button to change its icon, and update the Stop command
  // state.  |is_loading| is true if the current WebContents is loading.
  // |force| is true if the button should change its icon immediately.
  void UpdateReloadStopState(bool is_loading, bool force);

  void UpdateTabRestoreCommandState();

  // Updates commands for find.
  void UpdateCommandsForFind();

  // Updates commands for Media Router.
  void UpdateCommandsForMediaRouter();

  // Add/remove observers for interstitial attachment/detachment from
  // |contents|.
  void AddInterstitialObservers(content::WebContents* contents);
  void RemoveInterstitialObservers(content::WebContents* contents);

  inline BrowserWindow* window();
  inline Profile* profile();

  Browser* browser_;

  // The CommandUpdater that manages the browser window commands.
  CommandUpdater command_updater_;

  std::vector<InterstitialObserver*> interstitial_observers_;

  PrefChangeRegistrar profile_pref_registrar_;
  PrefChangeRegistrar local_pref_registrar_;
  BooleanPrefMember pref_signin_allowed_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCommandController);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_COMMAND_CONTROLLER_H_
