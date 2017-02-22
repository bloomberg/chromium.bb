// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-dialog',

  properties: {
    /**
     * Controls visibility of the bottom-buttons element.
     */
    hasButtons: {
      type: Boolean,
      value: false,
    },

    /**
     * Switches styles to "Welcome screen".
     */
    welcomeScreen: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  focus: function() {
    /* When Network Selection Dialog is shown because user pressed "Back"
       button on EULA screen, display_manager does not inform this dialog that
       it is shown. It ouly focuses this dialog.
       So this emulates show().
       TODO (alemate): fix this once event flow is updated.
    */
    this.show();
  },

  /**
    * This is called from oobe_welcome when this dialog is shown.
    */
  show: function() {
    var focusedElements = this.getElementsByClassName('focus-on-show');
    if (focusedElements)
      focusedElements[0].focus();

    this.fire('show-dialog');
  },
});
