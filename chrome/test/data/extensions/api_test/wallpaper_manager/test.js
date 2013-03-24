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
  var requestImage = function(url, onLoadCallback) {
    var wallpaperRequest = new XMLHttpRequest();
    wallpaperRequest.open('GET', url, true);
    wallpaperRequest.responseType = 'arraybuffer';
    try {
      wallpaperRequest.send(null);
      wallpaperRequest.onloadend = function(e) {
        onLoadCallback(wallpaperRequest.status, wallpaperRequest.response);
      };
    } catch (e) {
      console.error(e);
      chrome.test.fail('An error thrown when requesting wallpaper.');
    };
  };
  chrome.test.runTests([
    function getWallpaperStrings() {
      chrome.wallpaperPrivate.getStrings(pass(function(strings) {
        wallpaperStrings = strings;
      }));
    },
    function setOnlineJpegWallpaper() {
      var url = "http://a.com:PORT/files/extensions/api_test" +
          "/wallpaper_manager/test.jpg";
      url = url.replace(/PORT/, config.testServer.port);
      requestImage(url, function(requestStatus, response) {
        if (requestStatus === 200) {
          wallpaper = response;
          chrome.wallpaperPrivate.setWallpaper(wallpaper,
                                               'CENTER_CROPPED',
                                               url,
                                               pass(function() {
            chrome.wallpaperPrivate.setCustomWallpaperLayout('CENTER',
                fail('Only custom wallpaper can change layout.'));
          }));
        } else {
          chrome.test.fail('Failed to load test.jpg from local server.');
        }
      });
    },
    function setCustomJpegWallpaper() {
      chrome.wallpaperPrivate.setCustomWallpaper(wallpaper,
                                                 'CENTER_CROPPED',
                                                 true,
                                                 '123',
                                                 pass(function(thumbnail) {
        chrome.wallpaperPrivate.setCustomWallpaperLayout('CENTER',
                                                         pass(function() {
          chrome.wallpaperPrivate.setCustomWallpaperLayout('STRETCH', pass());
        }));
      }));
    },
    function getCustomWallpaperThumbnail() {
      chrome.wallpaperPrivate.getOfflineWallpaperList('CUSTOM',
                                                      pass(function(lists) {
        chrome.test.assertEq(1, lists.length);
        chrome.wallpaperPrivate.getThumbnail(lists[0], 'CUSTOM',
                                             pass(function(data) {
          chrome.test.assertNoLastError();
        }));
      }));
    },
    function setCustomJepgBadWallpaper() {
      var url = "http://a.com:PORT/files/extensions/api_test" +
          "/wallpaper_manager/test_bad.jpg";
      url = url.replace(/PORT/, config.testServer.port);
      requestImage(url, function(requestStatus, response) {
        if (requestStatus === 200) {
          var badWallpaper = response;
          chrome.wallpaperPrivate.setCustomWallpaper(badWallpaper,
              'CENTER_CROPPED', false, '123',
              fail(wallpaperStrings.invalidWallpaper));
        } else {
          chrome.test.fail('Failed to load test_bad.jpg from local server.');
        }
      });
    },
    function setWallpaperFromFileSystem() {
      var url = "http://a.com:PORT/files/extensions/api_test" +
          "/wallpaper_manager/test.jpg";
      url = url.replace(/PORT/, config.testServer.port);
      chrome.wallpaperPrivate.setWallpaperIfExists(url, 'CENTER_CROPPED',
                                                   'ONLINE',
                                                   pass(function(exists) {
        chrome.test.assertTrue(exists);
        chrome.wallpaperPrivate.setWallpaperIfExists(
            'http://dummyurl/test1.jpg', 'CENTER_CROPPED', 'ONLINE',
            fail('Failed to set wallpaper test1.jpg from file system.'));
      }));
    },
    function getAndSetThumbnail() {
      var url = "http://a.com:PORT/files/extensions/api_test" +
          "/wallpaper_manager/test.jpg";
      url = url.replace(/PORT/, config.testServer.port);
      chrome.wallpaperPrivate.getThumbnail(url, 'ONLINE', pass(function(data) {
        chrome.test.assertNoLastError();
        if (data) {
          chrome.test.fail('Thumbnail is not found. getThumbnail should not ' +
                           'return any data.');
        }
        chrome.wallpaperPrivate.saveThumbnail(url, wallpaper, pass(function() {
          chrome.test.assertNoLastError();
          chrome.wallpaperPrivate.getThumbnail(url, 'ONLINE',
                                               pass(function(data) {
            chrome.test.assertNoLastError();
            // Thumbnail should already be saved to thumbnail directory.
            chrome.test.assertEq(wallpaper, data);
          }));
        }));
      }));
    },
    function getOfflineWallpaperList() {
      chrome.wallpaperPrivate.getOfflineWallpaperList('ONLINE',
                                                      pass(function(list) {
        // We have previously saved test.jpg in wallpaper directory.
        chrome.test.assertEq('test.jpg', list[0]);
        // Saves the same wallpaper to wallpaper directory but name it as
        // test1.jpg.
        chrome.wallpaperPrivate.setWallpaper(wallpaper,
                                             'CENTER_CROPPED',
                                             'http://dummyurl/test1.jpg',
                                             pass(function() {
          chrome.wallpaperPrivate.getOfflineWallpaperList('ONLINE',
                                                          pass(function(list) {
            chrome.test.assertEq('test.jpg', list[0]);
            chrome.test.assertEq('test1.jpg', list[1]);
          }));
        }));
      }));
    }
  ]);
});
