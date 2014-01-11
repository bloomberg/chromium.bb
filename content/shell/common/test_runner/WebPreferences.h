// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_TEST_RUNNER_WEBPREFERENCES_H_
#define CONTENT_SHELL_COMMON_TEST_RUNNER_WEBPREFERENCES_H_

#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebSettings.h"

namespace blink {
class WebView;
}

namespace WebTestRunner {

struct WebPreferences {
    int defaultFontSize;
    int minimumFontSize;
    bool DOMPasteAllowed;
    bool XSSAuditorEnabled;
    bool allowDisplayOfInsecureContent;
    bool allowFileAccessFromFileURLs;
    bool allowRunningOfInsecureContent;
    bool authorAndUserStylesEnabled;
    blink::WebString defaultTextEncodingName;
    bool experimentalWebGLEnabled;
    bool experimentalCSSRegionsEnabled;
    bool experimentalCSSGridLayoutEnabled;
    bool javaEnabled;
    bool javaScriptCanAccessClipboard;
    bool javaScriptCanOpenWindowsAutomatically;
    bool supportsMultipleWindows;
    bool javaScriptEnabled;
    bool loadsImagesAutomatically;
    bool offlineWebApplicationCacheEnabled;
    bool pluginsEnabled;
    bool allowUniversalAccessFromFileURLs;
    blink::WebSettings::EditingBehavior editingBehavior;
    bool tabsToLinks;
    bool hyperlinkAuditingEnabled;
    bool caretBrowsingEnabled;
    bool shouldRespectImageOrientation;
    bool asynchronousSpellCheckingEnabled;

    WebPreferences();
    void reset();
};

}

#endif  // CONTENT_SHELL_COMMON_TEST_RUNNER_WEBPREFERENCES_H_
