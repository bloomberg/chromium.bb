// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_UI_H_
#pragma once

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui_controller.h"

class GURL;
class PrefService;
class Profile;

// The WebContents used for the New Tab page.
class NewTabUI : public content::WebUIController,
                 public content::NotificationObserver {
 public:
  explicit NewTabUI(content::WebUI* web_ui);
  virtual ~NewTabUI();

  static void RegisterUserPrefs(PrefService* prefs);

  // Returns whether or not to show apps pages.
  static bool ShouldShowApps();

  // Returns whether or not the "suggestions links page" is enabled.
  static bool IsSuggestionsPageEnabled();

  // Adds "url", "title", and "direction" keys on incoming dictionary, setting
  // title as the url as a fallback on empty title.
  static void SetURLTitleAndDirection(base::DictionaryValue* dictionary,
                                      const string16& title,
                                      const GURL& gurl);

  // Returns a pointer to a NewTabUI if the WebUIController object is a new tab
  // page.
  static NewTabUI* FromWebUIController(content::WebUIController* ui);

  // The current preference version.
  static int current_pref_version() { return current_pref_version_; }

  // WebUIController implementation:
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewReused(
      content::RenderViewHost* render_view_host) OVERRIDE;

  // Returns true if the bookmark bar can be displayed over this webui, detached
  // from the location bar.
  bool CanShowBookmarkBar() const;

  bool showing_sync_bubble() { return showing_sync_bubble_; }
  void set_showing_sync_bubble(bool showing) { showing_sync_bubble_ = showing; }

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

    // Adds |resource| to the source. |resource_id| is resource id or 0,
    // which means return empty data set. |mime_type| is mime type of the
    // resource.
    void AddResource(const char* resource,
                     const char *mime_type,
                     int resource_id);

   private:
    virtual ~NewTabHTMLSource() {}

    // Pointer back to the original profile.
    Profile* profile_;

    // Maps resource files to mime types an resource ids.
    std::map<std::string, std::pair<std::string, int> > resource_map_;

    DISALLOW_COPY_AND_ASSIGN(NewTabHTMLSource);
  };

 private:
  FRIEND_TEST_ALL_PREFIXES(NewTabUITest, UpdateUserPrefsVersion);

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Reset the CSS caches.
  void InitializeCSSCaches();

  void StartTimingPaint(content::RenderViewHost* render_view_host);
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

  // If the sync promo NTP bubble is being shown.
  bool showing_sync_bubble_;

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(NewTabUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_UI_H_
