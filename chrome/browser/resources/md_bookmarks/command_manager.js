// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Element which shows context menus and handles keyboard
 * shortcuts.
 */
cr.define('bookmarks', function() {

  var CommandManager = Polymer({
    is: 'bookmarks-command-manager',

    behaviors: [
      bookmarks.StoreClient,
    ],

    properties: {
      /** @private {!Array<Command>} */
      menuCommands_: {
        type: Array,
        value: function() {
          return [
            Command.EDIT,
            Command.COPY,
            Command.DELETE,
            // <hr>
            Command.OPEN_NEW_TAB,
            Command.OPEN_NEW_WINDOW,
            Command.OPEN_INCOGNITO,
          ];
        },
      },

      /** @private {Set<string>} */
      menuIds_: Object,

      /** @private */
      globalCanEdit_: Boolean,
    },

    attached: function() {
      assert(CommandManager.instance_ == null);
      CommandManager.instance_ = this;

      this.watch('globalCanEdit_', function(state) {
        return state.prefs.canEdit;
      });
      this.updateFromStore();

      /** @private {function(!Event)} */
      this.boundOnOpenItemMenu_ = this.onOpenItemMenu_.bind(this);
      document.addEventListener('open-item-menu', this.boundOnOpenItemMenu_);

      /** @private {function()} */
      this.boundOnCommandUndo_ = function() {
        this.handle(Command.UNDO, new Set());
      }.bind(this);
      document.addEventListener('command-undo', this.boundOnCommandUndo_);

      /** @private {function(!Event)} */
      this.boundOnKeydown_ = this.onKeydown_.bind(this);
      document.addEventListener('keydown', this.boundOnKeydown_);

      /** @private {Object<Command, cr.ui.KeyboardShortcutList>} */
      this.shortcuts_ = {};

      this.addShortcut_(Command.EDIT, 'F2', 'Enter');
      this.addShortcut_(Command.COPY, 'Ctrl|c', 'Meta|c');
      this.addShortcut_(Command.DELETE, 'Delete', 'Delete Backspace');

      this.addShortcut_(Command.OPEN, 'Enter', 'Meta|ArrowDown Meta|o');
      this.addShortcut_(Command.OPEN_NEW_TAB, 'Ctrl|Enter', 'Meta|Enter');
      this.addShortcut_(Command.OPEN_NEW_WINDOW, 'Shift|Enter');

      this.addShortcut_(Command.UNDO, 'Ctrl|z', 'Meta|z');
      this.addShortcut_(Command.REDO, 'Ctrl|y Ctrl|Shift|Z', 'Meta|Shift|Z');
    },

    detached: function() {
      CommandManager.instance_ = null;
      document.removeEventListener('open-item-menu', this.boundOnOpenItemMenu_);
      document.removeEventListener('command-undo', this.boundOnCommandUndo_);
      document.removeEventListener('keydown', this.boundOnKeydown_);
    },

    /**
     * Display the command context menu at (|x|, |y|) in window co-ordinates.
     * Commands will execute on |items| if given, or on the currently selected
     * items.
     * @param {number} x
     * @param {number} y
     * @param {Set<string>=} items
     */
    openCommandMenuAtPosition: function(x, y, items) {
      this.menuIds_ = items || this.getState().selection.items;
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
      this.menuIds_ = new Set();
      /** @type {!CrActionMenuElement} */ (this.$.dropdown).close();
    },

    ////////////////////////////////////////////////////////////////////////////
    // Command handlers:

    /**
     * Determine if the |command| can be executed with the given |itemIds|.
     * Commands which appear in the context menu should be implemented
     * separately using `isCommandVisible_` and `isCommandEnabled_`.
     * @param {Command} command
     * @param {!Set<string>} itemIds
     * @return {boolean}
     */
    canExecute: function(command, itemIds) {
      switch (command) {
        case Command.OPEN:
          return itemIds.size > 0;
        case Command.UNDO:
        case Command.REDO:
          return this.globalCanEdit_;
        default:
          return this.isCommandVisible_(command, itemIds) &&
              this.isCommandEnabled_(command, itemIds);
      }
    },

    /**
     * @param {Command} command
     * @param {!Set<string>} itemIds
     * @return {boolean} True if the command should be visible in the context
     *     menu.
     */
    isCommandVisible_: function(command, itemIds) {
      switch (command) {
        case Command.EDIT:
          return itemIds.size == 1 && this.globalCanEdit_;
        case Command.COPY:
          return this.isSingleBookmark_(itemIds);
        case Command.DELETE:
          return itemIds.size > 0 && this.globalCanEdit_;
        case Command.OPEN_NEW_TAB:
        case Command.OPEN_NEW_WINDOW:
        case Command.OPEN_INCOGNITO:
          return itemIds.size > 0;
        default:
          return false;
      }
    },

    /**
     * @param {Command} command
     * @param {!Set<string>} itemIds
     * @return {boolean} True if the command should be clickable in the context
     *     menu.
     */
    isCommandEnabled_: function(command, itemIds) {
      switch (command) {
        case Command.EDIT:
        case Command.DELETE:
          var state = this.getState();
          return !this.containsMatchingNode_(itemIds, function(node) {
            return !bookmarks.util.canEditNode(state, node.id);
          });
        case Command.OPEN_NEW_TAB:
        case Command.OPEN_NEW_WINDOW:
          return this.expandUrls_(itemIds).length > 0;
        case Command.OPEN_INCOGNITO:
          return this.expandUrls_(itemIds).length > 0 &&
              this.getState().prefs.incognitoAvailability !=
              IncognitoAvailability.DISABLED;
        default:
          return true;
      }
    },

    /**
     * @param {Command} command
     * @param {!Set<string>} itemIds
     */
    handle: function(command, itemIds) {
      var state = this.getState();
      switch (command) {
        case Command.EDIT:
          var id = Array.from(itemIds)[0];
          /** @type {!BookmarksEditDialogElement} */ (this.$.editDialog.get())
              .showEditDialog(state.nodes[id]);
          break;
        case Command.COPY:
          var idList = Array.from(itemIds);
          chrome.bookmarkManagerPrivate.copy(idList, function() {
            bookmarks.ToastManager.getInstance().show(
                loadTimeData.getString('toastUrlCopied'), false);
          });
          break;
        case Command.DELETE:
          var idList = Array.from(this.minimizeDeletionSet_(itemIds));
          var title = state.nodes[idList[0]].title;
          var labelPromise = cr.sendWithPromise(
              'getPluralString', 'toastItemsDeleted', idList.length);
          chrome.bookmarkManagerPrivate.removeTrees(idList, function() {
            labelPromise.then(function(label) {
              var pieces = loadTimeData.getSubstitutedStringPieces(label, title)
                               .map(function(p) {
                                 // Make the bookmark name collapsible.
                                 p.collapsible = !!p.arg;
                                 return p;
                               });
              bookmarks.ToastManager.getInstance().showForStringPieces(
                  pieces, true);
            }.bind(this));
          }.bind(this));
          break;
        case Command.UNDO:
          chrome.bookmarkManagerPrivate.undo();
          bookmarks.ToastManager.getInstance().hide();
          break;
        case Command.REDO:
          chrome.bookmarkManagerPrivate.redo();
          break;
        case Command.OPEN_NEW_TAB:
        case Command.OPEN_NEW_WINDOW:
        case Command.OPEN_INCOGNITO:
          this.openUrls_(this.expandUrls_(itemIds), command);
          break;
        case Command.OPEN:
          var isFolder = itemIds.size == 1 &&
              this.containsMatchingNode_(itemIds, function(node) {
                return !node.url;
              });
          if (isFolder) {
            var folderId = Array.from(itemIds)[0];
            this.dispatch(
                bookmarks.actions.selectFolder(folderId, state.nodes));
          } else {
            this.openUrls_(this.expandUrls_(itemIds), command);
          }
          break;
        default:
          assert(false);
      }
    },

    /**
     * @param {!Event} e
     * @param {!Set<string>} itemIds
     * @return {boolean} True if the event was handled, triggering a keyboard
     *     shortcut.
     */
    handleKeyEvent: function(e, itemIds) {
      for (var commandName in this.shortcuts_) {
        var shortcut = this.shortcuts_[commandName];
        if (shortcut.matchesEvent(e) && this.canExecute(commandName, itemIds)) {
          this.handle(commandName, itemIds);

          e.stopPropagation();
          e.preventDefault();
          return true;
        }
      }

      return false;
    },

    ////////////////////////////////////////////////////////////////////////////
    // Private functions:

    /**
     * Register a keyboard shortcut for a command.
     * @param {Command} command Command that the shortcut will trigger.
     * @param {string} shortcut Keyboard shortcut, using the syntax of
     *     cr/ui/command.js.
     * @param {string=} macShortcut If set, enables a replacement shortcut for
     *     Mac.
     */
    addShortcut_: function(command, shortcut, macShortcut) {
      var shortcut = (cr.isMac && macShortcut) ? macShortcut : shortcut;
      this.shortcuts_[command] = new cr.ui.KeyboardShortcutList(shortcut);
    },

    /**
     * Minimize the set of |itemIds| by removing any node which has an ancestor
     * node already in the set. This ensures that instead of trying to delete
     * both a node and its descendant, we will only try to delete the topmost
     * node, preventing an error in the bookmarkManagerPrivate.removeTrees API
     * call.
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
     * @param {!Array<string>} urls
     * @param {Command} command
     * @private
     */
    openUrls_: function(urls, command) {
      assert(
          command == Command.OPEN || command == Command.OPEN_NEW_TAB ||
          command == Command.OPEN_NEW_WINDOW ||
          command == Command.OPEN_INCOGNITO);

      if (urls.length == 0)
        return;

      var incognito = command == Command.OPEN_INCOGNITO;
      if (command == Command.OPEN_NEW_WINDOW || incognito) {
        chrome.windows.create({url: urls, incognito: incognito});
      } else {
        if (command == Command.OPEN)
          chrome.tabs.create({url: urls.shift(), active: true});
        urls.forEach(function(url) {
          chrome.tabs.create({url: url, active: false});
        });
      }
    },

    /**
     * Returns all URLs in the given set of nodes and their immediate children.
     * Note that these will be ordered by insertion order into the |itemIds|
     * set, and that it is possible to duplicate a URL by passing in both the
     * parent ID and child ID.
     * @param {!Set<string>} itemIds
     * @return {!Array<string>}
     * @private
     */
    expandUrls_: function(itemIds) {
      var urls = [];
      var nodes = this.getState().nodes;

      itemIds.forEach(function(id) {
        var node = nodes[id];
        if (node.url) {
          urls.push(node.url);
        } else {
          node.children.forEach(function(childId) {
            var childNode = nodes[childId];
            if (childNode.url)
              urls.push(childNode.url);
          });
        }
      });

      return urls;
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
     * @param {!Set<string>} itemIds
     * @return {boolean} True if |itemIds| is a single bookmark (non-folder)
     *     node.
     */
    isSingleBookmark_: function(itemIds) {
      return itemIds.size == 1 &&
          this.containsMatchingNode_(itemIds, function(node) {
            return !!node.url;
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
      this.handle(e.target.getAttribute('command'), assert(this.menuIds_));
      this.closeCommandMenu();
    },

    /**
     * @param {!Event} e
     * @private
     */
    onKeydown_: function(e) {
      var selection = this.getState().selection.items;
      if (e.target == document.body)
        this.handleKeyEvent(e, selection);
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

      this.closeCommandMenu();
    },

    /**
     * @param {Command} command
     * @return {string}
     * @private
     */
    getCommandLabel_: function(command) {
      var multipleNodes = this.menuIds_.size > 1 ||
          this.containsMatchingNode_(this.menuIds_, function(node) {
            return !node.url;
          });
      var label;
      switch (command) {
        case Command.EDIT:
          if (this.menuIds_.size != 1)
            return '';

          var id = Array.from(this.menuIds_)[0];
          var itemUrl = this.getState().nodes[id].url;
          label = itemUrl ? 'menuEdit' : 'menuRename';
          break;
        case Command.COPY:
          label = 'menuCopyURL';
          break;
        case Command.DELETE:
          label = 'menuDelete';
          break;
        case Command.OPEN_NEW_TAB:
          label = multipleNodes ? 'menuOpenAllNewTab' : 'menuOpenNewTab';
          break;
        case Command.OPEN_NEW_WINDOW:
          label = multipleNodes ? 'menuOpenAllNewWindow' : 'menuOpenNewWindow';
          break;
        case Command.OPEN_INCOGNITO:
          label = multipleNodes ? 'menuOpenAllIncognito' : 'menuOpenIncognito';
          break;
      }

      return loadTimeData.getString(assert(label));
    },

    /**
     * @param {Command} command
     * @return {boolean}
     * @private
     */
    showDividerAfter_: function(command, itemIds) {
      return command == Command.DELETE &&
          (this.globalCanEdit_ || this.isSingleBookmark_(itemIds));
    },
  });

  /** @private {bookmarks.CommandManager} */
  CommandManager.instance_ = null;

  /** @return {!bookmarks.CommandManager} */
  CommandManager.getInstance = function() {
    return assert(CommandManager.instance_);
  };

  return {
    CommandManager: CommandManager,
  };
});
