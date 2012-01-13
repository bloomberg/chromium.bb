// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_UI_H_
#pragma once

#include <string>

#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "content/browser/webui/web_ui.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui_controller.h"

class GURL;
class PrefService;
class Profile;

// The TabContents used for the New Tab page.
class NewTabUI : public WebUI, public content::WebUIController,
                 public content::NotificationObserver {
 public:
  explicit NewTabUI(content::WebContents* manager);
  virtual ~NewTabUI();

  static void RegisterUserPrefs(PrefService* prefs);

  // Adds "url", "title", and "direction" keys on incoming dictionary, setting
  // title as the url as a fallback on empty title.
  static void SetURLTitleAndDirection(base::DictionaryValue* dictionary,
                                      const string16& title,
                                      const GURL& gurl);

  // Returns a pointer to a NewTabUI if the WebUI object is a new tab page.
  static NewTabUI* FromWebUI(WebUI* ui);

  // The current preference version.
  static int current_pref_version() { return current_pref_version_; }

  // WebUIController implementation:
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewReused(RenderViewHost* render_view_host) OVERRIDE;

  // Returns true if the bookmark bar can be displayed over this webui, detached
  // from the location bar.
  bool CanShowBookmarkBar() const;

  class NewTabHTMLSource : public ChromeURLDataManager::DataSource {
   public:
    explicit NewTabHTMLSource(Profile* profile);

    // Called when the network layer has requested a resource underneath
    // the path we registered.
    virtual void StartDataRequest(const std::string& path,
                                  bool is_incognito,
                                  int request_id) OVERRIDE;

    virtual std::string GetMimeType(const std::string&) const OVERRIDE;

    virtual bool ShouldReplaceExistingSource() const OVERRIDE;

   private:
    virtual ~NewTabHTMLSource() {}

    // Pointer back to the original profile.
    Profile* profile_;

    DISALLOW_COPY_AND_ASSIGN(NewTabHTMLSource);
  };

 private:
  FRIEND_TEST_ALL_PREFIXES(NewTabUITest, UpdateUserPrefsVersion);

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Reset the CSS caches.
  void InitializeCSSCaches();

  void StartTimingPaint(RenderViewHost* render_view_host);
  void PaintTimeout();

  Profile* GetProfile() const;

  content::NotificationRegistrar registrar_;

  // The time when we started benchmarking.
  base::TimeTicks start_;
  // The last time we got a paint notification.
  base::TimeTicks last_paint_;
  // Scoping so we can be sure our timeouts don't outlive us.
  base::OneShotTimer<NewTabUI> timer_;
  // The preference version. This used for migrating prefs of the NTP.
  static const int current_pref_version_ = 3;

  DISALLOW_COPY_AND_ASSIGN(NewTabUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_UI_H_
