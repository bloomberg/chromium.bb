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
 * Creates the window. Note, that only one window at once is supported.
 */
camera.bg.create = function() {
  chrome.app.window.create('views/main.html', {
    id: 'main',
    frame: 'none',
    hidden: true,  // Will be shown from main.js once loaded.
    bounds: {
      width: camera.bg.DEFAULT_WIDTH,
      height: camera.bg.DEFAULT_HEIGHT,
      left: Math.round(
          (window.screen.availWidth - camera.bg.DEFAULT_WIDTH) / 2),
      top: Math.round(
          (window.screen.availHeight - camera.bg.DEFAULT_HEIGHT) / 2)
    },
  }, function(inAppWindow) {
    camera.bg.appWindow = inAppWindow;
  });
};

chrome.app.runtime.onLaunched.addListener(camera.bg.create);
