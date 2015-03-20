// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Applies DomDistillerJs to the content of the page and returns a
// DomDistillerResults (as a javascript object/dict).
(function(options, stringify_output, use_new_context) {
  try {
    // The generated domdistiller.js accesses the window object only explicitly
    // via the window name. So, we create a new object with the normal window
    // object as its prototype and initialize the domdistiller.js with that new
    // context so that it doesn't change the real window object.
    function initialize(window) {
      // This include will be processed at build time by grit.
      <include src="../../../../third_party/dom_distiller_js/dist/js/domdistiller.js"/>
    }
    var context = use_new_context ? Object.create(window) : window
    context.setTimeout = function() {};
    context.clearTimeout = function() {};
    initialize(context);

    // The OPTIONS placeholder will be replaced with the DomDistillerOptions at
    // runtime.
    var distiller = context.org.chromium.distiller.DomDistiller;
    var res = distiller.applyWithOptions(options);

    if (stringify_output) {
      return JSON.stringify(res);
    }
    return res;
  } catch (e) {
    window.console.error("Error during distillation: " + e);
    if (e.stack != undefined) window.console.error(e.stack);
  }
  return undefined;
})(options = $$OPTIONS,
   stringify_output = $$STRINGIFY,
   use_new_context = $$NEW_CONTEXT)
