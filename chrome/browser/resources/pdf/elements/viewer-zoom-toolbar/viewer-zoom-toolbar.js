// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

ANIMATION_INTERVAL = 50;

Polymer({
  is: 'viewer-zoom-toolbar',

  properties: {
   /**
     * The default zoom percentage.
     */
    zoomValue: {
      type: Number,
      value: 100
    }
  },

  get visible() {
    return this.visible_;
  },

  ready: function() {
    this.visible_ = true;
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

  zoomIn: function() {
    this.fire('zoom-in');
  },

  zoomOut: function() {
    this.fire('zoom-out');
  },

  show: function() {
    if (!this.visible) {
      this.visible_ = true;
      this.$['fit-to-width-button'].show();
      this.$['fit-to-page-button'].show();
      this.$['zoom-in-button'].show();
      this.$['zoom-out-button'].show();
    }
  },

  hide: function() {
    if (this.visible) {
      this.visible_ = false;
      this.$['fit-to-page-button'].hide();
      this.$['fit-to-width-button'].hide();
      this.$['zoom-in-button'].hide();
      this.$['zoom-out-button'].hide();
    }
  },
});
