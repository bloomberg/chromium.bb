// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following test runs the BiDi Checker on the current page with LTR
// base direction. Any bad mixture or RTL and LTR will be reported back
// by this test.
function runBidiCheckerLTR() {
  var bidiErrors = bidichecker.checkPage(false, top.document.body);
  assertTrue(bidiErrors.length == 0, bidiErrors);
}