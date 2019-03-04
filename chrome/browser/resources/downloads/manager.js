// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  const Manager = Polymer({
    is: 'downloads-manager',

    behaviors: [
      FindShortcutBehavior,
    ],

    properties: {
      /** @private */
      hasDownloads_: {
        observer: 'hasDownloadsChanged_',
        type: Boolean,
      },

      /** @private */
      hasShadow_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** @private */
      inSearchMode_: {
        type: Boolean,
        value: false,
      },

      /** @private {!Array<!downloads.Data>} */
      items_: {
        type: Array,
        value: function() {
          return [];
        },
      },

      /** @private */
      spinnerActive_: {
        type: Boolean,
        notify: true,
      },

      /** @private {Element} */
      lastFocused_: Object,

      /** @private */
      listBlurred_: Boolean,
    },

    hostAttributes: {
      // TODO(dbeam): this should use a class instead.
      loading: true,
    },

    observers: [
      'itemsChanged_(items_.*)',
    ],

    listeners: {
      'restore-focus-after-remove': 'onRestoreFocusAfterRemove_',
    },

    /** @private {downloads.mojom.PageCallbackRouter} */
    mojoEventTarget_: null,

    /** @private {downloads.mojom.PageHandlerInterface} */
    mojoHandler_: null,

    /** @private {boolean} */
    restoreFocusAfterRemove_: false,

    /** @private {?downloads.SearchService} */
    searchService_: null,

    /** @private {!PromiseResolver} */
    loaded_: new PromiseResolver,

    /** @private {Array<number>} */
    listenerIds_: null,

    /** @override */
    created: function() {
      const browserProxy = downloads.BrowserProxy.getInstance();
      this.mojoEventTarget_ = browserProxy.callbackRouter;
      this.mojoHandler_ = browserProxy.handler;
      this.searchService_ = downloads.SearchService.getInstance();
    },

    /** @override */
    attached: function() {
      document.documentElement.classList.remove('loading');
      this.listenerIds_ = [
        this.mojoEventTarget_.clearAll.addListener(this.clearAll_.bind(this)),
        this.mojoEventTarget_.insertItems.addListener(
            this.insertItems_.bind(this)),
        this.mojoEventTarget_.removeItem.addListener(
            this.removeItem_.bind(this)),
        this.mojoEventTarget_.updateItem.addListener(
            this.updateItem_.bind(this)),
      ];

      // TODO(aee): Remove this conditional when the Polymer 2 migration
      // is completed. Polymer.DomIf exists in Polymer 2 and not in Polymer 1.
      if (typeof Polymer.DomIf == 'undefined') {
        this.$.downloadsList.preserveFocus = false;
      }
    },

    /** @override */
    detached: function() {
      this.listenerIds_.forEach(
          id => assert(this.mojoEventTarget_.removeListener(id)));
    },

    /** @private */
    clearAll_: function() {
      this.set('items_', []);
    },

    /** @private */
    hasDownloadsChanged_: function() {
      if (loadTimeData.getBoolean('allowDeletingHistory')) {
        this.$.toolbar.downloadsShowing = this.hasDownloads_;
      }

      if (this.hasDownloads_) {
        this.$.downloadsList.fire('iron-resize');
      }
    },

    /**
     * @param {number} index
     * @param {!Array<downloads.Data>} items
     * @private
     */
    insertItems_: function(index, items) {
      // Insert |items| at the given |index| via Array#splice().
      if (items.length > 0) {
        this.items_.splice.apply(this.items_, [index, 0].concat(items));
        this.updateHideDates_(index, index + items.length);
        this.notifySplices('items_', [{
                             index: index,
                             addedCount: items.length,
                             object: this.items_,
                             type: 'splice',
                             removed: [],
                           }]);
      }

      if (this.hasAttribute('loading')) {
        this.removeAttribute('loading');
        this.loaded_.resolve();
      }

      this.spinnerActive_ = false;
    },

    /** @private */
    itemsChanged_: function() {
      this.hasDownloads_ = this.items_.length > 0;

      if (this.inSearchMode_) {
        Polymer.IronA11yAnnouncer.requestAvailability();
        this.fire('iron-announce', {
          text: this.items_.length == 0 ?
              this.noDownloadsText_() :
              (this.items_.length == 1 ?
                   loadTimeData.getStringF(
                       'searchResultsSingular',
                       this.$.toolbar.getSearchText()) :
                   loadTimeData.getStringF(
                       'searchResultsPlural', this.items_.length,
                       this.$.toolbar.getSearchText()))
        });
      }
    },

    /**
     * @return {string} The text to show when no download items are showing.
     * @private
     */
    noDownloadsText_: function() {
      return loadTimeData.getString(
          this.inSearchMode_ ? 'noSearchResults' : 'noDownloads');
    },

    /**
     * @param {Event} e
     * @private
     */
    onCanExecute_: function(e) {
      e = /** @type {cr.ui.CanExecuteEvent} */ (e);
      switch (e.command.id) {
        case 'undo-command':
          e.canExecute = this.$.toolbar.canUndo();
          break;
        case 'clear-all-command':
          e.canExecute = this.$.toolbar.canClearAll();
          break;
      }
    },

    /**
     * @param {Event} e
     * @private
     */
    onCommand_: function(e) {
      if (e.command.id == 'clear-all-command') {
        this.mojoHandler_.clearAll();
      } else if (e.command.id == 'undo-command') {
        this.mojoHandler_.undo();
      }
    },

    /** @private */
    onScroll_: function() {
      const container = this.$.downloadsList.scrollTarget;
      const distanceToBottom =
          container.scrollHeight - container.scrollTop - container.offsetHeight;
      if (distanceToBottom <= 100) {
        // Approaching the end of the scrollback. Attempt to load more items.
        this.searchService_.loadMore();
      }
      this.hasShadow_ = container.scrollTop > 0;
    },

    /**
     * @return {!Promise}
     * @private
     */
    onLoad_: function() {
      cr.ui.decorate('command', cr.ui.Command);
      document.addEventListener('canExecute', this.onCanExecute_.bind(this));
      document.addEventListener('command', this.onCommand_.bind(this));

      this.searchService_.loadMore();
      return this.loaded_.promise;
    },

    /** @private */
    onSearchChanged_: function() {
      this.inSearchMode_ = this.searchService_.isSearching();
    },

    /**
     * @param {number} index
     * @private
     */
    removeItem_: function(index) {
      const removed = this.items_.splice(index, 1);
      this.updateHideDates_(index, index);
      this.notifySplices('items_', [{
                           index: index,
                           addedCount: 0,
                           object: this.items_,
                           type: 'splice',
                           removed: removed,
                         }]);
      if (this.restoreFocusAfterRemove_) {
        this.restoreFocusAfterRemove_ = false;
        if (this.items_.length > 0) {
          setTimeout(() => {
            this.$.downloadsList.focusItem(index);
            const item = getDeepActiveElement();
            if (item) {
              item.focusOnRemoveButton();
            }
          });
        }
      }
      this.onScroll_();
    },

    /** @private */
    onRestoreFocusAfterRemove_: function() {
      this.restoreFocusAfterRemove_ = true;
    },

    /**
     * Updates whether dates should show for |this.items_[start - end]|. Note:
     * this method does not trigger template bindings. Use notifySplices() or
     * after calling this method to ensure items are redrawn.
     * @param {number} start
     * @param {number} end
     * @private
     */
    updateHideDates_: function(start, end) {
      for (let i = start; i <= end; ++i) {
        const current = this.items_[i];
        if (!current) {
          continue;
        }
        const prev = this.items_[i - 1];
        current.hideDate = !!prev && prev.dateString == current.dateString;
      }
    },

    /**
     * @param {number} index
     * @param {!downloads.Data} data
     * @private
     */
    updateItem_: function(index, data) {
      this.items_[index] = data;
      this.updateHideDates_(index, index);

      // TODO(aee): Remove this conditional when the Polymer 2 migration
      // is completed. Polymer.DomIf exists in Polymer 2 and not in Polymer 1.
      if (Polymer.DomIf) {
        this.notifyPath(`items_.${index}`);
      } else {
        this.notifySplices('items_', [{
                             index: index,
                             addedCount: 0,
                             object: this.items_,
                             type: 'splice',
                             removed: [],
                           }]);
      }
      this.async(() => {
        const list = /** @type {!IronListElement} */ (this.$.downloadsList);
        list.updateSizeForIndex(index);
      });
    },

    // Override FindShortcutBehavior methods.
    handleFindShortcut: function(modalContextOpen) {
      if (modalContextOpen) {
        return false;
      }
      this.$.toolbar.focusOnSearchInput();
      return true;
    },

    // Override FindShortcutBehavior methods.
    searchInputHasFocus: function() {
      return this.$.toolbar.isSearchFocused();
    },
  });

  /** @return {!downloads.Manager} */
  Manager.get = function() {
    return /** @type {!downloads.Manager} */ (
        queryRequiredElement('downloads-manager'));
  };
  /** @return {!Promise} */
  Manager.onLoad = function() {
    return Manager.get().onLoad_();
  };

  return {Manager: Manager};
});
