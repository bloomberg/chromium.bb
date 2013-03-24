// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview App install/launch splash screen.
 */

/**
 * Initializes the click handler.
 */
initialize = function() {
  var query = window.location.search;

  var appId = '';
  if (query.substr(1, 4) == 'app=')
    appId = query.substr(5);

  chrome.send('initialize', [appId]);

  $('shortcut-info').hidden = !loadTimeData.getBoolean('shortcutEnabled');
  $('page').style.opacity = 1;
  window.webkitRequestAnimationFrame(function() {});
};

updateApp = function(app) {
  $('header').textContent = app.name;
  $('header').style.backgroundImage = 'url(' + app.iconURL + ')';
};

updateMessage = function(message) {
  $('launch-text').textContent = message;
};

disableTextSelectAndDrag();
document.addEventListener('DOMContentLoaded', initialize);
