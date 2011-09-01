// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_PAGE_HANDLER_H_

#include "base/values.h"
#include "content/browser/webui/web_ui.h"

class PrefService;
class Profile;

// Handler for general New Tab Page functionality that does not belong in a
// more specialized handler.
class NewTabPageHandler : public WebUIMessageHandler {
 public:
  NewTabPageHandler() {}
  virtual ~NewTabPageHandler() {}

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for "closePromo".
  void HandleClosePromo(const ListValue* args);

  // Callback for "closeSyncNotification".
  void HandleCloseSyncNotification(const ListValue* args);

  // Callback for "pageSelected".
  void HandlePageSelected(const ListValue* args);

  // Callback for "navigationDotUsed". This is called when a navigation dot is
  // clicked, whereas pageSelected might be called after any page-switching user
  // action.
  void HandleNavDotUsed(const ListValue* args);

  // Callback for "handleIntroMessageSeen". No arguments. Called when the intro
  // message is displayed.
  void HandleIntroMessageSeen(const ListValue* args);

  // Callback for "introMessageSuppressed". This is called when ntp4 intro
  // message has been suppressed.
  static void HandleIntroMessageSuppressed(PrefService* prefs);

  // Register NTP preferences.
  static void RegisterUserPrefs(PrefService* prefs);

  // Registers values (strings etc.) for the page.
  static void GetLocalizedValues(Profile* profile, DictionaryValue* values);

 private:
  // The purpose of this enum is to track which page on the NTP is showing.
  // The lower 10 bits of kNTPShownPage are used for the index within the page
  // group, and the rest of the bits are used for the page group ID (defined
  // here).
  enum {
    INDEX_MASK = (1 << 10) - 1,
    MOST_VISITED_PAGE_ID = 1 << 10,
    APPS_PAGE_ID = 2 << 10,
    BOOKMARKS_PAGE_ID = 3 << 10,
  };

  DISALLOW_COPY_AND_ASSIGN(NewTabPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_PAGE_HANDLER_H_
