// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_MD_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_MD_SETTINGS_UI_H_

#include <unordered_set>

#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"

#if defined(OS_WIN)
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_state_change_observer_win.h"
#endif

class GURL;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace settings {

class SettingsPageUIHandler;

// Exposed for testing.
bool IsValidOrigin(const GURL& url);

// The WebUI handler for chrome://md-settings.
class MdSettingsUI : public content::WebUIController,
                     public content::WebContentsObserver {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  MdSettingsUI(content::WebUI* web_ui, const GURL& url);
  ~MdSettingsUI() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentLoadedInFrame(
      content::RenderFrameHost *render_frame_host) override;
  void DocumentOnLoadCompletedInMainFrame() override;

 private:
  void AddSettingsPageUIHandler(std::unique_ptr<SettingsPageUIHandler> handler);

  // Weak references; all |handlers_| are owned by |web_ui()|.
  std::unordered_set<SettingsPageUIHandler*> handlers_;

  base::Time load_start_time_;

#if defined(OS_WIN)
  void UpdateCleanupDataSource(bool cleanupEnabled, bool partnerPowered);
  std::unique_ptr<safe_browsing::ChromeCleanerStateChangeObserver>
      cleanup_observer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(MdSettingsUI);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_MD_SETTINGS_UI_H_
