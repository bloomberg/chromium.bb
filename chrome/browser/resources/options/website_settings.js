// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  /////////////////////////////////////////////////////////////////////////////
  // WebsiteSettingsManager class:

  /**
   * Encapsulated handling of the website settings page.
   * @constructor
   */
  function WebsiteSettingsManager() {
    OptionsPage.call(this, 'websiteSettings',
                     loadTimeData.getString('websitesOptionsPageTabTitle'),
                     'website-settings-page');
  }

  cr.addSingletonGetter(WebsiteSettingsManager);

  WebsiteSettingsManager.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * The saved origins list.
     * @type {OriginList}
     * @private
     */
    originList_: null,

    /** @override */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('website-settings-overlay-confirm').onclick =
          OptionsPage.closeOverlay.bind(OptionsPage);

      $('resourceType').onchange = function() {
        var target = event.target;
        assert(target.tagName == 'SELECT');
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
     * @private
     */
    populateOrigins_: function(originDict) {
      // TODO(dhnishi): Include the last usage time instead of just pushing the
      // keys.
      this.originList_.dataModel = new ArrayDataModel(Object.keys(originDict));
    },

    /**
     * Update the table with the origins filtered by the value in the search
     * box.
     * @private
     */
    searchOrigins: function() {
      var filter =
         $('website-settings-search-box').value;
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

  // Export
  return {
    WebsiteSettingsManager: WebsiteSettingsManager
  };
});
