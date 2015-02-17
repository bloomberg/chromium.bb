// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

ANIMATION_INTERVAL = 50;

Polymer('viewer-zoom-selector', {
  /**
   * @type {number}
   * The minimum zoom percentage allowed.
   */
  zoomMin: 25,

  /**
   * @type {number}
   * The maximum zoom percentage allowed.
   */
  zoomMax: 500,

  /**
   * @type {number}
   * The default zoom percentage.
   */
  zoomValue: 100,

  get visible() {
    return this.visible_;
  },

  ready: function() {
    this.visible_ = false;
  },

  zoomValueChanged: function() {
    this.fire('zoom', { zoom: this.zoomValue / 100 });
  },

  fitToPage: function() {
    this.fire('fit-to-page');
  },

  fitToWidth: function() {
    this.fire('fit-to-width');
  },

  show: function() {
    if (!this.visible) {
      this.visible_ = true;
      this.$['fit-to-width-button'].show();
      this.$['fit-to-page-button'].show(ANIMATION_INTERVAL);
      this.$.slider.show();
    }
  },

  hide: function() {
    if (this.visible) {
      this.visible_ = false;
      this.$.slider.hide();
      this.$['fit-to-page-button'].hide();
      this.$['fit-to-width-button'].hide(ANIMATION_INTERVAL);
    }
  },
});
