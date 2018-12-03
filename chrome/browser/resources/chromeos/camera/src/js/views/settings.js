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
 * Creates the settings-view controller.
 * @extends {cca.views.View}
 * @constructor
 */
cca.views.Settings = function() {
  cca.views.View.call(this, '#settings', true, true);

  // End of properties, seal the object.
  Object.seal(this);

  document.querySelector('#settings-back').addEventListener(
      'click', () => this.leave());

  // Add event listeners for menu items.
  var items = [
    ['#settings-help', 'onHelpClicked_'],
  ];
  items.forEach(([selector, fn]) => {
    var element = document.querySelector(selector);
    var button = element.querySelector('button');
    button.addEventListener('click', (event) => {
      this[fn]();
      event.stopPropagation();
    });
    element.addEventListener('click', (event) => button.click());
  });
};

cca.views.Settings.prototype = {
  __proto__: cca.views.View.prototype,
};

/**
 * Handles clicking on the help button.
 * @param {Event} event Click event.
 * @private
 */
cca.views.Settings.prototype.onHelpClicked_ = function(event) {
  window.open('https://support.google.com/chromebook/answer/4487486');
};
