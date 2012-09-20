// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
This extension is a platform app that provides a Web Intent handler; it accepts
incoming requests and invokes chrome.test.succeed() immediately.
*/

function launchedListener(args) {
  if (!(args && args['intent'])) {
    chrome.test.fail('Expected web intent on args: ' + args);
    return;
  }
  var intent = args['intent'];
  chrome.test.assertEq('http://webintents.org/view', intent['action']);
  chrome.test.succeed();

  // Note that we're not using chrome.extension.sendRequest here to call back
  // to the source app - the call is not available in v2 packaged apps. The
  // most we can do for now is succeed or fail the test (to be caught by a
  // ResultCatcher in external_filesystem_apitest.cc).
}

chrome.app.runtime.onLaunched.addListener(launchedListener);
