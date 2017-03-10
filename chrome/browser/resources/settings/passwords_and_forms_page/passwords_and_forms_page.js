// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-passwords-and-forms-page' is the settings page
 * for passwords and auto fill.
 */

(function() {
'use strict';

Polymer({
  is: 'settings-passwords-and-forms-page',

  behaviors: [PrefsBehavior],

  properties: {
    /** @private Filter applied to passwords and password exceptions. */
    passwordFilter_: String,
  },

  /**
   * Shows the manage autofill sub page.
   * @param {!Event} event
   * @private
   */
  onAutofillTap_: function(event) {
    // Ignore clicking on the toggle button and verify autofill is enabled.
    if (Polymer.dom(event).localTarget != this.$.autofillToggle &&
        this.getPref('autofill.enabled').value) {
      settings.navigateTo(settings.Route.AUTOFILL);
    }
  },

  /**
   * Shows the manage passwords sub page.
   * @param {!Event} event
   * @private
   */
  onPasswordsTap_: function(event) {
    // Ignore clicking on the toggle button and only expand if the manager is
    // enabled.
    if (Polymer.dom(event).localTarget != this.$.passwordToggle &&
        this.getPref('credentials_enable_service').value) {
      settings.navigateTo(settings.Route.MANAGE_PASSWORDS);
    }
  },

  /**
   * Return "secondary-action" to be used as the dom-class name if the toggle
   * is on.
   * @private
   * @param {boolean} isToggleOn whether or not the associated toggle is on.
   */
  secondaryActionClass_: function(isToggleOn) {
    // TODO(scottchen): Having to do this is kind of annoying, but it'll be a
    // temporary patch. As we have plans to move the toggle completely into the
    // subpages themselves, this code *should* be eliminated soon.
    return isToggleOn ? 'secondary-action' : '';
  },
});
})();
