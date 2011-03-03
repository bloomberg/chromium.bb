// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // CookiesView class:

  /**
   * Encapsulated handling of the cookies and other site data page.
   * @constructor
   */
  function CookiesView(model) {
    OptionsPage.call(this, 'cookies',
                     templateData.cookiesViewPageTabTitle,
                     'cookiesViewPage');
  }

  cr.addSingletonGetter(CookiesView);

  CookiesView.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('cookies-search-box').addEventListener('search',
          this.handleSearchQueryChange_.bind(this));

      $('remove-all-cookies-button').onclick = function(e) {
        chrome.send('removeAllCookies', []);
      };

      var cookiesList = $('cookies-list');
      options.CookiesList.decorate(cookiesList);

      this.addEventListener('visibleChange', this.handleVisibleChange_);
    },

    lastQuery_ : null,

    /**
     * Search cookie using text in cookiesSearchBox.
     */
    searchCookie: function() {
      this.queryDelayTimerId_ = 0;
      var filter = $('cookies-search-box').value;
      if (this.lastQuery_ != filter) {
        this.lastQuery_ = filter;
        chrome.send('updateCookieSearchResults', [filter]);
      }
    },

    queryDelayTimerId_: 0,

    /**
     * Handles search query changes.
     * @private
     * @param {!Event} e The event object.
     */
    handleSearchQueryChange_: function(e) {
      if (this.queryDelayTimerId_) {
        window.clearTimeout(this.queryDelayTimerId_);
      }

      this.queryDelayTimerId_ = window.setTimeout(
          this.searchCookie.bind(this), 500);
    },

    initialized_: false,

    /**
     * Handler for OptionsPage's visible property change event.
     * @private
     * @param {Event} e Property change event.
     */
    handleVisibleChange_: function(e) {
      if (!this.initialized_ && this.visible) {
        this.initialized_ = true;
        this.searchCookie();
      }
    },
  };

  // CookiesViewHandler callbacks.
  CookiesView.onTreeItemAdded = function(args) {
    $('cookies-list').addByParentId(args[0], args[1], args[2]);
  };

  CookiesView.onTreeItemRemoved = function(args) {
    $('cookies-list').removeByParentId(args[0], args[1], args[2]);
  };

  CookiesView.loadChildren = function(args) {
    $('cookies-list').loadChildren(args[0], args[1]);
  };

  // Export
  return {
    CookiesView: CookiesView
  };

});
