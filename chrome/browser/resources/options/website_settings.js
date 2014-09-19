// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.ContentSettings', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  // Lookup table to generate the i18n strings.
  /** @const */ var permissionsLookup = {
    'geolocation': 'location',
    'notifications': 'notifications',
    'media-stream': 'mediaStream',
    'cookies': 'cookies',
    'multiple-automatic-downloads': 'multipleAutomaticDownloads',
    'images': 'images',
    'plugins': 'plugins',
    'popups': 'popups',
    'javascript': 'javascript',
    'battery': 'battery',
    'storage': 'storage'
  };

  /////////////////////////////////////////////////////////////////////////////
  // WebsiteSettingsManager class:

  /**
   * Encapsulated handling of the website settings page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function WebsiteSettingsManager() {
    Page.call(this, 'websiteSettings',
              loadTimeData.getString('websitesOptionsPageTabTitle'),
              'website-settings-page');
  }

  cr.addSingletonGetter(WebsiteSettingsManager);

  WebsiteSettingsManager.prototype = {
    __proto__: Page.prototype,

    /**
     * The saved allowed origins list.
     * @type {options.OriginList}
     * @private
     */
    allowedList_: null,

    /**
     * The saved blocked origins list.
     * @type {options.OriginList}
     * @private
     */
    blockedList_: null,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('website-settings-overlay-confirm').onclick =
          PageManager.closeOverlay.bind(PageManager);

      $('global-setting').onchange = function(event) {
        chrome.send('setDefaultContentSetting', [this.value]);
      };

      $('global-setting-toggle').onchange = function(event) {
        var value = event.target.checked;
        chrome.send('setGlobalEnabled', [value]);
      };

      var searchBox = $('website-settings-search-box');
      searchBox.addEventListener('search',
                                 this.handleSearchQueryChange_.bind(this));

      searchBox.onkeydown = function(e) {
        if (e.keyIdentifier == 'Enter')
          e.preventDefault();
      };

      this.createOriginsList_();
      this.updatePage_('geolocation');
    },

    /**
     * Called after the page has been shown. Show the content settings or
     * resource auditing for the location's hash.
     */
    didShowPage: function() {
      var hash = this.hash;
      if (hash)
        hash = hash.slice(1);
      else
        hash = 'geolocation';
      this.updatePage_(hash);
    },

    /**
     * Creates, decorates and initializes the origin list.
     * @private
     */
    createOriginsList_: function() {
      var allowedList = $('allowed-origin-list');
      options.OriginList.decorate(allowedList);
      this.allowedList_ = assertInstanceof(allowedList, options.OriginList);
      this.allowedList_.autoExpands = true;

      var blockedList = $('blocked-origin-list');
      options.OriginList.decorate(blockedList);
      this.blockedList_ = assertInstanceof(blockedList, options.OriginList);
      this.blockedList_.autoExpands = true;
    },

    /**
     * Populate an origin list with all of the origins with a given permission
     * or that are using a given resource.
     * @param {options.OriginList} originList A list to populate.
     * @param {!Object} originDict A dictionary of origins to their usage, which
           will be used to sort the origins.
     * @private
     */
    populateOriginsHelper_: function(originList, originDict) {
      var origins = Object.keys(originDict).map(function(origin) {
        // |usage| means the time of last usage in seconds since epoch
        // (Jan 1, 1970) for permissions and means the amount of local storage
        // in bytes used for local storage.
        return {
          origin: origin,
          usage: originDict[origin].usage,
          usageString: originDict[origin].usageString,
          readableName: originDict[origin].readableName,
        };
      });
      origins.sort(function(first, second) {
        return second.usage - first.usage;
      });
      originList.dataModel = new ArrayDataModel(origins);
    },


    /**
     * Populate the origin lists with all of the origins with a given permission
     * or that are using a given resource, potentially split by if allowed or
     * denied. If no blocked dictionary is provided, only the allowed list is
     * shown.
     * @param {!Object} allowedDict A dictionary of origins to their usage,
           which will be used to sort the origins in the main/allowed list.
     * @param {!Object} blockedDict An optional dictionary of origins to their
           usage, which will be used to sort the origins in the blocked list.
     * @param {bool} isGloballyEnabled If the content setting is turned on.
     * @private
     */
    populateOrigins: function(allowedDict, blockedDict, isGloballyEnabled) {
      this.populateOriginsHelper_(this.allowedList_, allowedDict);
      if (blockedDict) {
        this.populateOriginsHelper_(this.blockedList_, blockedDict);
        this.blockedList_.hidden = false;
        $('blocked-origin-list-title').hidden = false;
        this.allowedList_.classList.remove('nonsplit-origin-list');
      } else {
        this.blockedList_.hidden = true;
        $('blocked-origin-list-title').hidden = true;
        $('allowed-origin-list-title').hidden = true;
        this.allowedList_.classList.add('nonsplit-origin-list');
      }
      $('global-setting-toggle').checked = isGloballyEnabled;
    },

    /**
     * Update the table with the origins filtered by the value in the search
     * box.
     * @private
     */
    searchOrigins: function() {
      var filter = $('website-settings-search-box').value;
      chrome.send('updateOriginsSearchResults', [filter]);
    },

    /**
     * Handle and delay search query changes.
     * @param {!Event} e The event object.
     * @private
     */
    handleSearchQueryChange_: function(e) {
      if (this.queryDelayTimerId_)
        window.clearTimeout(this.queryDelayTimerId_);

      this.queryDelayTimerId_ = window.setTimeout(this.searchOrigins.bind(this),
                                                  160);
    },

    /**
     * Sets the default content setting dropdown on the page to the current
     * default.
     * @param {!Object} dict A dictionary with the management and value of the
     *     default setting for the last selected content setting..
     */
    updateDefault: function(dict) {
      // TODO(dhnishi): Remove duplicate default handling in the Content
      //     Settings page and here.
      var managedBy = dict.managedBy;
      var controlledBy = managedBy == 'policy' || managedBy == 'extension' ?
                managedBy : null;
      $('global-setting').disabled = (managedBy != 'default');

      var options = $('global-setting').options;
      for (var i = 0; i < options.length; i++) {
        if (options[i].value == dict.value) {
          options.selectedIndex = i;
        }
      }
    },

    /**
     * Updates the page with the given content setting or resource name's
     * information.
     * @param {string} typeName The name of the content setting or resource.
     */
    updatePage_: function(typeName) {
      if (typeName == 'storage')
        chrome.send('updateLocalStorage');
      else if (typeName == 'battery')
        chrome.send('updateBatteryUsage');
      else
        chrome.send('updateOrigins', [typeName]);

      var options = $('global-setting').options;
      options.length = 0;
      var permissionString = permissionsLookup[typeName];
      var permissions = ['Allow', 'Ask', 'Block'];
      for (var i = 0; i < permissions.length; i++) {
        var valueId = permissionString + permissions[i];
        if (loadTimeData.valueExists(valueId)) {
          options.add(new Option(loadTimeData.getString(valueId),
              permissions[i].toLowerCase()));
        }
      }
      if (options.length == 0) {
        $('website-settings-global-controls').hidden = true;
      } else {
        $('website-settings-global-controls').hidden = false;
        chrome.send('updateDefaultSetting');
      }

      $('website-settings-title').textContent =
          loadTimeData.getString(permissionString + 'TabLabel');
    }
  };

  WebsiteSettingsManager.populateOrigins = function(allowedDict, blockedDict,
      isGloballyEnabled) {
    WebsiteSettingsManager.getInstance().populateOrigins(allowedDict,
        blockedDict, isGloballyEnabled);
  };

  WebsiteSettingsManager.updateDefault = function(dict) {
    WebsiteSettingsManager.getInstance().updateDefault(dict);
  };

  WebsiteSettingsManager.showWebsiteSettings = function(hash) {
    PageManager.showPageByName('websiteSettings', true, {hash: '#' + hash});
  };

  // Export
  return {
    WebsiteSettingsManager: WebsiteSettingsManager
  };
});
