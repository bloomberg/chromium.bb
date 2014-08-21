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
     * The saved origins list.
     * @type {OriginList}
     * @private
     */
    originList_: null,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('website-settings-overlay-confirm').onclick =
          PageManager.closeOverlay.bind(PageManager);

      $('resourceType').onchange = function() {
        var target = event.target;
        assert(target.tagName == 'SELECT');
        if (target.value == 'storage')
          chrome.send('updateLocalStorage');
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
      this.originList_ = this.pageDiv.querySelector('.origin-list');
      options.OriginList.decorate(this.originList_);
      this.originList_.autoExpands = true;
    },

    /**
     * Populate the origin list with all of the origins with a given permission
     * or that are using a given resource.
     * @param {!Object} originDict A dictionary of origins to their usage, which
           will be used to sort the origins.
     * @private
     */
    populateOrigins_: function(originDict) {
      var origins = Object.keys(originDict).map(function(origin) {
        // |usage| means the time of last usage in seconds since epoch
        // (Jan 1, 1970) for permissions and means the amount of local storage
        // in bytes used for local storage.
        return {
          origin: origin,
          usage: originDict[origin].usage,
          usageString: originDict[origin].usageString
        };
      });
      origins.sort(function(first, second) {
        return second.usage - first.usage;
      });
      this.originList_.dataModel = new ArrayDataModel(origins);
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
    handleSearchQueryChange_: function() {
      if (this.queryDelayTimerId_)
        window.clearTimeout(this.queryDelayTimerId_);

      this.queryDelayTimerId_ = window.setTimeout(this.searchOrigins.bind(this),
                                                  160);
    },
  };

  WebsiteSettingsManager.populateOrigins = function(originDict) {
    WebsiteSettingsManager.getInstance().populateOrigins_(originDict);
  };

  WebsiteSettingsManager.showEditPage = function(url) {
    WebsiteSettingsEditor.getInstance().populatePage(url);
  };

  // Export
  return {
    WebsiteSettingsManager: WebsiteSettingsManager
  };
});
