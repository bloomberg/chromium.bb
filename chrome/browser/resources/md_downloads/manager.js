// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  var Manager = Polymer({
    is: 'downloads-manager',

    created: function() {
      /** @private {!downloads.ActionService} */
      this.actionService_ = new downloads.ActionService;
    },

    properties: {
      hasDownloads_: {
        type: Boolean,
        value: false,
      },
    },

    ready: function() {
      window.addEventListener('resize', this.onResize_.bind(this));
      // onResize_() doesn't need to be called immediately here because it's
      // guaranteed to be called again shortly when items are received.
    },

    /**
     * @return {number} A guess at how many items could be visible at once.
     * @private
     */
    guesstimateNumberOfVisibleItems_: function() {
      var toolbarHeight = this.$.toolbar.offsetHeight;
      return Math.floor((window.innerHeight - toolbarHeight) / 46) + 1;
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
          e.canExecute = true;
          break;
      }
    },

    /**
     * @param {Event} e
     * @private
     */
    onCommand_: function(e) {
      if (e.command.id == 'clear-all-command')
        this.actionService_.clearAll();
      else if (e.command.id == 'undo-command')
        this.actionService_.undo();
    },

    /** @private */
    onLoad_: function() {
      this.$.toolbar.setActionService(this.actionService_);

      cr.ui.decorate('command', cr.ui.Command);
      document.addEventListener('canExecute', this.onCanExecute_.bind(this));
      document.addEventListener('command', this.onCommand_.bind(this));

      // Shows all downloads.
      this.actionService_.search('');
    },

    /** @private */
    onResize_: function() {
      // TODO(dbeam): expose <paper-header-panel>'s #mainContainer in Polymer.
      var container = this.$.panel.$.mainContainer;
      var scrollbarWidth = container.offsetWidth - container.clientWidth;
      this.items_.forEach(function(item) {
        item.scrollbarWidth = scrollbarWidth;
      });
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
      var oldIdMap = this.idMap_ || {};

      /** @private {!Object<!downloads.Item>} */
      this.idMap_ = {};

      /** @private {!Array<!downloads.Item>} */
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
        var item = oldIdMap[id] ||
            new downloads.Item(this.iconLoader_, this.actionService_);

        this.idMap_[id] = item;  // Associated by ID for fast lookup.
        this.items_.push(item);  // Add to sorted list for order.

        // Render |item| but don't actually add to the DOM yet. |this.items_|
        // must be fully created to be able to find the right spot to insert.
        item.update(data);

        // Collapse redundant dates.
        var prev = list[i - 1];
        item.hideDate = !!prev && prev.date_string == data.date_string;

        delete oldIdMap[id];
      }

      // Remove stale, previously rendered items from the DOM.
      for (var id in oldIdMap) {
        if (oldIdMap[id].parentNode)
          oldIdMap[id].parentNode.removeChild(oldIdMap[id]);
        delete oldIdMap[id];
      }

      for (var i = 0; i < this.items_.length; ++i) {
        var item = this.items_[i];
        if (item.parentNode)  // Already in the DOM; skip.
          continue;

        var before = null;
        // Find the next rendered item after this one, and insert before it.
        for (var j = i + 1; !before && j < this.items_.length; ++j) {
          if (this.items_[j].parentNode)
            before = this.items_[j];
        }
        // If |before| is null, |item| will just get added at the end.
        this.$['downloads-list'].insertBefore(item, before);
      }

      this.hasDownloads_ = this.size_() > 0;

      if (loadTimeData.getBoolean('allowDeletingHistory'))
        this.$.toolbar.canClearAll = this.hasDownloads_;

      this.onResize_();
    },

    /**
     * @param {!downloads.Data} data
     * @private
     */
    updateItem_: function(data) {
      this.idMap_[data.id].update(data);
      this.onResize_();
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

window.addEventListener('load', downloads.Manager.onLoad);
