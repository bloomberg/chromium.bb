/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 **/

/**
 * Listens for the app launching then creates the window.
 *
 * @see http://developer.chrome.com/trunk/apps/app.window.html
 */
chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('calculator.html', {
    defaultWidth: 243, minWidth: 243, maxWidth: 243,
    defaultHeight: 380, minHeight: 380, maxHeight: 380,
    id: 'calculator'
  }, function (appWindow) {
    var window = appWindow.contentWindow;
    window.onload = function (window) {
      new View(window, new Model(8));
    }.bind(this, window);
  });
});
