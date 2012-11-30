// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var WALLPAPER_PICKER_WIDTH = 550;
var WALLPAPER_PICKER_HEIGHT = 420;

var wallpaperPickerWindow;

chrome.app.runtime.onLaunched.addListener(function() {
  if (wallpaperPickerWindow && !wallpaperPickerWindow.contentWindow.closed) {
    wallpaperPickerWindow.focus();
    chrome.wallpaperPrivate.minimizeInactiveWindows();
    return;
  }

  chrome.app.window.create('main.html', {
    frame: 'chrome',
    width: WALLPAPER_PICKER_WIDTH,
    height: WALLPAPER_PICKER_HEIGHT,
    minWidth: WALLPAPER_PICKER_WIDTH,
    maxWidth: WALLPAPER_PICKER_WIDTH,
    minHeight: WALLPAPER_PICKER_HEIGHT,
    maxHeight: WALLPAPER_PICKER_HEIGHT
  }, function(w) {
    wallpaperPickerWindow = w;
    chrome.wallpaperPrivate.minimizeInactiveWindows();
    w.onClosed.addListener(function() {
      chrome.wallpaperPrivate.restoreMinimizedWindows();
    });
  });
});
