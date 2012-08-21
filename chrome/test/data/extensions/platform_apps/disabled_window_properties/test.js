// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: Some of the test code was put in the global scope on purpose!

function useToolbarGetter() {
  var result;
  try {
    // The following will invoke the window.toolbar getter, and this may or may
    // not throw an exception.
    result = {object: window.toolbar};
  } catch (e) {
    result = {exception: e};
  }
  return result;
}

var resultFromToolbarGetterAtStart = useToolbarGetter();

// The following statement implicitly invokes the window.toolbar setter.  This
// should delete the "disabler" getter and setter that were set up in
// chrome/renderer/resources/extensions/platform_app.js, and restore normal
// getter/setter behaviors from here on.
var toolbar = {blah: 'glarf'};

var resultFromToolbarGetterAfterRedefinition = useToolbarGetter();
var toolbarIsWindowToolbarAfterRedefinition = (toolbar === window.toolbar);

toolbar.blah = 'baz';

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.assertTrue(
      resultFromToolbarGetterAtStart.hasOwnProperty('exception'));

  chrome.test.assertTrue(
      resultFromToolbarGetterAfterRedefinition.hasOwnProperty('object'));
  chrome.test.assertTrue(toolbarIsWindowToolbarAfterRedefinition);

  chrome.test.assertEq('baz', toolbar.blah);

  chrome.test.notifyPass();
});
