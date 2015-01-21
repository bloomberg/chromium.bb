// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This API call should always return permission denied because geolocation
// implementation is not complete in app_shell.

function checkErrorCode(error) {
  if (error.code == error.PERMISSION_DENIED)
    chrome.test.succeed();
  else
    chrome.test.fail();
};

chrome.test.runTests([
  function geolocation_getCurrentPosition() {
    navigator.geolocation.getCurrentPosition(chrome.test.fail,
                                             checkErrorCode);
  }
]);
