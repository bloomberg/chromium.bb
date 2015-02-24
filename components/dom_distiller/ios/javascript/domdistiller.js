// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Applies DomDistillerJs to the content of the page and returns a
// DomDistillerResults (as a javascript object/dict).
(function() {
  try {
    // UIWebView's JavaScript engine has a bug that causes crashes when creating
    // a separate window object (as in the non-iOS version of this file), so
    // allow the script to run directly in the window until a better solution
    // is created.
    // TODO(kkhorimoto): investigate whether this is necessary for WKWebView.
    <include
    src="../../../../third_party/dom_distiller_js/package/js/domdistiller.js"/>
    // UIWebView requires javascript to return a single string value.
    return JSON.stringify(
        window.com.dom_distiller.DomDistiller.applyWithOptions($$OPTIONS));
  } catch (e) {
    window.console.error("Error during distillation: " + e);
    if (e.stack != undefined) window.console.error(e.stack);
  }
  return undefined;
})();
