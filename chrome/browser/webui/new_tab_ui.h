// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_NEW_TAB_UI_H_
#define CHROME_BROWSER_WEBUI_NEW_TAB_UI_H_
#pragma once

#include <string>

#include "base/gtest_prod_util.h"
#include "base/timer.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/webui/chrome_url_data_manager.h"
#include "chrome/browser/webui/web_ui.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class GURL;
class MessageLoop;
class PrefService;
class Profile;

// The TabContents used for the New Tab page.
class NewTabUI : public WebUI,
                 public NotificationObserver {
 public:
  explicit NewTabUI(TabContents* manager);
  ~NewTabUI();

  // Override WebUI methods so we can hook up the paint timer to the render
  // view host.
  virtual void RenderViewCreated(RenderViewHost* render_view_host);
  virtual void RenderViewReused(RenderViewHost* render_view_host);

  static void RegisterUserPrefs(PrefService* prefs);
  static void MigrateUserPrefs(PrefService* prefs, int old_pref_version,
                               int new_pref_version);

  // Whether we should disable the web resources backend service
  static bool WebResourcesEnabled();

  // Whether we should disable the first run notification based on the command
  // line switch.
  static bool FirstRunDisabled();

  // Adds "url", "title", and "direction" keys on incoming dictionary, setting
  // title as the url as a fallback on empty title.
  static void SetURLTitleAndDirection(DictionaryValue* dictionary,
                                      const string16& title,
                                      const GURL& gurl);

  // Converts a list of TabRestoreService entries to the JSON format required
  // by the NTP and adds them to the given list value.
  static void AddRecentlyClosedEntries(
      const TabRestoreService::Entries& entries,
      ListValue* entry_list_value);

  // The current preference version.
  static int current_pref_version() { return current_pref_version_; }

  class NewTabHTMLSource : public ChromeURLDataManager::DataSource {
   public:
    explicit NewTabHTMLSource(Profile* profile);

    // Called when the network layer has requested a resource underneath
    // the path we registered.
    virtual void StartDataRequest(const std::string& path,
                                  bool is_off_the_record,
                                  int request_id);

    virtual std::string GetMimeType(const std::string&) const;

    virtual bool ShouldReplaceExistingSource() const;

    // Setters and getters for first_run.
    static void set_first_run(bool first_run) { first_run_ = first_run; }
    static bool first_run() { return first_run_; }

   private:
    virtual ~NewTabHTMLSource() {}

    // Whether this is the first run.
    static bool first_run_;

    // Pointer back to the original profile.
    Profile* profile_;

    DISALLOW_COPY_AND_ASSIGN(NewTabHTMLSource);
  };

 private:
  FRIEND_TEST_ALL_PREFIXES(NewTabUITest, UpdateUserPrefsVersion);

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Reset the CSS caches.
  void InitializeCSSCaches();

  void StartTimingPaint(RenderViewHost* render_view_host);
  void PaintTimeout();

  // Updates the user prefs version and calls |MigrateUserPrefs| if needed.
  // Returns true if the version was updated.
  static bool UpdateUserPrefsVersion(PrefService* prefs);

  NotificationRegistrar registrar_;

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

#endif  // CHROME_BROWSER_WEBUI_NEW_TAB_UI_H_
