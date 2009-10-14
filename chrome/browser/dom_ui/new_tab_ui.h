// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_NEW_TAB_UI_H_
#define CHROME_BROWSER_DOM_UI_NEW_TAB_UI_H_

#include <string>

#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/common/notification_registrar.h"

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

  class NewTabHTMLSource : public ChromeURLDataManager::DataSource {
   public:
    explicit NewTabHTMLSource(Profile* profile);

    // Called when the network layer has requested a resource underneath
    // the path we registered.
    virtual void StartDataRequest(const std::string& path, int request_id);

    virtual std::string GetMimeType(const std::string&) const {
      return "text/html";
    }

    virtual MessageLoop* MessageLoopForRequestPath(const std::string& path)
        const {
      // NewTabHTMLSource does all of the operations that need to be on the
      // UI thread from InitFullHTML, called by the constructor.  It is safe
      // to call StartDataRequest from any thread, so return NULL.
      return NULL;
    }

    // Setters and getters for first_view.
    static void set_first_view(bool first_view) { first_view_ = first_view; }
    static bool first_view() { return first_view_; }

    // Setters and getters for first_run.
    static void set_first_run(bool first_run) { first_run_ = first_run; }
    static bool first_run() { return first_run_; }

   private:
    // In case a file path to the new tab page was provided this tries to load
    // the file and returns the file content if successful. This returns an
    // empty string in case of failure.
    static std::string GetCustomNewTabPageFromCommandLine();

    // Populate full_html_.  This must be called from the UI thread because it
    // involves profile access.
    //
    // A new NewTabHTMLSource object is used for each new tab page instance
    // and each reload of an existing new tab page, so there is no concern
    // about cached data becoming stale.
    void InitFullHTML();

    // The content to be served by StartDataRequest, stored by InitFullHTML.
    std::string full_html_;

    // Whether this is the first viewing of the new tab page and
    // we think it is the user's startup page.
    static bool first_view_;

    // Whether this is the first run.
    static bool first_run_;

    // The user's profile.
    Profile* profile_;

    DISALLOW_COPY_AND_ASSIGN(NewTabHTMLSource);
  };

 private:
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // Reset the CSS caches.
  void InitializeCSSCaches();

  NotificationRegistrar registrar_;

  // The message id that should be displayed in this NewTabUIContents
  // instance's motd area.
  int motd_message_id_;

  // Whether the user is in incognito mode or not, used to determine
  // what HTML to load.
  bool incognito_;

  DISALLOW_COPY_AND_ASSIGN(NewTabUI);
};

#endif  // CHROME_BROWSER_DOM_UI_NEW_TAB_UI_H_
