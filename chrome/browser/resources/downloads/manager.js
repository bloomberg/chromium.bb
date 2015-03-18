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
     */
    setSearchText: function(searchText) {
      this.searchText_ = searchText;

      $('downloads-summary-text').textContent = this.searchText_ ?
          loadTimeData.getStringF('searchresultsfor', this.searchText_) : '';

      // Split quoted terms (e.g., 'The "lazy" dog' => ['The', 'lazy', 'dog']).
      function trim(s) { return s.trim(); }
      chrome.send('getDownloads', searchText.split(/"([^"]*)"/).map(trim));
    },

    /**
     * Called when all items need to be updated.
     * @param {!Array<!downloads.Data>} list A list of new download data.
     */
    updateAll: function(list) {
      var oldIdMap = this.idMap_ || {};

      /** @private {!Object<!downloads.Item>} */
      this.idMap_ = {};

      /** @private {!Array<!downloads.Item>} */
      this.items_ = [];

      for (var i = 0; i < list.length; ++i) {
        var data = list[i];
        var id = data.id;

        // Re-use old items when possible (saves work, preserves focus).
        var item = oldIdMap[id] || new downloads.Item;

        this.idMap_[id] = item;  // Associated by ID for fast lookup.
        this.items_.push(item);  // Add to sorted list for order.

        // Render |item| but don't actually add to the DOM yet. |this.items_|
        // must be fully created to be able to find the right spot to insert.
        item.render(data);

        // Collapse redundant dates.
        var prev = list[i - 1];
        item.view.dateContainer.hidden =
            prev && prev.date_string == data.date_string;

        delete oldIdMap[id];
      }

      // Remove stale, previously rendered items from the DOM.
      for (var id in oldIdMap) {
        oldIdMap[id].unrender();
        delete oldIdMap[id];
      }

      for (var i = 0; i < this.items_.length; ++i) {
        var item = this.items_[i];
        if (item.view.node.parentNode)  // Already in the DOM; skip.
          continue;

        var before = null;
        // Find the next rendered item after this one, and insert before it.
        for (var j = i + 1; !before && j < this.items_.length; ++j) {
          if (this.items_[j].view.node.parentNode)
            before = this.items_[j].view.node;
        }
        // If |before| is null, |item| will just get added at the end.
        this.node_.insertBefore(item.view.node, before);
      }

      var noDownloadsOrResults = $('no-downloads-or-results');
      noDownloadsOrResults.textContent = loadTimeData.getString(
          this.searchText_ ? 'no_search_results' : 'no_downloads');

      var hasDownloads = this.size() > 0;
      this.node_.hidden = !hasDownloads;
      noDownloadsOrResults.hidden = hasDownloads;

      if (loadTimeData.getBoolean('allow_deleting_history'))
        $('clear-all').hidden = !hasDownloads || this.searchText_.length > 0;

      this.rebuildFocusGrid_();
    },

    /** @param {!downloads.Data} data Info about the item to update. */
    updateItem: function(data) {
      var activeElement = document.activeElement;

      var item = this.idMap_[data.id];
      item.render(data);
      var focusRow = this.decorateItem_(item);

      if (focusRow.contains(activeElement) &&
          !downloads.FocusRow.shouldFocus(activeElement)) {
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
        var focusRow = this.decorateItem_(item);
        this.focusGrid_.addRow(focusRow);

        if (focusRow.contains(activeElement) &&
            !downloads.FocusRow.shouldFocus(activeElement)) {
          focusRow.getEquivalentElement(activeElement).focus();
        }
      }, this);
    },

    /**
     * @param {!downloads.Item} item An item to decorate as a FocusRow.
     * @return {!downloads.FocusRow} |item| decorated as a FocusRow.
     */
    decorateItem_: function(item) {
      downloads.FocusRow.decorate(item.view.node, item.view, this.node_);
      return assertInstanceof(item.view.node, downloads.FocusRow);
    },

    /** @return {number} The number of downloads shown on the page. */
    size: function() {
      return this.items_.length;
    },

    clearAll: function() {
      if (loadTimeData.getBoolean('allow_deleting_history')) {
        chrome.send('clearAll');
        this.setSearchText('');
      }
    },

    onLoad: function() {
      this.node_ = $('downloads-display');

      $('clear-all').onclick = function() {
        this.clearAll();
      }.bind(this);

      $('open-downloads-folder').onclick = function() {
        chrome.send('openDownloadsFolder');
      };

      $('term').onsearch = function(e) {
        this.setSearchText($('term').value);
      }.bind(this);

      cr.ui.decorate('command', cr.ui.Command);
      document.addEventListener('canExecute', this.onCanExecute_.bind(this));
      document.addEventListener('command', this.onCommand_.bind(this));

      this.setSearchText('');
    },

    /**
     * @param {Event} e
     * @private
     */
    onCanExecute_: function(e) {
      e = /** @type {cr.ui.CanExecuteEvent} */(e);
      e.canExecute = e.command.id != 'undo-command' ||
                     document.activeElement != $('term');
    },

    /**
     * @param {Event} e
     * @private
     */
    onCommand_: function(e) {
      if (e.command.id == 'undo-command')
        chrome.send('undo');
      else if (e.command.id == 'clear-all-command')
        this.clearAll();
    },
  };

  Manager.updateAll = function(list) {
    Manager.getInstance().updateAll(list);
  };

  Manager.updateItem = function(item) {
    Manager.getInstance().updateItem(item);
  };

  Manager.setSearchText = function(searchText) {
    Manager.getInstance().setSearchText(searchText);
  };

  Manager.onLoad = function() {
    Manager.getInstance().onLoad();
  };

  Manager.size = function() {
    return Manager.getInstance().size();
  };

  return {Manager: Manager};
});

window.addEventListener('DOMContentLoaded', downloads.Manager.onLoad);
