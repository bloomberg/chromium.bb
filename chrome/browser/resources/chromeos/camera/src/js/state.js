// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for the app state.
 */
cca.state = cca.state || {};

/**
 * Checks if the specified state exists.
 * @param {string} state State to be checked.
 * @return {boolean} Whether the state exists.
 */
cca.state.get = function(state) {
  return document.body.classList.contains(state);
};

/**
 * Sets the specified state on or off.
 * @param {string} state State to be set.
 * @param {boolean} val True to set the state on, false otherwise.
 */
cca.state.set = function(state, val) {
  document.body.classList.toggle(state, val);
};
