// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  /////////////////////////////////////////////////////////////////////////////
  // WebsiteSettingsManager class:

  /**
   * Encapsulated handling of the website settings page.
   * @constructor
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
     * @type {OriginList}
     * @private
     */
    allowedList_: null,

    /**
     * The saved blocked origins list.
     * @type {OriginList}
     * @private
     */
    blockedList_: null,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('website-settings-overlay-confirm').onclick =
          PageManager.closeOverlay.bind(PageManager);

      $('resourceType').onchange = function(event) {
        var target = event.target;
        assert(target.tagName == 'SELECT');
        if (target.value == 'storage')
          chrome.send('updateLocalStorage');
        else if (target.value == 'battery')
          chrome.send('updateBatteryUsage');
        else
          chrome.send('updateOrigins', [target.value]);
      };

      var searchBox = $('website-settings-search-box');
      searchBox.addEventListener('search',
                                 this.handleSearchQueryChange_.bind(this));

      searchBox.onkeydown = function(e) {
        if (e.keyIdentifier == 'Enter')
          e.preventDefault();
      };

      this.createOriginsList_();
      chrome.send('updateOrigins', ['geolocation']);
    },

    /**
     * Creates, decorates and initializes the origin list.
     * @private
     */
    createOriginsList_: function() {
      this.allowedList_ = $('allowed-origin-list');
      options.OriginList.decorate(this.allowedList_);
      this.allowedList_.autoExpands = true;

      this.blockedList_ = $('blocked-origin-list');
      options.OriginList.decorate(this.blockedList_);
      this.blockedList_.autoExpands = true;
    },

    /**
     * Populate an origin list with all of the origins with a given permission
     * or that are using a given resource.
     * @param {OriginList} originList A list to populate.
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
     * @private
     */
    populateOrigins: function(allowedDict, blockedDict) {
      this.populateOriginsHelper_(this.allowedList_, allowedDict);
      if (blockedDict) {
        this.populateOriginsHelper_(this.blockedList_, blockedDict);
        this.blockedList_.hidden = false;
        $('blocked-origin-list-title').hidden = false;
        this.allowedList_.classList.remove('nonsplit-origin-list');
      } else {
        this.blockedList_.hidden = true;
        $('blocked-origin-list-title').hidden = true;
        this.allowedList_.classList.add('nonsplit-origin-list');
      }
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
  };

  WebsiteSettingsManager.populateOrigins = function(allowedDict, blockedDict) {
    WebsiteSettingsManager.getInstance().populateOrigins(allowedDict,
        blockedDict);
  };

  WebsiteSettingsManager.showEditPage = function(url) {
    WebsiteSettingsEditor.getInstance().populatePage(url);
  };

  // Export
  return {
    WebsiteSettingsManager: WebsiteSettingsManager
  };
});
