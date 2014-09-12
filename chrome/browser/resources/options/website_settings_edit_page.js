// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.WebsiteSettings', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;

  /////////////////////////////////////////////////////////////////////////////
  // WebsiteSettingsEditor class:

  /**
   * Encapsulated handling of the website settings editor page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function WebsiteSettingsEditor() {
    Page.call(this, 'websiteEdit',
                     loadTimeData.getString('websitesOptionsPageTabTitle'),
                     'website-settings-edit-page');
    this.permissions = ['geolocation', 'notifications', 'media-stream',
                        'cookies', 'multiple-automatic-downloads', 'images',
                        'plugins', 'popups', 'javascript'];
    this.permissionsLookup = {
      'geolocation': 'Location',
      'notifications': 'Notifications',
      'media-stream': 'MediaStream',
      'cookies': 'Cookies',
      'multiple-automatic-downloads': 'Downloads',
      'images': 'Images',
      'plugins': 'Plugins',
      'popups': 'Popups',
      'javascript': 'Javascript'
    };
  }

  cr.addSingletonGetter(WebsiteSettingsEditor);

  WebsiteSettingsEditor.prototype = {
    __proto__: Page.prototype,


    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('website-settings-storage-delete-button').onclick = function(event) {
        chrome.send('deleteLocalStorage');
      };

      $('website-settings-battery-stop-button').onclick = function(event) {
        chrome.send('stopOrigin');
      };

      $('websiteSettingsEditorCancelButton').onclick =
          PageManager.closeOverlay.bind(PageManager);

      $('websiteSettingsEditorDoneButton').onclick = function(event) {
        WebsiteSettingsEditor.getInstance().updatePermissions();
        PageManager.closeOverlay.bind(PageManager)();
      };

      var permissionList =
          this.pageDiv.querySelector('.origin-permission-list');
      for (var key in this.permissions) {
        permissionList.appendChild(
            this.makePermissionOption_(this.permissions[key]));
      }
    },

    /**
     * Populates the page with the proper information for a given URL.
     * @param {string} url The URL of the page.
     * @private
     */
    populatePage: function(url) {
        this.url = url;

        var titleEl = $('website-title');
        titleEl.textContent = url;
        titleEl.style.backgroundImage = getFaviconImageSet(url);

        chrome.send('getOriginInfo', [url]);
    },

    /**
     * Populates and displays the page with given origin information.
     * @param {string} localStorage A string describing the local storage use.
     * @param {string} batteryUsage A string describing the battery use.
     * @param {Object} permissions A dictionary of permissions to their
     *     available and current settings, and if it is editable.
     * @param {boolean} showPage If the page should raised.
     * @private
     */
    populateOrigin_: function(localStorage, batteryUsage, permissions,
        showPage) {
      $('local-storage-title').textContent = localStorage;
      $('battery-title').textContent = batteryUsage;
      for (var key in permissions) {
        var selector = $(key + '-select-option');

        var options = permissions[key].options;
        selector.options.length = 0;
        for (var option in options) {
          selector.options[selector.options.length] =
              new Option(loadTimeData.getString(options[option] + 'Exception'),
                  options[option]);
        }

        selector.value = permissions[key].setting;
        selector.originalValue = permissions[key].setting;
        selector.disabled = !permissions[key].editable;
      }
      if (showPage)
        PageManager.showPageByName('websiteEdit', false);
    },

    updatePermissions: function() {
      for (var key in this.permissions) {
        var selection = $(this.permissions[key] + '-select-option');
        if (selection.value != selection.originalValue) {
          chrome.send('setOriginPermission',
              [this.permissions[key], selection.value]);
        }
      }
    },

    /**
     * Populates the origin permission list with the different usable
     * permissions.
     * @param {string} permissionName A string with the permission name.
     * @return {Element} The element with the usable permission setting.
     */
    makePermissionOption_: function(permissionName) {
      var permissionOption = cr.doc.createElement('div');
      permissionOption.className = 'permission-option';

      var permissionNameSpan = cr.doc.createElement('span');
      permissionNameSpan.className = 'permission-name';
      permissionNameSpan.textContent = loadTimeData.getString('websites' +
          this.permissionsLookup[permissionName] + 'Description');
      permissionOption.appendChild(permissionNameSpan);

      var permissionSelector = cr.doc.createElement('select');
      permissionSelector.setAttribute('id', permissionName + '-select-option');
      permissionSelector.className = 'weaktrl permission-selection-option';
      permissionOption.appendChild(permissionSelector);
      return permissionOption;
    },
  };

  WebsiteSettingsEditor.populateOrigin = function(localStorage, batteryUsage,
      permissions, showPage) {
    WebsiteSettingsEditor.getInstance().populateOrigin_(localStorage,
                                                        batteryUsage,
                                                        permissions,
                                                        showPage);
  };

  WebsiteSettingsEditor.showEditPage = function(url) {
    WebsiteSettingsEditor.getInstance().populatePage(url);
  };

  // Export
  return {
    WebsiteSettingsEditor: WebsiteSettingsEditor
  };

});
