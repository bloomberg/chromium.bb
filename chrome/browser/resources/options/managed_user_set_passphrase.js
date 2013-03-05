// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  //////////////////////////////////////////////////////////////////////////////
  // ManagedUserSetPassphraseOverlay class:

  /**
   * Encapsulated handling of the Managed User Set Passphrase page.
   * @constructor
   */
  function ManagedUserSetPassphraseOverlay() {
    OptionsPage.call(
      this,
      'setPassphrase',
      loadTimeData.getString('setPassphraseTitle'),
      'managed-user-set-passphrase-overlay');
  }

  cr.addSingletonGetter(ManagedUserSetPassphraseOverlay);

  /** Closes the page and resets the passphrase fields */
  var closePage = function() {
    $('managed-user-passphrase').value = '';
    $('passphrase-confirm').value = '';
    OptionsPage.closeOverlay();
  };

  ManagedUserSetPassphraseOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    /** @override */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);
      $('managed-user-passphrase').oninput = this.updateDisplay_;
      $('passphrase-confirm').oninput = this.updateDisplay_;

      $('save-passphrase').onclick = function() {
        chrome.send('setPassphrase', [$('managed-user-passphrase').value]);
        closePage();
      };

      $('cancel-passphrase').onclick = closePage;
    },

    /** Updates the display according to the validity of the user input. */
    updateDisplay_: function() {
      var valid =
          $('passphrase-confirm').value == $('managed-user-passphrase').value;
      $('passphrase-mismatch').hidden = valid;
      $('passphrase-confirm').setCustomValidity(
          valid ? '' : $('passphrase-mismatch').textContent);
      $('save-passphrase').disabled =
          !$('passphrase-confirm').validity.valid ||
          !$('managed-user-passphrase').validity.valid;
    },

    /** @override */
    canShowPage: function() {
      return ManagedUserSettings.getInstance().authenticationState ==
          options.ManagedUserAuthentication.AUTHENTICATED;
    },
  };

  /** Used for browsertests. */
  var ManagedUserSetPassphraseForTesting = {
    getPassphraseInput: function() {
      return $('managed-user-passphrase');
    },
    getPassphraseConfirmInput: function() {
      return $('passphrase-confirm');
    },
    getSavePassphraseButton: function() {
      return $('save-passphrase');
    },
    updateDisplay: function() {
      return ManagedUserSetPassphraseOverlay.getInstance().updateDisplay_();
    }
  };

  // Export
  return {
    ManagedUserSetPassphraseOverlay: ManagedUserSetPassphraseOverlay,
    ManagedUserSetPassphraseForTesting: ManagedUserSetPassphraseForTesting
  };

});
