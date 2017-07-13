// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  var ItemList = Polymer({
    is: 'extensions-item-list',

    behaviors: [Polymer.NeonAnimatableBehavior, Polymer.IronResizableBehavior],

    properties: {
      /** @type {Array<!chrome.developerPrivate.ExtensionInfo>} */
      items: Array,

      /** @type {extensions.ItemDelegate} */
      delegate: Object,

      inDevMode: {
        type: Boolean,
        value: false,
      },

      filter: String,

      /** @private {Array<!chrome.developerPrivate.ExtensionInfo>} */
      shownItems_: {
        type: Array,
        computed: 'computeShownItems_(items.*, filter)',
      }
    },

    listeners: {
      'list.extension-item-size-changed': 'itemSizeChanged_',
    },

    ready: function() {
      /** @type {extensions.AnimationHelper} */
      this.animationHelper = new extensions.AnimationHelper(this, this.$.list);
      this.animationHelper.setEntryAnimations([extensions.Animation.FADE_IN]);
      this.animationHelper.setExitAnimations([extensions.Animation.HERO]);
    },

    /**
     * Called when a subpage for a given item is about to be shown.
     * @param {string} id
     */
    willShowItemSubpage: function(id) {
      this.sharedElements = {hero: this.$$('#' + id)};
    },

    /**
     * Updates the size for a given item.
     * @param {CustomEvent} e
     * @private
     * @suppress {checkTypes} Closure doesn't know $.list is an IronList.
     */
    itemSizeChanged_: function(e) {
      this.$.list.updateSizeForItem(e.detail.item);
    },

    /**
     * Called right before an item enters the detailed view.
     * @param {CustomEvent} e
     * @private
     */
    showItemDetails_: function(e) {
      this.sharedElements = {hero: e.detail.element};
    },

    /**
     * Computes the list of items to be shown.
     * @param {Object} changeRecord The changeRecord for |items|.
     * @param {string} filter The updated filter string.
     * @return {Array<!chrome.developerPrivate.ExtensionInfo>}
     * @private
     */
    computeShownItems_: function(changeRecord, filter) {
      return this.items.filter(function(item) {
        return item.name.toLowerCase().includes(this.filter.toLowerCase());
      }, this);
    },

    shouldShowEmptyItemsMessage_: function() {
      return this.items.length === 0;
    },

    shouldShowEmptySearchMessage_: function() {
      return !this.shouldShowEmptyItemsMessage_() &&
          this.shownItems_.length === 0;
    },
  });

  return {
    ItemList: ItemList,
  };
});
