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
      sidebar: {
        type: Object,
      }
    },

    ready: function() {
      /** @type {extensions.Sidebar} */
      this.sidebar = this.$$('extensions-sidebar');
      extensions.Service.getInstance().managerReady(this);
      this.sidebar.setScrollDelegate(new ScrollHelper(this));
    },

    /**
     * Creates and adds a new extensions-item element to the list, inserting it
     * into its sorted position in the relevant section.
     * @param {!chrome.developerPrivate.ExtensionInfo} extension The extension
     *     the item is representing.
     * @param {!extensions.ItemDelegate} delegate The delegate for the item.
     */
    addItem: function(extension, delegate) {
      var listId;
      var ExtensionType = chrome.developerPrivate.ExtensionType;
      switch (extension.type) {
        case ExtensionType.HOSTED_APP:
        case ExtensionType.LEGACY_PACKAGED_APP:
          listId = 'websites-list';
          break;
        case ExtensionType.PLATFORM_APP:
          listId = 'apps-list';
          break;
        case ExtensionType.EXTENSION:
        case ExtensionType.SHARED_MODULE:
          listId = 'extensions-list';
          break;
        case ExtensionType.THEME:
          assertNotReached(
              'Don\'t send themes to the chrome://extensions page');
          break;
      }
      assert(listId);
      var extensionItem = new extensions.Item(extension, delegate);
      var itemList = this.$[listId];

      var insertBeforeChild = Array.prototype.find.call(itemList.children,
                                                        function(item) {
        return compareExtensions(item.data, extension) > 0;
      });
      itemList.insertBefore(extensionItem, insertBeforeChild);
    },

    /**
     * @param {string} id The id of the extension to get the item for.
     * @return {?extensions.Item}
     */
    getItem: function(id) {
      return this.$$('#' + id);
    },

    /** @param {string} id The id of the item to remove. */
    removeItem: function(id) {
      var item = this.getItem(id);
      if (item)
        item.parentNode.removeChild(item);
    },

    /**
     * Applies a function to each item present in the DOM.
     * @param {!function(extensions.Item):void} callback The function to apply.
     */
    forEachItem: function(callback) {
      Array.prototype.forEach.call(
          /** @type {Array<extensions.Item>} */(
              Polymer.dom(this.root).querySelectorAll('extensions-item')),
          callback);
     },
  });

  /**
   * @param {extensions.Manager} manager
   * @constructor
   * @implements {extensions.SidebarScrollDelegate}
   */
  function ScrollHelper(manager) {
    this.items_ = manager.$.items;
  }

  ScrollHelper.prototype = {
    /** @override */
    scrollToExtensions: function() {
      this.items_.scrollTop =
          this.items_.querySelector('#extensions-header').offsetTop;
    },

    /** @override */
    scrollToApps: function() {
      this.items_.scrollTop =
          this.items_.querySelector('#apps-header').offsetTop;
    },

    /** @override */
    scrollToWebsites: function() {
      this.items_.scrollTop =
          this.items_.querySelector('#websites-header').offsetTop;
    },
  };

  return {Manager: Manager};
});
