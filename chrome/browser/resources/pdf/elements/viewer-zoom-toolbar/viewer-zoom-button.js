// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'viewer-zoom-button',

  behaviors: [
    Polymer.NeonAnimationRunnerBehavior
  ],

  properties: {
    /**
     * Icons to be displayed on the FAB. Multiple icons should be separated with
     * spaces, and will be cycled through every time the FAB is clicked.
     */
    icons: String,

    /**
     * Array version of the list of icons. Polymer does not allow array
     * properties to be set from HTML, so we must use a string property and
     * perform the conversion manually.
     * @private
     */
    icons_: {
      type: Array,
      value: [''],
      computed: 'computeIconsArray_(icons)'
    },

    tooltips: Array,

    opened: {
      type: Boolean,
      value: true
    },

    delay: Number,

    animationConfig: {
      type: Object,
      computed: 'computeAnimationConfig_(delay)'
    },

    /**
     * Index of the icon currently being displayed.
     */
    activeIndex: {
      type: Number,
      value: 0
    },

    /**
     * Icon currently being displayed on the FAB.
     * @private
     */
    visibleIcon_: {
      type: String,
      computed: 'computeVisibleIcon_(icons_, activeIndex)'
    },

    visibleTooltip_: {
      type: String,
      computed: 'computeVisibleTooltip_(tooltips, activeIndex)'
    }
  },

  computeAnimationConfig_: function(delay) {
    return {
      'entry': {
        name: 'transform-animation',
        node: this,
        timing: {
          easing: 'cubic-bezier(0, 0, 0.2, 1)',
          duration: 250,
          delay: delay
        },
        transformFrom: 'translateX(100%)'
      },
      'exit': {
        name: 'transform-animation',
        node: this,
        timing: {
          easing: 'cubic-bezier(0.4, 0, 1, 1)',
          duration: 250,
          delay: delay
        },
        transformTo: 'translateX(100%)'
      }
    };
  },

  computeIconsArray_: function(icons) {
    return icons.split(' ');
  },

  computeVisibleIcon_: function(icons, activeIndex) {
    return icons[activeIndex];
  },

  computeVisibleTooltip_: function(tooltips, activeIndex) {
    return tooltips[activeIndex];
  },

  listeners: {
    'neon-animation-finish': '_onAnimationFinished'
  },

  _onAnimationFinished: function() {
    this.style.transform = this.opened ? 'none' : 'translateX(100%)';
  },

  show: function() {
    if (!this.opened) {
      this.toggle_();
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

  fireClick: function() {
    // We cannot attach an on-click to the entire viewer-zoom-button, as this
    // will include clicks on the margins. Instead, proxy clicks on the FAB
    // through.
    this.fire('fabclick');

    this.activeIndex = (this.activeIndex + 1) % this.icons_.length;
  }
});
