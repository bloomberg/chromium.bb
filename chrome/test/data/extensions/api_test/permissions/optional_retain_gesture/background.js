// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var fail = chrome.test.callbackFail;

var GESTURE_ERROR = "This function must be called during a user gesture";

function busyLoop(milliseconds) {
  var startTime = new Date().getTime();
  while (new Date().getTime() - startTime < milliseconds) {}
}

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function testPermissionsRetainGesture() {
      chrome.test.runWithUserGesture(function() {
        chrome.permissions.request(
            {permissions: ['bookmarks']},
            function(granted) {
              chrome.test.assertTrue(granted);

              // The user gesture is retained, so we can request again.
              chrome.permissions.request(
                  {permissions: ['bookmarks']},
                  function(granted) {
                    chrome.test.assertTrue(granted);

                    // The user gesture is retained but is consumed outside,
                    // so the following request will fail.
                    chrome.permissions.request(
                        {permissions: ['bookmarks']},
                        fail(GESTURE_ERROR));
                  }
              );

              // Consume the user gesture
              window.open("", "", "");
            }
        );
      });
    },

    function testPermissionsRetainGestureExpire() {
      chrome.test.runWithUserGesture(function() {
        chrome.permissions.request(
            {permissions: ['bookmarks']},
            function(granted) {
              chrome.test.assertTrue(granted);

              var request = function() {
                // The following request will fail if the user gesture is
                // expired.
                chrome.permissions.request(
                    {permissions: ['bookmarks']},
                    function(granted1) {
                      if (chrome.runtime.lastError) {
                        chrome.test.assertFalse(granted1);
                        chrome.test.assertEq(chrome.runtime.lastError.message,
                                             GESTURE_ERROR);
                        chrome.test.succeed();
                      } else {
                        console.log("Retrying permissions.request in 3 " +
                            "seconds");
                        busyLoop(3000);
                        request();
                      }
                    });
              };

              // Wait 2s since the user gesture timeout is 1s.
              busyLoop(2000);
              request();
            }
        );
      });
    }
  ]);
});
