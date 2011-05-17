// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following test runs the BiDi Checker on the current page with LTR
// base direction. Any bad mixture or RTL and LTR will be reported back
// by this test.

function filtersForPage(pageName, isRTL) {
  // Suppression filters should be added to the object below. You may omit
  // either RTL or LTR filters, or may not include filters for your page at all.
  // For additional info about BidiChecker go to
  // http://code.google.com/p/bidichecker
  // For specific info about filters, check out the following links:
  // http://bidichecker.googlecode.com/svn/trunk/docs/users_guide.html#TOC-Error-descriptions
  // http://bidichecker.googlecode.com/svn/trunk/docs/jsdoc/index.html
  // TODO(ofri): Link to more comprehensive documentation when available.
  var filters = {
    // Page filters
    "chrome://history/" : {
      // Filters for LTR UI
      "LTR" : [
        // BUG: http://code.google.com/p/chromium/issues/detail?id=80791
        bidichecker.FilterFactory.atText("בדיקה")
      ],
      // Filters for RTL UI
      "RTL" : [
      ]
    },
    "chrome://settings/autofill" : {
      "LTR" : [
        // BUG: http://code.google.com/p/chromium/issues/detail?id=82267
        bidichecker.FilterFactory.atText("משה ב כהן, דרך מנחם בגין")
      ]
    }
  };

  var dir = isRTL ? "RTL" : "LTR";
  if (filters.hasOwnProperty(pageName) &&
      filters[pageName].hasOwnProperty(dir)) {
    return filters[pageName][dir];
  } else {
    return [];
  }
}

function buildPrettyErrors(bidiErrors) {
  var idx;
  var prettyErrors = [];
  for (idx = 0; idx < bidiErrors.length; ++idx) {
    prettyErrors[idx] = bidiErrors[idx].toString();
  }
  return prettyErrors;
}

function runBidiChecker(pageName, isRTL) {
  var filters = filtersForPage(pageName, isRTL);
  var bidiErrors = bidichecker.checkPage(isRTL, top.document.body, filters);
  assertTrue(bidiErrors.length == 0, buildPrettyErrors(bidiErrors));
}