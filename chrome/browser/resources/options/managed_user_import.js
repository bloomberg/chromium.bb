// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;
  var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * ManagedUserImportOverlay class.
   * Encapsulated handling of the 'Import existing managed user' overlay page.
   * @constructor
   * @class
   */
  function ManagedUserImportOverlay() {
    var title = loadTimeData.getString('managedUserImportTitle');
    OptionsPage.call(this, 'managedUserImport',
                     title, 'managed-user-import');
  };

  cr.addSingletonGetter(ManagedUserImportOverlay);

  ManagedUserImportOverlay.prototype = {
    // Inherit from OptionsPage.
    __proto__: OptionsPage.prototype,

    /** @override */
    canShowPage: function() {
      return !BrowserOptions.getCurrentProfile().isManaged;
    },

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      var managedUserList = $('managed-user-list');
      options.managedUserOptions.ManagedUserList.decorate(managedUserList);

      var avatarGrid = $('select-avatar-grid');
      options.ProfilesIconGrid.decorate(avatarGrid);
      var avatarIcons = loadTimeData.getValue('avatarIcons');
      avatarGrid.dataModel = new ArrayDataModel(avatarIcons);

      managedUserList.addEventListener('change', function(event) {
        var managedUser = managedUserList.selectedItem;
        if (!managedUser)
          return;

        $('managed-user-import-ok').disabled =
            managedUserList.selectedItem.onCurrentDevice;
      });

      var self = this;
      $('managed-user-import-cancel').onclick = function(event) {
        OptionsPage.closeOverlay();
        self.updateImportInProgress_(false);

        // 'cancelCreateProfile' is handled by BrowserOptionsHandler.
        chrome.send('cancelCreateProfile');
      };

      $('managed-user-import-ok').onclick =
          this.showAvatarGridOrSubmit_.bind(this);

      $('create-new-user-link').onclick = function(event) {
        OptionsPage.closeOverlay();
        OptionsPage.navigateToPage('createProfile');
      };
    },

    /**
     * @override
     */
    didShowPage: function() {
      chrome.send('requestExistingManagedUsers');

      this.updateImportInProgress_(false);
      $('managed-user-import-error-bubble').hidden = true;
      $('managed-user-import-ok').disabled = true;
      $('select-avatar-grid').hidden = true;
      $('managed-user-list').hidden = false;

      $('managed-user-import-ok').textContent =
          loadTimeData.getString('managedUserImportOk');
      $('managed-user-import-text').textContent =
          loadTimeData.getString('managedUserImportText');
      $('managed-user-import-title').textContent =
          loadTimeData.getString('managedUserImportTitle');
    },

    /**
     * Called when the user clicks the "OK" button. In case the managed
     * user being imported has no avatar in sync, it shows the avatar
     * icon grid. In case the avatar grid is visible or the managed user
     * already has an avatar stored in sync, it proceeds with importing
     * the managed user.
     * @private
     */
    showAvatarGridOrSubmit_: function() {
      var managedUser = $('managed-user-list').selectedItem;
      if (!managedUser)
        return;

      $('managed-user-import-error-bubble').hidden = true;

      if ($('select-avatar-grid').hidden && managedUser.needAvatar) {
        this.showAvatarGridHelper_();
        return;
      }

      var avatarUrl = managedUser.needAvatar ?
          $('select-avatar-grid').selectedItem : managedUser.iconURL;

      this.updateImportInProgress_(true);

      // 'createProfile' is handled by BrowserOptionsHandler.
      chrome.send('createProfile', [managedUser.name, avatarUrl,
                                    false, true, managedUser.id]);
    },

    /**
     * Hides the 'managed user list' and shows the avatar grid instead.
     * It also updates the overlay text and title to instruct the user
     * to choose an avatar for the supervised user.
     * @private
     */
    showAvatarGridHelper_: function() {
      $('managed-user-list').hidden = true;
      $('select-avatar-grid').hidden = false;
      $('select-avatar-grid').redraw();
      $('select-avatar-grid').selectedItem =
          loadTimeData.getValue('avatarIcons')[0];

      $('managed-user-import-ok').textContent =
          loadTimeData.getString('managedUserSelectAvatarOk');
      $('managed-user-import-text').textContent =
          loadTimeData.getString('managedUserSelectAvatarText');
      $('managed-user-import-title').textContent =
          loadTimeData.getString('managedUserSelectAvatarTitle');
    },

    /**
     * Updates the UI according to the importing state.
     * @param {boolean} inProgress True to indicate that
     *     importing is in progress and false otherwise.
     * @private
     */
    updateImportInProgress_: function(inProgress) {
      $('managed-user-import-ok').disabled = inProgress;
      $('managed-user-list').disabled = inProgress;
      $('select-avatar-grid').disabled = inProgress;
      $('create-new-user-link').disabled = inProgress;
      $('managed-user-import-throbber').hidden = !inProgress;
    },

    /**
     * Adds all the existing |managedUsers| to the list.
     * @param {Array.<Object>} managedUsers An array of managed user objects.
     *     Each object is of the form:
     *       managedUser = {
     *         id: "Managed User ID",
     *         name: "Managed User Name",
     *         iconURL: "chrome://path/to/icon/image",
     *         onCurrentDevice: true or false,
     *         needAvatar: true or false
     *       }
     * @private
     */
    receiveExistingManagedUsers_: function(managedUsers) {
      managedUsers.sort(function(a, b) {
        return a.name.localeCompare(b.name);
      });

      $('managed-user-list').dataModel = new ArrayDataModel(managedUsers);
      if (managedUsers.length == 0)
        this.onError_(loadTimeData.getString('noExistingManagedUsers'));
    },

    /**
     * Displays an error message if an error occurs while
     * importing a managed user.
     * Called by BrowserOptions via the BrowserOptionsHandler.
     * @param {string} error The error message to display.
     * @private
     */
    onError_: function(error) {
      var errorBubble = $('managed-user-import-error-bubble');
      errorBubble.hidden = false;
      errorBubble.textContent = error;
      this.updateImportInProgress_(false);
    },

    /**
     * Closes the overlay if importing the managed user was successful.
     * @private
     */
    onSuccess_: function() {
      this.updateImportInProgress_(false);
      OptionsPage.closeOverlay();
    },
  };

  // Forward public APIs to private implementations.
  [
    'onError',
    'onSuccess',
    'receiveExistingManagedUsers',
  ].forEach(function(name) {
    ManagedUserImportOverlay[name] = function() {
      var instance = ManagedUserImportOverlay.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    ManagedUserImportOverlay: ManagedUserImportOverlay,
  };
});
