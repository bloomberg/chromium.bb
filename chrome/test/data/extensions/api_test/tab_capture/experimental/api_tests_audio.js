// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabCapture = chrome.tabCapture;

chrome.test.runTests([

  function onlyAudio() {
    tabCapture.capture({audio: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      stream.stop();
      chrome.test.succeed();
    });
  },

  function invalidAudioConstraints() {
    var tabCaptureListener = function(info) {
      if (info.status == 'stopped') {
        tabCapture.onStatusChanged.removeListener(tabCaptureListener);

        tabCapture.capture({audio: true, video: true}, function(stream) {
          chrome.test.assertTrue(!!stream);
          chrome.test.succeed();
        });
      }
    };
    tabCapture.onStatusChanged.addListener(tabCaptureListener);

    tabCapture.capture({audio: true,
                        audioConstraints: {
                          'mandatory': {
                            'notValid': '123'
                          }
                        }}, function(stream) {
                          chrome.test.assertTrue(!stream);
                        });
  }

]);
