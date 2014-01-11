// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/test_runner/WebPreferences.h"

using namespace blink;

namespace WebTestRunner {

WebPreferences::WebPreferences()
{
    reset();
}

void WebPreferences::reset()
{
    defaultFontSize = 16;
    minimumFontSize = 0;
    DOMPasteAllowed = true;
    XSSAuditorEnabled = false;
    allowDisplayOfInsecureContent = true;
    allowFileAccessFromFileURLs = true;
    allowRunningOfInsecureContent = true;
    defaultTextEncodingName = WebString::fromUTF8("ISO-8859-1");
    experimentalWebGLEnabled = false;
    experimentalCSSRegionsEnabled = true;
    experimentalCSSGridLayoutEnabled = true;
    javaEnabled = false;
    javaScriptCanAccessClipboard = true;
    javaScriptCanOpenWindowsAutomatically = true;
    supportsMultipleWindows = true;
    javaScriptEnabled = true;
    loadsImagesAutomatically = true;
    offlineWebApplicationCacheEnabled = true;
    pluginsEnabled = true;
    caretBrowsingEnabled = false;

    // Allow those layout tests running as local files, i.e. under
    // LayoutTests/http/tests/local, to access http server.
    allowUniversalAccessFromFileURLs = true;

#ifdef __APPLE__
    editingBehavior = WebSettings::EditingBehaviorMac;
#else
    editingBehavior = WebSettings::EditingBehaviorWin;
#endif

    tabsToLinks = false;
    hyperlinkAuditingEnabled = false;
    shouldRespectImageOrientation = false;
    asynchronousSpellCheckingEnabled = false;
}

}
