// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-list',

  behaviors: [
    bookmarks.StoreClient,
  ],

  properties: {
    /**
     * A list of item ids wrapped in an Object. This is necessary because
     * iron-list is unable to distinguish focusing index 6 from focusing id '6'
     * so the item we supply to iron-list needs to be non-index-like.
     * @private {Array<{id: string}>}
     */
    displayedList_: {
      type: Array,
      value: function() {
        // Use an empty list during initialization so that the databinding to
        // hide #bookmarksCard takes effect.
        return [];
      },
    },

    /** @private {Array<string>} */
    displayedIds_: {
      type: Array,
      observer: 'onDisplayedIdsChanged_',
    },

    /** @private */
    searchTerm_: String,
  },

  listeners: {
    'click': 'deselectItems_',
  },

  attached: function() {
    var list = /** @type {IronListElement} */ (this.$.bookmarksCard);
    list.scrollTarget = this;

    this.watch('displayedIds_', function(state) {
      return bookmarks.util.getDisplayedList(state);
    });
    this.watch('searchTerm_', function(state) {
      return state.search.term;
    });
    this.updateFromStore();
  },

  /** @return {HTMLElement} */
  getDropTarget: function() {
    return this.$.message;
  },

  /**
   * Updates `displayedList_` using splices to be equivalent to `newValue`. This
   * allows the iron-list to delete sublists of items which preserves scroll and
   * focus on incremental update.
   * @param {Array<string>} newValue
   * @param {Array<string>} oldValue
   */
  onDisplayedIdsChanged_: function(newValue, oldValue) {
    if (!oldValue) {
      this.displayedList_ = this.displayedIds_.map(function(id) {
        return {id: id};
      });
    } else {
      var splices = Polymer.ArraySplice.calculateSplices(newValue, oldValue);
      splices.forEach(function(splice) {
        // TODO(calamity): Could use notifySplices to improve performance here.
        var additions =
            newValue.slice(splice.index, splice.index + splice.addedCount)
                .map(function(id) {
                  return {id: id};
                });
        this.splice.apply(this, [
          'displayedList_', splice.index, splice.removed.length
        ].concat(additions));
      }.bind(this));
    }
  },

  /** @private */
  emptyListMessage_: function() {
    var emptyListMessage = this.searchTerm_ ? 'noSearchResults' : 'emptyList';
    return loadTimeData.getString(emptyListMessage);
  },

  /** @private */
  isEmptyList_: function() {
    return this.displayedList_.length == 0;
  },

  /** @private */
  deselectItems_: function() {
    this.dispatch(bookmarks.actions.deselectItems());
  },
});
