// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Global wallpaperManager reference useful for poking at from the console.
*/
var wallpaperManager;

function init() {
  window.addEventListener('load', function() {
      chrome.wallpaperPrivate.minimizeInactiveWindows(); });
  window.addEventListener('unload', function() {
      chrome.wallpaperPrivate.restoreMinimizedWindows(); });
  WallpaperManager.initStrings(function() {
    wallpaperManager = new WallpaperManager(document.body);
  });
}

document.addEventListener('DOMContentLoaded', init);
