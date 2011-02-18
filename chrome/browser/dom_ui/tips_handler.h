// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class pulls data from a web resource (such as a JSON feed) which
// has been stored in the user's preferences file.  Used mainly
// by the suggestions and tips area of the new tab page.

#ifndef CHROME_BROWSER_DOM_UI_TIPS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_TIPS_HANDLER_H_
#pragma once

#include <string>

#include "chrome/browser/webui/web_ui.h"

class DictionaryValue;
class ListValue;
class PrefService;

class TipsHandler : public WebUIMessageHandler {
 public:
  TipsHandler() : tips_cache_(NULL) {}
  virtual ~TipsHandler() {}

  // WebUIMessageHandler implementation and overrides.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  // Callback which pulls tips data from the preferences.
  void HandleGetTips(const ListValue* args);

  // Register tips cache with pref service.
  static void RegisterUserPrefs(PrefService* prefs);

 private:
  // Make sure the string we are pushing to the NTP is a valid URL.
  bool IsValidURL(const std::wstring& url_string);

  // Send a tip to the NTP.  tip_type is "tip_html_text" if the tip is from
  // the tip server, and "set_homepage_tip" if it's the tip to set the NTP
  // as home page.
  void SendTip(const std::string& tip, const std::string& tip_type,
               int tip_index);

  // So we can push data out to the page that has called this handler.
  WebUI* web_ui_;

  // Filled with data from cache in preferences.
  DictionaryValue* tips_cache_;

  DISALLOW_COPY_AND_ASSIGN(TipsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_TIPS_HANDLER_H_

