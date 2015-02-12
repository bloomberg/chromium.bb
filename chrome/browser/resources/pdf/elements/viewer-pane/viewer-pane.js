// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('viewer-pane', {
  /**
   * @type {string}
   * The heading to be used in the pane.
   */
  heading: '',

  /**
   * @type {boolean}
   * Whether the pane was opened by the user.
   */
  get openedByUser() {
    return this.openedByUser_;
  },

  /**
   * @type {boolean}
   * Whether the pane is currently visible.
   */
  get visible() {
    return this.visible_;
  },

  /**
   * Override openAction to prevent paper-dialog-base from scrolling back to
   * top.
   */
  openAction: function() {

  },

  ready: function() {
    this.super();
    this.openedByUser_ = false;
    this.visible_ = false;
  },

  /**
   * Makes the pane appear if it was opened by the user.
   * This is used if we need to show the pane but only if it was already opened.
   */
  showIfOpenedByUser: function() {
    if (this.openedByUser && !this.visible) {
      this.toggle();
      this.visible_ = true;
    }
  },

  /**
   * Hides the pane if it was opened by the user.
   * This is used if we need to hide the pane but remember that it was opened.
   */
  hideIfOpenedByUser: function() {
    if (this.openedByUser && this.visible) {
      this.toggle();
      this.visible_ = false;
    }
  },

  buttonToggle: function() {
    this.openedByUser_ = !this.openedByUser_;
    this.visible_ = this.openedByUser_;
    this.toggle();
  },
});
