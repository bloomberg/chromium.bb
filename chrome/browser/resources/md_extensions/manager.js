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
      var extensionItem = new extensions.Item(extension, delegate);
      var itemList = this.$['item-list'];
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

  return {Manager: Manager};
});
