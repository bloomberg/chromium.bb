// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  var Manager = Polymer({
    is: 'downloads-manager',

    properties: {
      hasDownloads_: {
        type: Boolean,
        value: false,
      },

      items_: {
        type: Array,
      },
    },

    /**
     * @param {Event} e
     * @private
     */
    onCanExecute_: function(e) {
      e = /** @type {cr.ui.CanExecuteEvent} */(e);
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
      if (e.command.id == 'clear-all-command')
        downloads.ActionService.getInstance().clearAll();
      else if (e.command.id == 'undo-command')
        downloads.ActionService.getInstance().undo();
    },

    /** @private */
    onLoad_: function() {
      cr.ui.decorate('command', cr.ui.Command);
      document.addEventListener('canExecute', this.onCanExecute_.bind(this));
      document.addEventListener('command', this.onCommand_.bind(this));

      // Shows all downloads.
      downloads.ActionService.getInstance().search('');
    },

    /**
     * @return {number} The number of downloads shown on the page.
     * @private
     */
    size_: function() {
      return this.items_.length;
    },

    /**
     * Called when all items need to be updated.
     * @param {!Array<!downloads.Data>} list A list of new download data.
     * @private
     */
    updateAll_: function(list) {
      /** @private {!Object<number>} */
      this.idToIndex_ = {};

      for (var i = 0; i < list.length; ++i) {
        var data = list[i];

        this.idToIndex_[data.id] = data.index = i;

        var prev = list[i - 1];
        data.hideDate = !!prev && prev.date_string == data.date_string;
      }

      // TODO(dbeam): this resets the scroll position, which is a huge bummer.
      // Removing something from the bottom of the list should not scroll you
      // back to the top. The grand plan is to restructure how the C++ sends the
      // JS data so that it only gets updates (rather than the most recent set
      // of items). TL;DR - we can't ship with this bug.
      this.items_ = list;

      var hasDownloads = this.size_() > 0;
      if (!hasDownloads) {
        var isSearching = downloads.ActionService.getInstance().isSearching();
        var messageToShow = isSearching ? 'noSearchResults' : 'noDownloads';
        this.$['no-downloads'].querySelector('span').textContent =
            loadTimeData.getString(messageToShow);
      }
      this.hasDownloads_ = hasDownloads;

      if (loadTimeData.getBoolean('allowDeletingHistory'))
        this.$.toolbar.downloadsShowing = this.hasDownloads_;

      this.$.panel.classList.remove('loading');
    },

    /**
     * @param {!downloads.Data} data
     * @private
     */
    updateItem_: function(data) {
      var index = this.idToIndex_[data.id];
      this.set('items_.' + index, data);
      this.$['downloads-list'].updateSizeForItem(index);
    },
  });

  Manager.size = function() {
    return document.querySelector('downloads-manager').size_();
  };

  Manager.updateAll = function(list) {
    document.querySelector('downloads-manager').updateAll_(list);
  };

  Manager.updateItem = function(item) {
    document.querySelector('downloads-manager').updateItem_(item);
  };

  Manager.onLoad = function() {
    document.querySelector('downloads-manager').onLoad_();
  };

  return {Manager: Manager};
});
