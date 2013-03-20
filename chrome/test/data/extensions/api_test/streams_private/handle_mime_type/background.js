// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// True iff the notifyFail has alerady been called.
var hasFailed = false;

chrome.streamsPrivate.onExecuteMimeTypeHandler.addListener(
    function(mime_type, original_url, content_url) {
  // The tests are setup so resources with MIME type 'application/msword' are
  // meant to be handled by the extension. The extension getting an event with
  // the MIME type 'application/msword' means the test has succeeded.
  if (mime_type == 'application/msword') {
    chrome.test.notifyPass();
    return;
  }

  // The tests are setup so resources with MIME type 'plain/text' are meant to
  // be handled by the browser (i.e. downloaded). The extension getting event
  // with MIME type 'plain/text' is thus a failure.
  if (mime_type == 'plain/text') {
    chrome.test.notifyFail(
        'Unexpected request to handle "plain/text" MIME type.');
    // Set |hasFailed| so notifyPass doesn't get called later (when event with
    // MIME type 'test/done' is received).
    hasFailed = true;
    return;
  }

  // MIME type 'test/done' is received only when tests for which no events
  // should be raised to notify the extension it's job is done. If the extension
  // receives the 'test/done' and there were no previous failures, notify that
  // the test has succeeded.
  if (!hasFailed && mime_type == 'test/done')
    chrome.test.notifyPass();
});
