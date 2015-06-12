// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'viewer-zoom-button',

  behaviors: [
    Polymer.NeonAnimationRunnerBehavior
  ],

  properties: {
    icon: String,

    opened: {
      type: Boolean,
      value: true
    },

    delay: Number,

    animationConfig: {
      type: Object,
      computed: 'computeAnimationConfig(delay)'
    }
  },

  computeAnimationConfig: function(delay) {
    return {
      'entry': {
        name: 'transform-animation',
        node: this,
        timing: {
          easing: 'cubic-bezier(0, 0, 0.2, 1)',
          duration: 250,
          delay: delay
        },
        transformFrom: 'translateX(150%)'
      },
      'exit': {
        name: 'transform-animation',
        node: this,
        timing: {
          easing: 'cubic-bezier(0.4, 0, 1, 1)',
          duration: 250,
          delay: delay
        },
        transformTo: 'translateX(150%)'
      }
    };
  },

  listeners: {
    'neon-animation-finish': '_onAnimationFinished'
  },

  _onAnimationFinished: function() {
    // Must use visibility: hidden so that the buttons do not change layout as
    // they are hidden.
    if (!this.opened)
      this.style.visibility = 'hidden';
  },

  show: function() {
    if (!this.opened) {
      this.toggle_();
      this.style.visibility = 'initial';
    }
  },

  hide: function() {
    if (this.opened)
      this.toggle_();
  },

  toggle_: function() {
    this.opened = !this.opened;
    this.cancelAnimation();
    this.playAnimation(this.opened ? 'entry' : 'exit');
  },
});
