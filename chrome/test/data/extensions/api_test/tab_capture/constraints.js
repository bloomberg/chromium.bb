// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function supportsMediaConstraints() {
    chrome.tabCapture.capture({
      video: true,
      audio: true,
      videoConstraints: {
          mandatory: {
            maxWidth: 1000,
            minWidth: 300
          }
      }
    }, function(stream) {
      chrome.test.assertTrue(!!stream);
      stream.getVideoTracks()[0].stop();
      stream.getAudioTracks()[0].stop();
      chrome.test.succeed();
    });
  },

  function rejectsOptionalMediaConstraints() {
    chrome.tabCapture.capture({
      video: true,
      audio: true,
      videoConstraints: {
        mandatory: {
        },
        optional: {
          maxWidth: 1000,
          minWidth: 300
        }
      }
    }, function(stream) {
      chrome.test.assertTrue(!stream);
      chrome.test.succeed();
    });
  },

  function rejectsInvalidConstraints() {
    chrome.tabCapture.capture({
      video: true,
      audio: true,
      videoConstraints: {
        mandatory: {
          notValid: '123'
        }
      }
    }, function(stream) {
      chrome.test.assertTrue(!stream);

      chrome.tabCapture.capture({
        audio: true,
        audioConstraints: {
          mandatory: {
            notValid: '123'
          }
        }
      }, function(stream) {
        chrome.test.assertTrue(!stream);
        chrome.test.succeed();
      });
    });
  }
]);
