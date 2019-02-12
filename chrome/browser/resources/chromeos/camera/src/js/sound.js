// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for sound.
 */
cca.sound = cca.sound || {};

/**
 * Plays a sound.
 * @param {string} selector Selector of the sound.
 */
cca.sound.play = function(selector) {
  // TODO(yuli): Don't play sounds if the speaker settings is muted.
  var element = document.querySelector(selector);
  element.currentTime = 0;
  element.play();
  return true;
};
