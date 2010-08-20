// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /**
   * PasswordsRemoveAllOverlay class
   * Encapsulated handling of the 'Remove All' overlay page.
   * @class
   */
  function PasswordsRemoveAllOverlay() {
    OptionsPage.call(this, 'passwordsRemoveAllOverlay',
                     templateData.remove_all_title,
                     'passwordsRemoveAllOverlay');
  }

  cr.addSingletonGetter(PasswordsRemoveAllOverlay);

  PasswordsRemoveAllOverlay.prototype = {
    // Inherit StopSyncingOverlay from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      $('remove-all-cancel').onclick = function(e) {
        OptionsPage.clearOverlays();
      }

      $('remove-all-confirm').onclick = function(e) {
        PasswordsExceptions.removeAllPasswords();
        OptionsPage.clearOverlays();
      }
    }
  };

  // Export
  return {
    PasswordsRemoveAllOverlay: PasswordsRemoveAllOverlay
  };

});

