// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These includes will be processed at build time by grit.
<include src="../../../../third_party/dom_distiller_js/package/js/domdistiller.js"/>

// Applies DomDistillerJs to the content of the page and returns a
// DomDistillerResults (as a javascript object/dict).
(function() {
  try {
    // The OPTIONS placeholder will be replaced with the DomDistillerOptions at
    // runtime.
    res = com.dom_distiller.DomDistiller.applyWithOptions($$OPTIONS);
    return res;
  } catch (e) {
    window.console.error("Error during distillation: " + e);
    if (e.stack != undefined) window.console.error(e.stack);
  }
  return undefined;
})()
