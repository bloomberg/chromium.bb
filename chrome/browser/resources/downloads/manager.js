// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  /**
   * Class to own and manage download items.
   * @constructor
   */
  function Manager() {}

  cr.addSingletonGetter(Manager);

  Manager.prototype = {
    /** @private {string} */
    searchText_: '',

    /**
     * Sets the search text, updates related UIs, and tells the browser.
     * @param {string} searchText Text we're searching for.
     * @private
     */
    setSearchText_: function(searchText) {
      this.searchText_ = searchText;

      $('downloads-summary-text').textContent = this.searchText_ ?
          loadTimeData.getStringF('searchResultsFor', this.searchText_) : '';

      // Split quoted terms (e.g., 'The "lazy" dog' => ['The', 'lazy', 'dog']).
      function trim(s) { return s.trim(); }
      chrome.send('getDownloads', searchText.split(/"([^"]*)"/).map(trim));
    },

    /**
     * @return {number} A guess at how many items could be visible at once.
     * @private
     */
    guesstimateNumberOfVisibleItems_: function() {
      var headerHeight = document.querySelector('header').offsetHeight;
      var summaryHeight = $('downloads-summary').offsetHeight;
      var nonItemSpace = headerHeight + summaryHeight;
      return Math.floor((window.innerHeight - nonItemSpace) / 46) + 1;
    },

    /**
     * Called when all items need to be updated.
     * @param {!Array<!downloads.Data>} list A list of new download data.
     * @private
     */
    updateAll_: function(list) {
      var oldIdMap = this.idMap_ || {};

      /** @private {!Object<!downloads.ItemView>} */
      this.idMap_ = {};

      /** @private {!Array<!downloads.ItemView>} */
      this.items_ = [];

      if (!this.iconLoader_) {
        var guesstimate = Math.max(this.guesstimateNumberOfVisibleItems_(), 1);
        /** @private {downloads.ThrottledIconLoader} */
        this.iconLoader_ = new downloads.ThrottledIconLoader(guesstimate);
      }

      for (var i = 0; i < list.length; ++i) {
        var data = list[i];
        var id = data.id;

        // Re-use old items when possible (saves work, preserves focus).
        var item = oldIdMap[id] || new downloads.ItemView(this.iconLoader_);

        this.idMap_[id] = item;  // Associated by ID for fast lookup.
        this.items_.push(item);  // Add to sorted list for order.

        // Render |item| but don't actually add to the DOM yet. |this.items_|
        // must be fully created to be able to find the right spot to insert.
        item.update(data);

        // Collapse redundant dates.
        var prev = list[i - 1];
        item.dateContainer.hidden =
            prev && prev.date_string == data.date_string;

        delete oldIdMap[id];
      }

      // Remove stale, previously rendered items from the DOM.
      for (var id in oldIdMap) {
        var oldNode = oldIdMap[id].node;
        if (oldNode.parentNode)
          oldNode.parentNode.removeChild(oldNode);
        delete oldIdMap[id];
      }

      for (var i = 0; i < this.items_.length; ++i) {
        var item = this.items_[i];
        if (item.node.parentNode)  // Already in the DOM; skip.
          continue;

        var before = null;
        // Find the next rendered item after this one, and insert before it.
        for (var j = i + 1; !before && j < this.items_.length; ++j) {
          if (this.items_[j].node.parentNode)
            before = this.items_[j].node;
        }
        // If |before| is null, |item| will just get added at the end.
        this.node_.insertBefore(item.node, before);
      }

      var noDownloadsOrResults = $('no-downloads-or-results');
      noDownloadsOrResults.textContent = loadTimeData.getString(
          this.searchText_ ? 'noSearchResults' : 'noDownloads');

      var hasDownloads = this.size_() > 0;
      this.node_.hidden = !hasDownloads;
      noDownloadsOrResults.hidden = hasDownloads;

      if (loadTimeData.getBoolean('allowDeletingHistory'))
        $('clear-all').hidden = !hasDownloads || this.searchText_.length > 0;

      this.rebuildFocusGrid_();
    },

    /**
     * @param {!downloads.Data} data Info about the item to update.
     * @private
     */
    updateItem_: function(data) {
      var activeElement = document.activeElement;

      var item = this.idMap_[data.id];
      item.update(data);

      if (item.node.contains(activeElement) &&
          !cr.ui.FocusRow.isFocusable(activeElement)) {
        var focusRow = this.focusGrid_.getRowForRoot(item.node);
        focusRow.getEquivalentElement(activeElement).focus();
      }
    },

    /**
     * Rebuild the focusGrid_ using the elements that each download will have.
     * @private
     */
    rebuildFocusGrid_: function() {
      var activeElement = document.activeElement;

      /** @private {!cr.ui.FocusGrid} */
      this.focusGrid_ = this.focusGrid_ || new cr.ui.FocusGrid();
      this.focusGrid_.destroy();

      this.items_.forEach(function(item) {
        var focusRow = new downloads.FocusRow(item.node, this.node_);

        this.focusGrid_.addRow(focusRow);

        if (item.node.contains(activeElement) &&
            !cr.ui.FocusRow.isFocusable(activeElement)) {
          focusRow.getEquivalentElement(activeElement).focus();
        }
      }, this);

      this.focusGrid_.ensureRowActive();
    },

    /**
     * @return {number} The number of downloads shown on the page.
     * @private
     */
    size_: function() {
      return this.items_.length;
    },

    /** @private */
    clearAll_: function() {
      if (loadTimeData.getBoolean('allowDeletingHistory')) {
        chrome.send('clearAll');
        this.setSearchText_('');
      }
    },

    /** @private */
    onLoad_: function() {
      this.node_ = $('downloads-display');

      $('clear-all').onclick = function() {
        this.clearAll_();
      }.bind(this);

      $('open-downloads-folder').onclick = function() {
        chrome.send('openDownloadsFolder');
      };

      $('term').onsearch = function(e) {
        this.setSearchText_($('term').value);
      }.bind(this);

      cr.ui.decorate('command', cr.ui.Command);
      document.addEventListener('canExecute', this.onCanExecute_.bind(this));
      document.addEventListener('command', this.onCommand_.bind(this));

      this.setSearchText_('');
    },

    /**
     * @param {Event} e
     * @private
     */
    onCanExecute_: function(e) {
      e = /** @type {cr.ui.CanExecuteEvent} */(e);
      switch (e.command.id) {
        case 'undo-command':
          e.canExecute = document.activeElement != $('term');
          break;
        case 'clear-all-command':
          e.canExecute = true;
          break;
      }
    },

    /**
     * @param {Event} e
     * @private
     */
    onCommand_: function(e) {
      if (e.command.id == 'undo-command')
        chrome.send('undo');
      else if (e.command.id == 'clear-all-command')
        this.clearAll_();
    },
  };

  Manager.updateAll = function(list) {
    Manager.getInstance().updateAll_(list);
  };

  Manager.updateItem = function(item) {
    Manager.getInstance().updateItem_(item);
  };

  Manager.setSearchText = function(searchText) {
    Manager.getInstance().setSearchText_(searchText);
  };

  Manager.onLoad = function() {
    Manager.getInstance().onLoad_();
  };

  Manager.size = function() {
    return Manager.getInstance().size_();
  };

  return {Manager: Manager};
});

window.addEventListener('DOMContentLoaded', downloads.Manager.onLoad);
