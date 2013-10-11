// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function emptySourceList() {
    chrome.desktopCapture.chooseDesktopMedia(
        [],
        chrome.test.callback(function(id) {
          chrome.test.assertEq("undefined", typeof id);
        }, "At least one source type must be specified."));
  },

  // The prompt is canceled.
  function pickerUiCanceled() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window"],
        chrome.test.callbackPass(function(id) {
          chrome.test.assertEq("string", typeof id);
          chrome.test.assertTrue(id == "");
        }));
  },

  // A source is chosen.
  function chooseMedia() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window"],
        chrome.test.callbackPass(function(id) {
          chrome.test.assertEq("string", typeof id);
          chrome.test.assertTrue(id != "");
        }));
  },

  // For the following two tests FakeDestkopPickerFactory will verify that
  // the right set of sources is selected when creating picker model.
  function screensOnly() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["screen"], chrome.test.callbackPass(function(id) {}));
  },

  function windowsOnly() {
    chrome.desktopCapture.chooseDesktopMedia(
        ["window"], chrome.test.callbackPass(function(id) {}));
  },

  // Show window picker and then get the selected stream using
  // getUserMedia().
  function chooseMediaAndGetStream() {
    function onPickerResult(id) {
      chrome.test.assertEq("string", typeof id);
      chrome.test.assertTrue(id != "");
      navigator.webkitGetUserMedia({
        audio: false,
        video: { mandatory: { chromeMediaSource: "desktop",
                              chromeMediaSourceId: id } }
      }, chrome.test.succeed, chrome.test.fail);
    }

    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window"], onPickerResult);
  },

  // Same as above but attempts to specify invalid source id.
  function chooseMediaAndTryGetStreamWithInvalidId() {
    function onPickerResult(id) {
      navigator.webkitGetUserMedia({
        audio: false,
        video: { mandatory: { chromeMediaSource: "desktop",
                              chromeMediaSourceId: id + "x" } }
      }, chrome.test.fail, chrome.test.succeed);
    }

    chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window"], onPickerResult);
  },

  function cancelDialog() {
    var requestId = chrome.desktopCapture.chooseDesktopMedia(
        ["screen", "window"],
        chrome.test.fail);
    chrome.test.assertEq("number", typeof requestId);
    chrome.desktopCapture.cancelChooseDesktopMedia(requestId);
    chrome.test.succeed();
  }
]);
