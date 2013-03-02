// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_url_handler_impl.h"

#include "base/string_util.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"

namespace content {

// Handles rewriting view-source URLs for what we'll actually load.
static bool HandleViewSource(GURL* url,
                             BrowserContext* browser_context) {
  if (url->SchemeIs(chrome::kViewSourceScheme)) {
    // Load the inner URL instead.
    *url = GURL(url->path());

    // Bug 26129: limit view-source to view the content and not any
    // other kind of 'active' url scheme like 'javascript' or 'data'.
    static const char* const allowed_sub_schemes[] = {
      chrome::kHttpScheme, chrome::kHttpsScheme, chrome::kFtpScheme,
      chrome::kChromeDevToolsScheme, chrome::kChromeUIScheme,
      chrome::kFileScheme, chrome::kFileSystemScheme
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

// Turns a non view-source URL into the corresponding view-source URL.
static bool ReverseViewSource(GURL* url, BrowserContext* browser_context) {
  // No action necessary if the URL is already view-source:
  if (url->SchemeIs(chrome::kViewSourceScheme))
    return false;

  url_canon::Replacements<char> repl;
  repl.SetScheme(chrome::kViewSourceScheme,
      url_parse::Component(0, strlen(chrome::kViewSourceScheme)));
  repl.SetPath(url->spec().c_str(),
      url_parse::Component(0, url->spec().size()));
  *url = url->ReplaceComponents(repl);
  return true;
}

static bool HandleDebugUrl(GURL* url, BrowserContext* browser_context) {
  // Circumvent processing URLs that the renderer process will handle.
  return *url == GURL(kChromeUICrashURL) ||
         *url == GURL(kChromeUIHangURL) ||
         *url == GURL(kChromeUIKillURL) ||
         *url == GURL(kChromeUIShorthangURL);
}

// static
BrowserURLHandler* BrowserURLHandler::GetInstance() {
  return BrowserURLHandlerImpl::GetInstance();
}

// static
BrowserURLHandler::URLHandler BrowserURLHandler::null_handler() {
  // Required for VS2010: http://connect.microsoft.com/VisualStudio/feedback/details/520043/error-converting-from-null-to-a-pointer-type-in-std-pair
  return NULL;
}

// static
BrowserURLHandlerImpl* BrowserURLHandlerImpl::GetInstance() {
  return Singleton<BrowserURLHandlerImpl>::get();
}

BrowserURLHandlerImpl::BrowserURLHandlerImpl() {
  AddHandlerPair(&HandleDebugUrl, BrowserURLHandlerImpl::null_handler());

  GetContentClient()->browser()->BrowserURLHandlerCreated(this);

  // view-source:
  AddHandlerPair(&HandleViewSource, &ReverseViewSource);
}

BrowserURLHandlerImpl::~BrowserURLHandlerImpl() {
}

void BrowserURLHandlerImpl::AddHandlerPair(URLHandler handler,
                                           URLHandler reverse_handler) {
  url_handlers_.push_back(HandlerPair(handler, reverse_handler));
}

void BrowserURLHandlerImpl::RewriteURLIfNecessary(
    GURL* url,
    BrowserContext* browser_context,
    bool* reverse_on_redirect) {
  for (size_t i = 0; i < url_handlers_.size(); ++i) {
    URLHandler handler = *url_handlers_[i].first;
    if (handler && handler(url, browser_context)) {
      *reverse_on_redirect = (url_handlers_[i].second != NULL);
      return;
    }
  }
}

bool BrowserURLHandlerImpl::ReverseURLRewrite(
    GURL* url, const GURL& original, BrowserContext* browser_context) {
  for (size_t i = 0; i < url_handlers_.size(); ++i) {
    URLHandler reverse_rewriter = *url_handlers_[i].second;
    if (reverse_rewriter) {
      GURL test_url(original);
      URLHandler handler = *url_handlers_[i].first;
      if (!handler) {
        if (reverse_rewriter(url, browser_context))
          return true;
      } else if (handler(&test_url, browser_context)) {
        return reverse_rewriter(url, browser_context);
      }
    }
  }
  return false;
}

}  // namespace content
