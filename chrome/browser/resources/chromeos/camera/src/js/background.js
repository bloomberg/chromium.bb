// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for the background page.
 */
camera.bg = {};

/**
 * Singleton window handle of the Camera app.
 * @type {AppWindow}
 */
camera.bg.appWindow = null;

/**
 * Default width of the window in pixels.
 * @type {number}
 * @const
 */
camera.bg.DEFAULT_WIDTH = 640;

/**
 * Default height of the window in pixels.
 * @type {number}
 * @const
 */
camera.bg.DEFAULT_HEIGHT = 360;

/**
 * Top bar color of the window.
 * @type {string}
 * @const
 */
camera.bg.TOPBAR_COLOR = "#1E2023";

/**
 * Creates the window. Note, that only one window at once is supported.
 */
camera.bg.create = function() {
  var maximized = false;
  var fullscreen = false;
  chrome.storage.local.get(['maximized', 'fullscreen'], function(result) {
    if (!chrome.runtime.lastError) {
      if (result.maximized)
        maximized = result.maximized;
      if (result.fullscreen)
        fullscreen = result.fullscreen;
    }
  });
  chrome.app.window.create('views/main.html', {
    id: 'main',
    frame: {
      color: camera.bg.TOPBAR_COLOR
    },
    hidden: true,  // Will be shown from main.js once loaded.
    outerBounds: {
      width: camera.bg.DEFAULT_WIDTH,
      height: camera.bg.DEFAULT_HEIGHT,
      left: Math.round(
          (window.screen.availWidth - camera.bg.DEFAULT_WIDTH) / 2),
      top: Math.round(
          (window.screen.availHeight - camera.bg.DEFAULT_HEIGHT) / 2)
    },
  }, function(inAppWindow) {
    // Temporary workaround for crbug.com/452737.
    // 'state' option in CreateWindowOptions is ignored when a window is
    // launched with 'hidden' option, so we maintain window state manually here.
    if (maximized)
      inAppWindow.maximize();
    if (fullscreen)
      inAppWindow.fullscreen();
    inAppWindow.onClosed.addListener(function() {
      chrome.storage.local.set({maximized: inAppWindow.isMaximized()});
      chrome.storage.local.set({fullscreen: inAppWindow.isFullscreen()});
    });
    camera.bg.appWindow = inAppWindow;
  });
};

/**
 * Creates the window for tests. If renamed, the Camera's autotest must be
 * updated too.
 */
camera.bg.createForTesting = function() {
  camera.bg.create();
};

chrome.app.runtime.onLaunched.addListener(camera.bg.create);
