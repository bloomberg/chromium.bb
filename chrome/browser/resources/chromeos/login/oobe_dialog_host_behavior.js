// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'OobeDialogHostBehavior' is a behavior for oobe-dialog containers to
 * match oobe-dialog bahavior.
 */

/** @polymerBehavior */
var OobeDialogHostBehavior = {
  properties: {
    /**
     * True when dialog is displayed in full-screen mode.
     */
    fullScreenDialog: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  /**
   * Pass down fullScreenDialog attribute.
   */
  onBeforeShow: function() {
    var screens = Polymer.dom(this.root).querySelectorAll('oobe-dialog');
    for (var i = 0; i < screens.length; ++i) {
      if (this.fullScreenDialog)
        screens[i].fullScreenDialog = true;

      screens[i].onBeforeShow();
    }
  },
};

/**
 * TODO(alemate): Replace with an interface. b/24294625
 * @typedef {{
 *   onBeforeShow: function()
 * }}
 */
OobeDialogHostBehavior.Proto;
