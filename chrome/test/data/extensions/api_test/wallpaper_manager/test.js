// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// wallpaperPrivate api test
// browser_tests --gtest_filter=ExtensionApiTest.wallpaperPrivate

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;

chrome.test.getConfig(function(config) {
  var wallpaper;
  var wallpaperStrings;
  chrome.test.runTests([
    function getWallpaperStrings() {
      chrome.wallpaperPrivate.getStrings(pass(function(strings) {
        wallpaperStrings = strings;
      }));
    },
    function setOnlineJpegWallpaper() {
      var wallpaperRequest = new XMLHttpRequest();
      var url = "http://a.com:PORT/files/extensions/api_test" +
          "/wallpaper_manager/test.jpg";
      url = url.replace(/PORT/, config.testServer.port);
      wallpaperRequest.open('GET', url, true);
      wallpaperRequest.responseType = 'arraybuffer';
      try {
        wallpaperRequest.send(null);
        wallpaperRequest.onload = function (e) {
          if (wallpaperRequest.status === 200) {
              wallpaper = wallpaperRequest.response;
              chrome.wallpaperPrivate.setWallpaper(wallpaper,
                                                   'CENTER_CROPPED',
                                                   url,
                                                   pass());
          } else {
            chrome.test.fail('Failed to load test.jpg from local server.');
          }
        };
      } catch (e) {
        console.error(e);
        chrome.test.fail('An error thrown when requesting wallpaper.');
      };
    },
    function setCustomJpegWallpaper() {
      chrome.wallpaperPrivate.setCustomWallpaper(wallpaper,
                                                 'CENTER_CROPPED',
                                                 pass());
    },
    function setCustomJepgBadWallpaper() {
      var wallpaperRequest = new XMLHttpRequest();
      var url = "http://a.com:PORT/files/extensions/api_test" +
          "/wallpaper_manager/test_bad.jpg";
      url = url.replace(/PORT/, config.testServer.port);
      wallpaperRequest.open('GET', url, true);
      wallpaperRequest.responseType = 'arraybuffer';
      try {
        wallpaperRequest.send(null);
        wallpaperRequest.onload = function (e) {
          if (wallpaperRequest.status === 200) {
              var badWallpaper = wallpaperRequest.response;
              chrome.wallpaperPrivate.setCustomWallpaper(badWallpaper,
                  'CENTER_CROPPED', fail(wallpaperStrings.invalidWallpaper));
          } else {
            chrome.test.fail('Failed to load test_bad.jpg from local server.');
          }
        };
      } catch (e) {
        console.error(e);
        chrome.test.fail('An error thrown when requesting wallpaper.');
      };
    }
  ]);
});
