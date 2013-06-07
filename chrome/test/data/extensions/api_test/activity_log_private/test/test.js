// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.activityLogPrivate.onExtensionActivity.addListener(
    function(activity) {
      var activityId = activity["extensionId"];
      chrome.test.assertEq("pknkgggnfecklokoggaggchhaebkajji", activityId);
      var apiCall = activity["chromeActivityDetail"]["apiCall"];
      chrome.test.assertEq("runtime.onMessageExternal", apiCall);
      chrome.test.succeed();
    }
);

chrome.test.runTests([
    function triggerAnActivity() {
      chrome.runtime.sendMessage("pknkgggnfecklokoggaggchhaebkajji",
                                 "knock knock",
                                 function response() { });
  }
]);

