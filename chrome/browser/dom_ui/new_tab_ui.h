// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_NEW_TAB_UI_H_
#define CHROME_BROWSER_DOM_UI_NEW_TAB_UI_H_

#include <string>

#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/common/notification_registrar.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class GURL;
class MessageLoop;
class PrefService;
class Profile;

// The TabContents used for the New Tab page.
class NewTabUI : public DOMUI,
                 public NotificationObserver {
 public:
  explicit NewTabUI(TabContents* manager);
  ~NewTabUI();

  // Override DOMUI methods so we can hook up the paint timer to the render
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

  // The current preference version.
  static const int current_pref_version() { return current_pref_version_; }

  class NewTabHTMLSource : public ChromeURLDataManager::DataSource {
   public:
    explicit NewTabHTMLSource(Profile* profile);

    // Called when the network layer has requested a resource underneath
    // the path we registered.
    virtual void StartDataRequest(const std::string& path,
                                  bool is_off_the_record,
                                  int request_id);

    virtual std::string GetMimeType(const std::string&) const {
      return "text/html";
    }

    // Setters and getters for first_run.
    static void set_first_run(bool first_run) { first_run_ = first_run; }
    static bool first_run() { return first_run_; }

   private:
    ~NewTabHTMLSource() {}

    // Whether this is the first run.
    static bool first_run_;

    // Pointer back to the original profile.
    Profile* profile_;

    DISALLOW_COPY_AND_ASSIGN(NewTabHTMLSource);
  };

 private:
  FRIEND_TEST(NewTabUITest, UpdateUserPrefsVersion);

  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // Reset the CSS caches.
  void InitializeCSSCaches();

  // Updates the user prefs version and calls |MigrateUserPrefs| if needed.
  // Returns true if the version was updated.
  static bool UpdateUserPrefsVersion(PrefService* prefs);

  NotificationRegistrar registrar_;

  // The preference version. This used for migrating prefs of the NTP.
  static const int current_pref_version_ = 1;

  DISALLOW_COPY_AND_ASSIGN(NewTabUI);
};

#endif  // CHROME_BROWSER_DOM_UI_NEW_TAB_UI_H_
