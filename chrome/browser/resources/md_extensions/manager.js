// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  var Manager = Polymer({
    is: 'extensions-manager',

    properties: {
      sidebar: {
        type: Object,
      }
    },

    ready: function() {
      this.sidebar = this.$$('extensions-sidebar');
      extensions.Service.getInstance().managerReady(this);
      this.sidebar.setScrollDelegate(new ScrollHelper(this));
    },

    /**
     * Creates and adds a new extensions-item element to the list.
     * @param {!chrome.developerPrivate.ExtensionInfo} extension The extension
     *     the item is representing.
     * @param {!extensions.ItemDelegate} delegate The delegate for the item.
     * @param {!number=} opt_index The index at which to place the item. If
     *     not present, the item is appended to the end of the list.
     */
    addItem: function(extension, delegate, opt_index) {
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
      var refNode = opt_index !== undefined ?
          itemList.children[opt_index] : undefined;
      itemList.insertBefore(extensionItem, refNode);
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
