// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_url_handler.h"

#include "base/string_util.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/dom_ui/dom_ui_factory.h"
#include "chrome/browser/extensions/extension_dom_ui.h"
#include "chrome/browser/profile.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"

// Handles rewriting view-source URLs for what we'll actually load.
static bool HandleViewSource(GURL* url, Profile* profile) {
  if (url->SchemeIs(chrome::kViewSourceScheme)) {
    // Load the inner URL instead.
    *url = GURL(url->path());

    // Bug 26129: limit view-source to view the content and not any
    // other kind of 'active' url scheme like 'javascript' or 'data'.
    static const char* const allowed_sub_schemes[] = {
      chrome::kHttpScheme, chrome::kHttpsScheme, chrome::kFtpScheme,
      chrome::kChromeUIScheme
    };

    bool is_sub_scheme_allowed = false;
    for (size_t i = 0; i < arraysize(allowed_sub_schemes); i++) {
      if (url->SchemeIs(allowed_sub_schemes[i])) {
        is_sub_scheme_allowed = true;
        break;
      }
    }

    if (!is_sub_scheme_allowed) {
      *url = GURL(chrome::kAboutBlankURL);
      return false;
    }

    return true;
  }
  return false;
}

// Handles rewriting DOM UI URLs.
static bool HandleDOMUI(GURL* url, Profile* profile) {
  if (!DOMUIFactory::UseDOMUIForURL(*url))
    return false;

  // Special case the new tab page. In older versions of Chrome, the new tab
  // page was hosted at chrome-internal:<blah>. This might be in people's saved
  // sessions or bookmarks, so we say any URL with that scheme triggers the new
  // tab page.
  if (url->SchemeIs(chrome::kChromeInternalScheme)) {
    // Rewrite it with the proper new tab URL.
    *url = GURL(chrome::kChromeUINewTabURL);
  }

  return true;
}

std::vector<BrowserURLHandler::URLHandler> BrowserURLHandler::url_handlers_;

// static
void BrowserURLHandler::InitURLHandlers() {
  if (!url_handlers_.empty())
    return;

  // Add the default URL handlers.
  url_handlers_.push_back(&ExtensionDOMUI::HandleChromeURLOverride);
  url_handlers_.push_back(&WillHandleBrowserAboutURL);  // about:
  url_handlers_.push_back(&HandleDOMUI);                // chrome: & friends.
  url_handlers_.push_back(&HandleViewSource);           // view-source:
}

// static
void BrowserURLHandler::RewriteURLIfNecessary(GURL* url, Profile* profile) {
  if (url_handlers_.empty())
    InitURLHandlers();
  for (size_t i = 0; i < url_handlers_.size(); ++i) {
    if ((*url_handlers_[i])(url, profile))
      return;
  }
}
