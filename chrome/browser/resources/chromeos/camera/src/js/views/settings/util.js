// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Namespace for settings views.
 */
cca.views.settings = cca.views.settings || {};

/**
 * Namespace for settings views' utilities.
 */
cca.views.settings.util = cca.views.settings.util || {};

/**
 * Sets up the menu header and items.
 * @param {cca.views.View} view View whose menu items to be set up.
 * @param {Object.<string|function(Event=)>} itemHandlers Click-handlers
 *     mapped by menu item ids.
 */
cca.views.settings.util.setupMenu = function(view, itemHandlers) {
  view.root.querySelector('.menu-header button').addEventListener(
      'click', () => view.leave());

  view.root.querySelectorAll('.menu-item').forEach((wrapper) => {
    var wrapped = wrapper.querySelector('button, input');
    wrapped.addEventListener('click', (event) => {
      var handler = itemHandlers[wrapper.id];
      if (handler) {
        handler(event);
      }
      event.stopPropagation(); // Don't trigger another click in the wrapper.
    });
    wrapper.addEventListener('click', (event) => wrapped.click());
  });
};
