// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for toast.
 */
camera.toast = camera.toast || {};

/**
 * Shows a toast message.
 * @param {string} message Message to be shown.
 */
camera.toast.show = function(message) {
  camera.toast.update_(message, false);
};

/**
 * Speaks a toast message.
 * @param {string} message Message to be spoken.
 */
camera.toast.speak = function(message) {
  camera.toast.update_(message, true);
};

/**
 * Updates the toast message.
 * @param {string} message Message to be updated.
 * @param {boolean} spoken Whether the toast is spoken only.
 * @private
 */
camera.toast.update_ = function(message, spoken) {
  // TTS speaks changes of on-screen aria-live elements. Force content changes
  // and clear content once inactive to avoid stale content being read out.
  var element = document.querySelector('#toast');
  camera.util.animateCancel(element); // Cancel the active toast if any.
  element.textContent = ''; // Force to reiterate repeated messages.
  element.textContent = chrome.i18n.getMessage(message) || message;

  element.classList.toggle('spoken', spoken);
  camera.util.animateOnce(element, () => element.textContent = '');
};
