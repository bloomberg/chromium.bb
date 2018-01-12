// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var frame;
var frameRuntime;
var frameStorage;
var frameTabs;

chrome.test.runTests([
  function createFrame() {
    frame = document.createElement('iframe');
    frame.src = chrome.runtime.getURL('frame.html');
    frame.onload = chrome.test.succeed;
    document.body.appendChild(frame);
  },
  function useFrameStorageAndRuntime() {
    frameRuntime = frame.contentWindow.chrome.runtime;
    chrome.test.assertTrue(!!frameRuntime);
    frameStorage = frame.contentWindow.chrome.storage.local;
    chrome.test.assertTrue(!!frameStorage);
    frameTabs = frame.contentWindow.chrome.tabs;
    chrome.test.assertTrue(!!frameTabs);
    chrome.test.assertEq(chrome.runtime.getURL('background.js'),
                         frameRuntime.getURL('background.js'));
    frameStorage.set({foo: 'bar'}, function() {
      chrome.test.assertFalse(!!chrome.runtime.lastError);
      chrome.test.assertFalse(!!frameRuntime.lastError);
      chrome.storage.local.get('foo', function(vals) {
        chrome.test.assertFalse(!!chrome.runtime.lastError);
        chrome.test.assertFalse(!!frameRuntime.lastError);
        chrome.test.assertEq('bar', vals.foo);
        chrome.test.succeed();
      });
    });
  },
  function removeFrameAndUseStorageAndRuntime() {
    document.body.removeChild(frame);
    try {
      frameStorage.set({foo: 'baz'});
    } catch (e) {}

    try {
      let url = frameRuntime.getURL('background.js');
    } catch (e) {}

    chrome.test.succeed();
  },
]);
