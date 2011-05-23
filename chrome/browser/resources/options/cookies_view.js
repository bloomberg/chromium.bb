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

    /**
     * The timer id of the timer set on search query change events.
     * @type {number}
     * @private
     */
    queryDelayTimerId_: 0,

    /**
     * The most recent search query, or null if the query is empty.
     * @type {?string}
     * @private
     */
    lastQuery_ : null,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('cookies-search-box').addEventListener('search',
          this.handleSearchQueryChange_.bind(this));

      $('remove-all-cookies-button').onclick = function(e) {
        chrome.send('removeAllCookies', []);
      };

      var cookiesList = $('cookies-list');
      options.CookiesList.decorate(cookiesList);
      window.addEventListener('resize', this.handleResize_.bind(this));

      this.addEventListener('visibleChange', this.handleVisibleChange_);
    },

    /**
     * Search cookie using text in |cookies-search-box|.
     */
    searchCookie: function() {
      this.queryDelayTimerId_ = 0;
      var filter = $('cookies-search-box').value;
      if (this.lastQuery_ != filter) {
        this.lastQuery_ = filter;
        chrome.send('updateCookieSearchResults', [filter]);
      }
    },

    /**
     * Handles search query changes.
     * @param {!Event} e The event object.
     * @private
     */
    handleSearchQueryChange_: function(e) {
      if (this.queryDelayTimerId_)
        window.clearTimeout(this.queryDelayTimerId_);

      this.queryDelayTimerId_ = window.setTimeout(
          this.searchCookie.bind(this), 500);
    },

    initialized_: false,

    /**
     * Handler for OptionsPage's visible property change event.
     * @param {Event} e Property change event.
     * @private
     */
    handleVisibleChange_: function(e) {
      if (!this.visible)
        return;
      // Resize the cookies list whenever the options page becomes visible.
      this.handleResize_(null);
      if (!this.initialized_) {
        this.initialized_ = true;
        this.searchCookie();
      } else {
        $('cookies-list').redraw();
      }
    },

    /**
     * Handler for when the window changes size. Resizes the cookies list to
     * match the window height.
     * @param {?Event} e Window resize event, or null if called directly.
     * @private
     */
    handleResize_: function(e) {
      if (!this.visible)
        return;
      var cookiesList = $('cookies-list');
      // 25 pixels from the window bottom seems like a visually pleasing amount.
      var height = window.innerHeight - cookiesList.offsetTop - 25;
      cookiesList.style.height = height + 'px';
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
