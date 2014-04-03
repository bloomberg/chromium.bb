// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These includes will be processed at build time by grit.
<include src="../../../../third_party/dom_distiller_js/js/domdistiller.js"/>

// Extracts long-form content from a page and returns an array where the first
// element is the article title, the second element is HTML containing the
// long-form content, the third element is the next page link, and the fourth
// element is the previous page link.
(function() {
  var result = new Array(4);
  try {
    result[0] = com.dom_distiller.DocumentTitleGetter.getDocumentTitle(
        document.title, document.documentElement);
    result[1] = com.dom_distiller.ContentExtractor.extractContent();
    result[2] = com.dom_distiller.PagingLinksFinder.findNext(
        document.documentElement);
    // TODO(shashishekhar): Add actual previous page link here.
    result[3] = '';
  } catch (e) {
    window.console.log("Error during distillation: " + e);
  }
  return result;
})()
