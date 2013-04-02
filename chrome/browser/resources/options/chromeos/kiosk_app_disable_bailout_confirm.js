// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  /**
   * A confirmation overlay for disabling kiosk app bailout shortcut.
   * @extends {options.OptionsPage}
   * @constructor
   */
  function KioskDisableBailoutConfirm() {
    OptionsPage.call(this,
                     'kioskDisableBailoutConfirm',
                     '',
                     'kiosk-disable-bailout-confirm-overlay');
  }

  cr.addSingletonGetter(KioskDisableBailoutConfirm);

  KioskDisableBailoutConfirm.prototype = {
    __proto__: OptionsPage.prototype,

    /** @override */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var el = $('kiosk-disable-bailout-shortcut');
      el.customChangeHandler = this.handleDisableBailoutShortcutChange_;

      $('kiosk-disable-bailout-confirm-button').onclick = function(e) {
        OptionsPage.closeOverlay();
        Preferences.setBooleanPref(el.pref, true);
      };
      $('kiosk-disable-bailout-cancel-button').onclick = this.handleCancel;
    },

    /** @override */
    handleCancel: function() {
      OptionsPage.closeOverlay();
      $('kiosk-disable-bailout-shortcut').checked = false;
    },

    /**
     * Custom change handler for the disable bailout shortcut checkbox.
     * It blocks the underlying pref being changed and brings up confirmation
     * alert to user.
     * @private
     */
    handleDisableBailoutShortcutChange_: function() {
      // Let default processing happening if user un-checks the box.
      if (!$('kiosk-disable-bailout-shortcut').checked)
        return false;

      // Otherwise, show the confirmation overlay.
      OptionsPage.showPageByName('kioskDisableBailoutConfirm', false);
      return true;
    }
  };

  // Export
  return {
    KioskDisableBailoutConfirm: KioskDisableBailoutConfirm
  };
});

