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
    var title = loadTimeData.getString('managedUserImportTabTitle');
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

      managedUserList.addEventListener('change', function(event) {
        var managedUser = managedUserList.selectedItem;
        if (!managedUser)
          return;

        $('managed-user-import-ok').disabled =
            managedUserList.selectedItem.onCurrentDevice;
      });

      $('managed-user-import-cancel').onclick = function(event) {
        OptionsPage.closeOverlay();
        // 'cancelCreateProfile' is handled by BrowserOptionsHandler.
        chrome.send('cancelCreateProfile');
      };

      $('managed-user-import-ok').onclick = function(event) {
        var managedUser = managedUserList.selectedItem;
        if (!managedUser)
          return;

        $('managed-user-import-error-bubble').hidden = true;
        $('managed-user-import-ok').disabled = true;

        // 'createProfile' is handled by BrowserOptionsHandler.
        chrome.send('createProfile',
            [managedUser.name, managedUser.iconURL,
             false, true, managedUser.id]);
      };

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
      $('managed-user-import-error-bubble').hidden = true;
      $('managed-user-import-ok').disabled = true;
    },

    /**
     * Adds all the existing |managedUsers| to the list.
     * @param {Array.<Object>} managedUsers An array of managed user objects.
     *     Each object is of the form:
     *       managedUser = {
     *         id: "Managed User ID",
     *         name: "Managed User Name",
     *         iconURL: "chrome://path/to/icon/image",
     *         onCurrentDevice: true or false
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
    },

    /**
     * Closes the overlay if importing the managed user was successful.
     * @private
     */
    onSuccess_: function() {
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
