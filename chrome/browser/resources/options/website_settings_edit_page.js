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
   */
  function WebsiteSettingsEditor() {
    Page.call(this, 'websiteEdit',
                     loadTimeData.getString('websitesOptionsPageTabTitle'),
                     'website-settings-edit-page');
    this.permissions = ['geolocation', 'notifications', 'media-stream'];
  }

  cr.addSingletonGetter(WebsiteSettingsEditor);

  WebsiteSettingsEditor.prototype = {
    __proto__: Page.prototype,


    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('websiteSettingsEditorCancelButton').onclick =
          PageManager.closeOverlay.bind(PageManager);

      $('websiteSettingsEditorDoneButton').onclick = function(event) {
        WebsiteSettingsEditor.getInstance().updatePermissions();
        PageManager.closeOverlay.bind(PageManager)();
      };
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
     * @param {string} local_storage A string describing the local storage use.
     * @param {Object} permissions A dictionary of permissions to their
     *     available and current settings, and if it is editable.
     * @private
     */
    populateOrigin_: function(local_storage, permissions) {
      $('local-storage-title').textContent = local_storage;
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
  };

  WebsiteSettingsEditor.populateOrigin = function(local_storage, permissions) {
    WebsiteSettingsEditor.getInstance().populateOrigin_(local_storage,
                                                        permissions);
  };

  // Export
  return {
    WebsiteSettingsEditor: WebsiteSettingsEditor
  };

});
