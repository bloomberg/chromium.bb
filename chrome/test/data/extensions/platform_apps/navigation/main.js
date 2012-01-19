// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var inAppUrl = 'nav-target.html';

function runTests(remoteUrl) {
  var testForm = document.getElementById('test-form');
  var testLink = document.getElementById('test-link');

  function clickTestLink() {
    var clickEvent = document.createEvent('MouseEvents');
    clickEvent.initMouseEvent('click', true, true, window,
                              0, 0, 0, 0, 0, false, false,
                              false, false, 0, null);

    return testLink.dispatchEvent(clickEvent);
  }

  var tests = [
    // Location functions to in-app URLs.
    function() { window.location = inAppUrl },
    function() { window.location.href = inAppUrl; },
    function() { window.location.replace(inAppUrl); },
    function() { window.location.assign(inAppUrl); },

    // Location function to remote URLs (not testing all ways of navigating to
    // it, since that was covered by the previous set)
    function() { window.location = remoteUrl; },

    // Form submission (GET, POST, in-app, and remote)
    function() {
      testForm.method = 'GET';
      testForm.action = inAppUrl;
      testForm.submit();
    },
    function() {
      testForm.method = 'POST';
      testForm.action = inAppUrl;
      testForm.submit();
    },
    function() {
      testForm.method = 'GET';
      testForm.action = remoteUrl;
      testForm.submit();
    },
    function() {
      testForm.method = 'POST';
      testForm.action = remoteUrl;
      testForm.submit();
    },

    // Clicks on links (in-app and remote).
    function() { testLink.href = inAppUrl; clickTestLink(); },
    function() { testLink.href = remoteUrl; clickTestLink(); },

    // If we manage to execute this test case, then we haven't navigated away.
    function() { window.domAutomationController.send(true); }
  ];

  var testIndex = 0;

  function runTest() {
    var test = tests[testIndex++];

    console.log('Testing ' + (testIndex - 1) + ': ' + test);
    test();

    if (testIndex < tests.length) {
      // Don't run the next test immediately, since navigation happens
      // asynchronously, and if we do end up navigating, we don't want to still
      // execute the final test case (which signals success).
      window.setTimeout(runTest, 100);
    }
  }

  runTest();
}
