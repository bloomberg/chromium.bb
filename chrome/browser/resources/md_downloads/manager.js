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
      /** @private {!Array} */
      this.items_ = list;

      var noDownloadsOrResults = $('no-downloads-or-results');
      noDownloadsOrResults.textContent = loadTimeData.getString(
          this.searchText_ ? 'no_search_results' : 'no_downloads');

      var hasDownloads = this.size() > 0;
      this.node_.hidden = !hasDownloads;
      noDownloadsOrResults.hidden = hasDownloads;

      if (loadTimeData.getBoolean('allow_deleting_history'))
        $('clear-all').hidden = !hasDownloads || this.searchText_.length > 0;
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

      $('search-button').onclick = function() {
        if (!$('search-term').hidden)
          return;
        $('clear-search').hidden = false;
        $('search-term').hidden = false;
      };

      $('clear-search').onclick = function() {
        $('clear-search').hidden = true;
        $('search-term').hidden = true;
        $('search-term').value = '';
        this.setSearchText('');
      }.bind(this);

      // TODO(dbeam): this previously used onsearch, which batches keystrokes
      // together. This should probably be re-instated eventually.
      $('search-term').oninput = function(e) {
        this.setSearchText($('search-term').value);
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
                     $('search-term').contains(document.activeElement);
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

  Manager.updateItem = function(item) {};

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
