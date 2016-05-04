// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * Compares two extensions to determine which should come first in the list.
   * @param {chrome.developerPrivate.ExtensionInfo} a
   * @param {chrome.developerPrivate.ExtensionInfo} b
   * @return {number}
   */
  var compareExtensions = function(a, b) {
    function compare(x, y) {
      return x < y ? -1 : (x > y ? 1 : 0);
    }
    function compareLocation(x, y) {
      if (x.location == y.location)
        return 0;
      if (x.location == chrome.developerPrivate.Location.UNPACKED)
        return -1;
      if (y.location == chrome.developerPrivate.Location.UNPACKED)
        return 1;
      return 0;
    }
    return compareLocation(a, b) ||
           compare(a.name.toLowerCase(), b.name.toLowerCase()) ||
           compare(a.id, b.id);
  };

  var Manager = Polymer({
    is: 'extensions-manager',

    properties: {
      /** @type {extensions.Sidebar} */
      sidebar: Object,

      /** @type {extensions.ItemDelegate} */
      itemDelegate: Object,

      inDevMode: {
        type: Boolean,
        value: false,
      },

      filter: {
        type: String,
        value: '',
      },

      /** @type {!Array<!chrome.developerPrivate.ExtensionInfo>} */
      extensions: {
        type: Array,
        value: function() { return []; },
      },

      /** @type {!Array<!chrome.developerPrivate.ExtensionInfo>} */
      apps: {
        type: Array,
        value: function() { return []; },
      },
    },

    behaviors: [
      I18nBehavior,
    ],

    listeners: {
      'items-list.extension-item-show-details': 'showItemDetails_',
    },

    created: function() {
      this.readyPromiseResolver = new PromiseResolver();
    },

    ready: function() {
      /** @type {extensions.Sidebar} */
      this.sidebar =
          /** @type {extensions.Sidebar} */(this.$$('extensions-sidebar'));
      this.listHelper_ = new ListHelper(this);
      this.sidebar.setListDelegate(this.listHelper_);
      this.$.toolbar.setSearchDelegate(new SearchHelper(this));
      this.readyPromiseResolver.resolve();
    },

    /**
     * @param {chrome.developerPrivate.ExtensionType} type The type of item.
     * @return {string} The ID of the list that the item belongs in.
     * @private
     */
    getListId_: function(type) {
      var listId;
      var ExtensionType = chrome.developerPrivate.ExtensionType;
      switch (type) {
        case ExtensionType.HOSTED_APP:
        case ExtensionType.LEGACY_PACKAGED_APP:
        case ExtensionType.PLATFORM_APP:
          listId = 'apps';
          break;
        case ExtensionType.EXTENSION:
        case ExtensionType.SHARED_MODULE:
          listId = 'extensions';
          break;
        case ExtensionType.THEME:
          assertNotReached(
              'Don\'t send themes to the chrome://extensions page');
          break;
      }
      assert(listId);
      return listId;
    },

    /**
     * @param {string} listId The list to look for the item in.
     * @param {string} itemId The id of the item to look for.
     * @return {number} The index of the item in the list, or -1 if not found.
     * @private
     */
    getIndexInList_: function(listId, itemId) {
      return this[listId].findIndex(function(item) {
        return item.id == itemId;
      });
    },

    /**
     * @return {boolean} Whether the list should be visible.
     * @private
     */
    computeListHidden_: function() {
      return this.$['items-list'].items.length == 0;
    },

    /**
     * Creates and adds a new extensions-item element to the list, inserting it
     * into its sorted position in the relevant section.
     * @param {!chrome.developerPrivate.ExtensionInfo} item The extension
     *     the new element is representing.
     */
    addItem: function(item) {
      var listId = this.getListId_(item.type);
      // We should never try and add an existing item.
      assert(this.getIndexInList_(listId, item.id) == -1);
      var insertBeforeChild = this[listId].findIndex(function(listEl) {
        return compareExtensions(listEl, item) > 0;
      });
      if (insertBeforeChild == -1)
        insertBeforeChild = this[listId].length;
      this.splice(listId, insertBeforeChild, 0, item);
    },

    /**
     * @param {!chrome.developerPrivate.ExtensionInfo} item The data for the
     *     item to update.
     */
    updateItem: function(item) {
      var listId = this.getListId_(item.type);
      var index = this.getIndexInList_(listId, item.id);
      // We should never try and update a non-existent item.
      assert(index >= 0);
      this.set([listId, index], item);
    },

    /**
     * @param {!chrome.developerPrivate.ExtensionInfo} item The data for the
     *     item to remove.
     */
    removeItem: function(item) {
      var listId = this.getListId_(item.type);
      var index = this.getIndexInList_(listId, item.id);
      // We should never try and remove a non-existent item.
      assert(index >= 0);
      this.splice(listId, index, 1);
    },

    /**
     * Shows the detailed view for a given item.
     * @param {CustomEvent} e
     * @private
     */
    showItemDetails_: function(e) {
      this.$['details-view'].set('data', assert(e.detail.element.data));
      this.$.pages.selected = 1;
    },

    /** @private */
    onDetailsViewClose_: function() {
      this.$.pages.selected = 0;
    }
  });

  /**
   * @param {extensions.Manager} manager
   * @constructor
   * @implements {extensions.SidebarListDelegate}
   */
  function ListHelper(manager) {
    this.manager_ = manager;
  }

  ListHelper.prototype = {
    /** @override */
    showType: function(type) {
      var items;
      switch (type) {
        case extensions.ShowingType.EXTENSIONS:
          items = this.manager_.extensions;
          break;
        case extensions.ShowingType.APPS:
          items = this.manager_.apps;
          break;
      }
      this.manager_.$['items-list'].set('items', assert(items));
    }
  };

  /**
   * @param {extensions.Manager} manager
   * @constructor
   * @implements {SearchFieldDelegate}
   */
  function SearchHelper(manager) {
    this.manager_ = manager;
  }

  SearchHelper.prototype = {
    /** @override */
    onSearchTermSearch: function(searchTerm) {
      this.manager_.filter = searchTerm;
    },
  };

  return {Manager: Manager};
});
