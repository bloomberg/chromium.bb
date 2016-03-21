// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-dialog' is a component for showing a modal dialog.
 */
Polymer({
  is: 'settings-dialog',

  properties: {
    /** @override */
    noCancelOnOutsideClick: {
      type: Boolean,
      value: true,
    },

    /** @override */
    noCancelOnEscKey: {
      type: Boolean,
      value: false,
    },

    /** @override */
    withBackdrop: {
      type: Boolean,
      value: true,
    },
  },

  behaviors: [Polymer.PaperDialogBehavior],

  /** @return {!PaperIconButtonElement} */
  getCloseButton: function() {
    return this.$.close;
  },
});
