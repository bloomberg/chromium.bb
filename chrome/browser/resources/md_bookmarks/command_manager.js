// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-command-manager',

  behaviors: [
    bookmarks.StoreClient,
  ],

  properties: {
    /** @type {Set<string>} */
    menuIds_: Object,
  },

  attached: function() {
    /** @private {function(!Event)} */
    this.boundOnOpenItemMenu_ = this.onOpenItemMenu_.bind(this);
    document.addEventListener('open-item-menu', this.boundOnOpenItemMenu_);

    /** @private {function(!Event)} */
    this.boundOnKeydown_ = this.onKeydown_.bind(this);
    document.addEventListener('keydown', this.boundOnKeydown_);

    /** @private {Object<Command, string>} */
    this.shortcuts_ = {};
    this.shortcuts_[Command.EDIT] = cr.isMac ? 'enter' : 'f2';
    this.shortcuts_[Command.COPY] = cr.isMac ? 'meta+c' : 'ctrl+c';
    this.shortcuts_[Command.DELETE] = cr.isMac ? 'delete backspace' : 'delete';
  },

  detached: function() {
    document.removeEventListener('open-item-menu', this.boundOnOpenItemMenu_);
    document.removeEventListener('keydown', this.boundOnKeydown_);
  },

  /**
   * Display the command context menu at (|x|, |y|) in window co-ordinates.
   * Commands will execute on the currently selected items.
   * @param {number} x
   * @param {number} y
   */
  openCommandMenuAtPosition: function(x, y) {
    this.menuIds_ = this.getState().selection.items;
    /** @type {!CrActionMenuElement} */ (this.$.dropdown)
        .showAtPosition({top: y, left: x});
  },

  /**
   * Display the command context menu positioned to cover the |target|
   * element. Commands will execute on the currently selected items.
   * @param {!Element} target
   */
  openCommandMenuAtElement: function(target) {
    this.menuIds_ = this.getState().selection.items;
    /** @type {!CrActionMenuElement} */ (this.$.dropdown).showAt(target);
  },

  closeCommandMenu: function() {
    /** @type {!CrActionMenuElement} */ (this.$.dropdown).close();
  },

  ////////////////////////////////////////////////////////////////////////////
  // Command handlers:

  /**
   * @param {Command} command
   * @param {!Set<string>} itemIds
   * @return {boolean}
   */
  canExecute: function(command, itemIds) {
    switch (command) {
      case Command.EDIT:
        return itemIds.size == 1;
      case Command.COPY:
        return itemIds.size == 1 &&
            this.containsMatchingNode_(itemIds, function(node) {
              return !!node.url;
            });
      case Command.DELETE:
        return itemIds.size > 0;
      default:
        return false;
    }
  },

  /**
   * @param {Command} command
   * @param {!Set<string>} itemIds
   */
  handle: function(command, itemIds) {
    switch (command) {
      case Command.EDIT:
        var id = Array.from(itemIds)[0];
        /** @type {!BookmarksEditDialogElement} */ (this.$.editDialog.get())
            .showEditDialog(this.getState().nodes[id]);
        break;
      case Command.COPY:
        var idList = Array.from(itemIds);
        chrome.bookmarkManagerPrivate.copy(idList, function() {
          // TODO(jiaxi): Add toast later.
        });
        break;
      case Command.DELETE:
        chrome.bookmarkManagerPrivate.removeTrees(
            Array.from(this.minimizeDeletionSet_(itemIds)), function() {
              // TODO(jiaxi): Add toast later.
            });
        break;
    }
  },

  ////////////////////////////////////////////////////////////////////////////
  // Private functions:

  /**
   * Minimize the set of |itemIds| by removing any node which has an ancestor
   * node already in the set. This ensures that instead of trying to delete both
   * a node and its descendant, we will only try to delete the topmost node,
   * preventing an error in the bookmarkManagerPrivate.removeTrees API call.
   * @param {!Set<string>} itemIds
   * @return {!Set<string>}
   */
  minimizeDeletionSet_: function(itemIds) {
    var minimizedSet = new Set();
    var nodes = this.getState().nodes;
    itemIds.forEach(function(itemId) {
      var currentId = itemId;
      while (currentId != ROOT_NODE_ID) {
        currentId = assert(nodes[currentId].parentId);
        if (itemIds.has(currentId))
          return;
      }
      minimizedSet.add(itemId);
    });
    return minimizedSet;
  },

  /**
   * @param {!Set<string>} itemIds
   * @param {function(BookmarkNode):boolean} predicate
   * @return {boolean} True if any node in |itemIds| returns true for
   *     |predicate|.
   */
  containsMatchingNode_: function(itemIds, predicate) {
    var nodes = this.getState().nodes;

    return Array.from(itemIds).some(function(id) {
      return predicate(nodes[id]);
    });
  },

  /**
   * @param {Event} e
   * @private
   */
  onOpenItemMenu_: function(e) {
    if (e.detail.targetElement) {
      this.openCommandMenuAtElement(e.detail.targetElement);
    } else {
      this.openCommandMenuAtPosition(e.detail.x, e.detail.y);
    }
  },

  /**
   * @param {Event} e
   * @private
   */
  onCommandClick_: function(e) {
    this.closeCommandMenu();
    this.handle(e.target.getAttribute('command'), assert(this.menuIds_));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onKeydown_: function(e) {
    var selection = this.getState().selection.items;
    // TODO(tsergeant): Prevent keyboard shortcuts when a dialog is open or text
    // field is focused.
    for (var commandName in this.shortcuts_) {
      var shortcut = this.shortcuts_[commandName];
      if (Polymer.IronA11yKeysBehavior.keyboardEventMatchesKeys(e, shortcut) &&
          this.canExecute(commandName, selection)) {
        this.handle(commandName, selection);

        e.stopPropagation();
        e.preventDefault();
        return;
      }
    }
  },

  /**
   * Close the menu on mousedown so clicks can propagate to the underlying UI.
   * This allows the user to right click the list while a context menu is
   * showing and get another context menu.
   * @param {Event} e
   * @private
   */
  onMenuMousedown_: function(e) {
    if (e.path[0] != this.$.dropdown)
      return;

    this.$.dropdown.close();
  },

  /** @private */
  getEditActionLabel_: function() {
    if (this.menuIds_.size > 1)
      return;

    var id = Array.from(this.menuIds_)[0];
    var itemUrl = this.getState().nodes[id].url;
    var label = itemUrl ? 'menuEdit' : 'menuRename';
    return loadTimeData.getString(label);
  },
});
