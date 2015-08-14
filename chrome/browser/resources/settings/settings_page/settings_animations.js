// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This file defines a set of common animations used by
 * 'settings-animated-pages'.
 */
Polymer({
  is: 'settings-fade-in-animation',

  behaviors: [
    Polymer.NeonAnimationBehavior
  ],

  configure: function(config) {
    var node = config.node;

    var timing = this.timingFromConfig(config);
    timing.delay = 150;
    timing.duration = 150;

    this._effect = new KeyframeEffect(node, [
      {'opacity': '0'},
      {'opacity': '1'}
    ], timing);
    return this._effect;
  }
});

Polymer({
  is: 'settings-fade-out-animation',

  behaviors: [
    Polymer.NeonAnimationBehavior
  ],

  configure: function(config) {
    var node = config.node;

    var timing = this.timingFromConfig(config);
    timing.duration = 150;

    this._effect = new KeyframeEffect(node, [
      {'opacity': '1'},
      {'opacity': '0'}
    ], timing);
    return this._effect;
  }
});
