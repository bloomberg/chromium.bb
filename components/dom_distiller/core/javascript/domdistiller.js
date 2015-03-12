// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Applies DomDistillerJs to the content of the page and returns a
// DomDistillerResults (as a javascript object/dict).
(function() {
  try {
    // The generated domdistiller.js accesses the window object only explicitly
    // via the window name. So, we create a new object with the normal window
    // object as its prototype and initialize the domdistiller.js with that new
    // context so that it doesn't change the real window object.
    function initialize(window) {
      // This include will be processed at build time by grit.
      <include src="../../../../third_party/dom_distiller_js/package/js/domdistiller.js"/>
    }
    <if expr="is_ios">
      // UIWebView's JavaScript engine has a bug that causes crashes when
      // creating a separate window object, so allow the script to run directly
      // in the window until a better solution is created.
      // TODO(kkhorimoto): investigate whether this is necessary for WKWebView.
      var context = window;
    </if>
    <if expr="not is_ios">
      var context = Object.create(window);
    </if>
    context.setTimeout = function() {};
    context.clearTimeout = function() {};
    initialize(context);

    // The OPTIONS placeholder will be replaced with the DomDistillerOptions at
    // runtime.
    var distiller = context.org.chromium.distiller.DomDistiller;
    var res = distiller.applyWithOptions($$OPTIONS);
  <if expr="is_ios">
    // UIWebView requires javascript to return a single string value.
    return JSON.stringify(res);
  </if>
  <if expr="not is_ios">
    return res;
  </if>
  } catch (e) {
    window.console.error("Error during distillation: " + e);
    if (e.stack != undefined) window.console.error(e.stack);
  }
  return undefined;
})()
