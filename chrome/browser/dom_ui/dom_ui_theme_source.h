// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_DOM_UI_THEME_SOURCE_H_
#define CHROME_BROWSER_DOM_UI_DOM_UI_THEME_SOURCE_H_

#include <string>

#include "chrome/browser/dom_ui/chrome_url_data_manager.h"

class Profile;

// ThumbnailSource is the gateway between network-level chrome:
// requests for thumbnails and the history backend that serves these.
class DOMUIThemeSource : public ChromeURLDataManager::DataSource {
 public:
  explicit DOMUIThemeSource(Profile* profile);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path, int request_id);
  virtual std::string GetMimeType(const std::string& path) const;

  virtual void SendResponse(int request_id, RefCountedMemory* data);

  virtual MessageLoop* MessageLoopForRequestPath(const std::string& path) const;

 private:
  // Populate new_tab_css_ and new_incognito_tab_css.  These must be called
  // from the UI thread because they involve profile and theme access.
  //
  // A new DOMUIThemeSource object is used for each new tab page instance
  // and each reload of an existing new tab page, so there is no concern about
  // cached data becoming stale.
  void InitNewTabCSS();
  void InitNewIncognitoTabCSS();

  // Send the CSS for the new tab or the new incognito tab.
  void SendNewTabCSS(int request_id, const std::string& css_string);

  // Fetch and send the theme bitmap.
  void SendThemeBitmap(int request_id, int resource_id);

  // Get the CSS string for the background position on the new tab page for the
  // states when the bar is attached or detached.
  std::string GetNewTabBackgroundCSS(bool bar_attached);

  // How the background image on the new tab page should be tiled (see tiling
  // masks in browser_theme_provider.h).
  std::string GetNewTabBackgroundTilingCSS();

  // The content to be served by SendNewTabCSS, stored by InitNewTabCSS and
  // InitNewIncognitoTabCSS.
  std::string new_tab_css_;
  std::string new_incognito_tab_css_;

  Profile* profile_;
  DISALLOW_COPY_AND_ASSIGN(DOMUIThemeSource);
};

#endif  // CHROME_BROWSER_DOM_UI_DOM_UI_THEME_SOURCE_H_
