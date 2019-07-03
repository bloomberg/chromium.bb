// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_UI_H_
#define CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_UI_H_

#include <map>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/desktop_media_id.h"

namespace content {
class WebContents;
}
namespace infobars {
class InfoBar;
}

class Profile;

class TabSharingUI : public MediaStreamUI,
                     public TabStripModelObserver,
                     public BrowserListObserver,
                     public infobars::InfoBarManager::Observer {
 public:
  TabSharingUI(const content::DesktopMediaID& media_id,
               base::string16 app_name);
  ~TabSharingUI() override;

  // MediaStreamUI:
  // Called when tab sharing has started. Creates infobars on all tabs.
  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // infobars::InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* info_bar, bool animate) override;
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override;

  // Runs |source_callback_| to start sharing the tab containing |infobar|.
  // Removes infobars on all tabs; OnStarted() will recreate the infobars with
  // updated title and buttons.
  void StartSharing(infobars::InfoBar* infobar);

  // Runs |stop_callback_| to stop sharing |shared_tab_|. Removes infobars on
  // all tabs.
  void StopSharing();

 private:
  void CreateInfobarForAllTabs();
  void RemoveInfobarForAllTabs();

  std::map<content::WebContents*, infobars::InfoBar*> infobars_;
  ScopedObserver<TabStripModel, TabStripModelObserver>
      tab_strip_models_observer_{this};
  const base::string16 app_name_;
  content::WebContents* shared_tab_;
  base::string16 shared_tab_name_;
  Profile* profile_;

  content::MediaStreamUI::SourceCallback source_callback_;
  base::OnceClosure stop_callback_;
};

#endif  // CHROME_BROWSER_UI_TAB_SHARING_TAB_SHARING_UI_H_
