// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // CookiesView class:

  /**
   * Encapsulated handling of ChromeOS accounts options page.
   * @constructor
   */
  function CookiesView(model) {
    OptionsPage.call(this, 'cookiesView',
                     templateData.cookiesViewPageTabTitle,
                     'cookiesViewPage');
  }

  cr.addSingletonGetter(CookiesView);

  CookiesView.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var cookiesTree = $('cookiesTree');

      options.CookiesTree.decorate(cookiesTree);
      cookiesTree.addEventListener('change',
          this.handleCookieTreeChange_.bind(this));
      cookiesTree.addEventListener('keydown', this.handleKeyDown_.bind(this));

      $('cookiesSearchBox').addEventListener('keydown',
          this.handleQueryEditKeyDown_.bind(this));

      var self = this;
      $('clearCookieSearchButton').onclick = function(e) {
        $('cookiesSearchBox').value = '';
        self.searchCookie();
      }

      $('remove-cookie').onclick = function(e) {
        self.removeSelectedCookie_();
      }

      $('remove-all-cookie').onclick = function(e) {
        chrome.send('removeAllCookies', []);
      }

      this.addEventListener('visibleChange', this.handleVisibleChange_);

      this.clearCookieInfo();
    },

    /**
     * Set visible detailed info.
     * @param {string} name Name of the details info pane to made visible.
     */
    updateVisibleDetailedInfo: function(name) {
      const infoPaneNames = [
          'cookiesInfo',
          'appCacheInfo',
          'webDbInfo',
          'localStorageInfo',
          'indexedDBInfo'];

      for (var i = 0 ; i < infoPaneNames.length; ++i) {
        var paneName = infoPaneNames[i];
        var pane = $(paneName);
        if (name == paneName) {
          pane.classList.remove('hidden');
        } else {
          pane.classList.add('hidden');
        }
      }
    },

    /**
     * Update remove button state.
     */
    updateRemoveButtonState: function() {
      var cookiesTree = $('cookiesTree');
      $('remove-cookie').disabled = !cookiesTree.children.length ||
          !cookiesTree.selectedItem;
      $('remove-all-cookie').disabled = !cookiesTree.children.length;
    },

    /**
     * Clears cookie info.
     */
    clearCookieInfo: function() {
      var noCookie = localStrings.getString('no_cookie');
      $('cookieName').textContent = noCookie;
      $('cookieContent').textContent = noCookie;
      $('cookieDomain').textContent = noCookie;
      $('cookiePath').textContent = noCookie;
      $('cookieSendFor').textContent = noCookie;
      $('cookieAccessibleToScript').textContent = noCookie;
      $('cookieCreated').textContent = noCookie;
      $('cookieExpires').textContent = noCookie;
    },

    /**
     * Sets cookie info to display.
     */
    setCookieInfo: function(cookie) {
      $('cookieName').textContent = cookie.name;
      $('cookieContent').textContent = cookie.content;
      $('cookieDomain').textContent = cookie.domain;
      $('cookiePath').textContent = cookie.path;
      $('cookieSendFor').textContent = cookie.sendfor;
      $('cookieAccessibleToScript').textContent = cookie.accessibleToScript;
      $('cookieCreated').textContent = cookie.created;
      $('cookieExpires').textContent = cookie.expires;
    },

    /**
     * Sets app cache info to display.
     */
    setAppCacheInfo: function(appCache) {
      $('appCacheManifest').textContent = appCache.manifest;
      $('appCacheSize').textContent = appCache.size;
      $('appCacheCreated').textContent = appCache.created;
      $('appCacheLastAccessed').textContent = appCache.accessed;
    },

    /**
     * Sets web db info to display.
     */
    setWebDbInfo: function(webDb) {
      $('webdbName').textContent = webDb.name;
      $('webdbDesc').textContent = webDb.desc;
      $('webdbSize').textContent = webDb.size;
      $('webdbLastModified').textContent = webDb.modified;
    },

    /**
     * Sets local storage info to display.
     */
    setLocalStorageInfo: function(localStorage) {
      $('localStorageOrigin').textContent = localStorage.origin;
      $('localStorageSize').textContent = localStorage.size;
      $('localStorageLastModified').textContent = localStorage.modified;
    },

    /**
     * Sets IndexedDB info to display.
     */
    setIndexedDBInfo: function(indexedDB) {
      $('indexedDBOrigin').textContent = indexedDB.origin;
      $('indexedDBSize').textContent = indexedDB.size;
      $('indexedDBLastModified').textContent = indexedDB.modified;
    },

    lastQuery_ : null,

    /**
     * Search cookie using text in cookiesSearchBox.
     */
    searchCookie: function() {
      this.queryDelayTimerId_ = 0;
      var filter = $('cookiesSearchBox').value;
      if (this.lastQuery_ != filter) {
        chrome.send('updateCookieSearchResults', [filter]);
        this.lastQuery_ = filter;
      }
    },

    /**
     * Handles cookie tree selection change.
     * @private
     * @param {!Event} e The change event object.
     */
    handleCookieTreeChange_: function(e) {
      this.updateRemoveButtonState();

      var cookiesTree = $('cookiesTree');
      var data = null;
      if (cookiesTree.selectedItem) {
        data = cookiesTree.selectedItem.data;
      }

      if (data && data.type == 'cookie') {
        this.setCookieInfo(data);
        this.updateVisibleDetailedInfo('cookiesInfo');
      } else if (data && data.type == 'database') {
        this.setWebDbInfo(data);
        this.updateVisibleDetailedInfo('webDbInfo');
      } else if (data && data.type == 'local_storage') {
        this.setLocalStorageInfo(data);
        this.updateVisibleDetailedInfo('localStorageInfo');
      } else if (data && data.type == 'app_cache') {
        this.setAppCacheInfo(data);
        this.updateVisibleDetailedInfo('appCacheInfo');
      } else if (data && data.type == 'indexed_db') {
        this.setIndexedDBInfo(data);
        this.updateVisibleDetailedInfo('indexedDBInfo');
      } else {
        this.clearCookieInfo();
        this.updateVisibleDetailedInfo('cookiesInfo');
      }
    },

    queryDelayTimerId_: 0,

    /**
     * Handles query edit key down.
     * @private
     * @param {!Event} e The event object.
     */
    handleQueryEditKeyDown_: function(e) {
      if (this.queryDelayTimerId_) {
        window.clearTimeout(this.queryDelayTimerId_);
      }

      this.queryDelayTimerId_ = window.setTimeout(
          this.searchCookie.bind(this), 500);
    },

    initalized_: false,

    /**
     * Handler for OptionsPage's visible property change event.
     * @private
     * @param {Event} e Property change event.
     */
    handleVisibleChange_: function(e) {
      if (!this.initalized_ && this.visible) {
        this.initalized_ = true;
        this.searchCookie();
      }
    },

    /**
     * Remove currently selected cookie.
     * @private
     */
    removeSelectedCookie_: function() {
      var selected = cookiesTree.selectedItem;
      if (selected)
        chrome.send('removeCookie', [selected.pathId]);
    },

    /**
     * Handler for keydown event.
     * @private
     * @param {Event} e KeyDown event.
     */
    handleKeyDown_: function(e) {
      // If 'Remove' button is enabled and key is 'Delete' key on all platforms
      // or 'Backspace' on Mac.
      if (!$('remove-cookie').disabled &&
          (e.keyIdentifier == 'U+007F' ||
            (cr.isMac && e.keyIdentifier == 'U+0008'))) {
        // No further key handling to avoid navigation triggered by 'Backspace'
        // on Mac.
        e.preventDefault();

        this.removeSelectedCookie_();
      }
    }
  };

  // CookiesViewHandler callbacks.
  CookiesView.onTreeItemAdded = function(args) {
    $('cookiesTree').addByParentId(args[0], args[1], args[2]);
  };

  CookiesView.onTreeItemRemoved = function(args) {
    $('cookiesTree').removeByParentId(args[0], args[1], args[2]);
  };

  CookiesView.loadChildren = function(args) {
    $('cookiesTree').loadChildren(args[0], args[1]);
  };

  // Export
  return {
    CookiesView: CookiesView
  };

});
