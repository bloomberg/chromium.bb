// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * ManagedUserCreateConfirm class.
   * Encapsulated handling of the confirmation overlay page when creating a
   * managed user.
   * @constructor
   * @class
   */
  function ManagedUserCreateConfirmOverlay() {
    OptionsPage.call(this, 'managedUserCreateConfirm',
                     '',  // The title will be based on the new profile name.
                     'managed-user-create-confirm');
  };

  cr.addSingletonGetter(ManagedUserCreateConfirmOverlay);

  ManagedUserCreateConfirmOverlay.prototype = {
    // Inherit from OptionsPage.
    __proto__: OptionsPage.prototype,

    // Info about the newly created profile.
    profileInfo_: null,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('managed-user-create-confirm-done').onclick = function(event) {
        OptionsPage.closeOverlay();
      };

      $('managed-user-create-confirm-switch').onclick = function(event) {
        OptionsPage.closeOverlay();
        chrome.send('switchToProfile', [self.profileInfo_.filePath]);
      };
    },

    /**
     * Sets the profile info used in the dialog and updates the profile name
     * displayed. Called by the profile creation overlay when this overlay is
     * opened.
     * @param {Object} profileInfo An object of the form:
     *     profileInfo = {
     *       name: "Profile Name",
     *       filePath: "/path/to/profile/data/on/disk"
     *       isManaged: (true|false),
     *     };
     * @private
     */
    setProfileInfo_: function(info) {
      self.profileInfo_ = info;
      $('managed-user-create-confirm-title').textContent =
          loadTimeData.getStringF('managedUserCreateConfirmTitle', info.name);
      $('managed-user-create-confirm-switch').textContent =
          loadTimeData.getStringF('managedUserCreateConfirmSwitch', info.name);
    },
  };

  // Forward public APIs to private implementations.
  [
    'setProfileInfo',
  ].forEach(function(name) {
    ManagedUserCreateConfirmOverlay[name] = function() {
      var instance = ManagedUserCreateConfirmOverlay.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    ManagedUserCreateConfirmOverlay: ManagedUserCreateConfirmOverlay,
  };
});
