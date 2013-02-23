// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
/** @const */ var BookmarkList = bmm.BookmarkList;
/** @const */ var BookmarkTree = bmm.BookmarkTree;
/** @const */ var Command = cr.ui.Command;
/** @const */ var CommandBinding = cr.ui.CommandBinding;
/** @const */ var LinkKind = cr.LinkKind;
/** @const */ var ListItem = cr.ui.ListItem;
/** @const */ var Menu = cr.ui.Menu;
/** @const */ var MenuButton = cr.ui.MenuButton;
/** @const */ var Promise = cr.Promise;
/** @const */ var Splitter = cr.ui.Splitter;
/** @const */ var TreeItem = cr.ui.TreeItem;

/**
 * Delay for expanding folder when pointer hovers on folder in tree view in
 * milliseconds.
 * @type {number}
 * @const
 */
// TODO(yosin): EXPAND_FOLDER_DELAY should follow system settings. 400ms is
// taken from Windows default settings.
var EXPAND_FOLDER_DELAY = 400;

/**
 * An array containing the BookmarkTreeNodes that were deleted in the last
 * deletion action. This is used for implementing undo.
 * @type {Array.<BookmarkTreeNode>}
 */
var lastDeletedNodes;

/**
 *
 * Holds the last DOMTimeStamp when mouse pointer hovers on folder in tree
 * view. Zero means pointer doesn't hover on folder.
 * @type {number}
 */
var lastHoverOnFolderTimeStamp = 0;

/**
 * Holds a function that will undo that last action, if global undo is enabled.
 * @type {Function}
 */
var performGlobalUndo;

/**
 * Holds a link controller singleton. Use getLinkController() rarther than
 * accessing this variabie.
 * @type {LinkController}
 */
var linkController;

/**
 * New Windows are not allowed in Windows 8 metro mode.
 */
var canOpenNewWindows = true;

/**
 * Incognito mode availability can take the following values: ,
 *   - 'enabled' for when both normal and incognito modes are available;
 *   - 'disabled' for when incognito mode is disabled;
 *   - 'forced' for when incognito mode is forced (normal mode is unavailable).
 */
var incognitoModeAvailability = 'enabled';

/**
 * Whether bookmarks can be modified.
 * @type {boolean}
 */
var canEdit = true;

/**
 * @type {TreeItem}
 * @const
 */
var searchTreeItem = new TreeItem({
  bookmarkId: 'q='
});

/**
 * @type {TreeItem}
 * @const
 */
var recentTreeItem = new TreeItem({
  icon: 'images/bookmark_manager_recent.png',
  bookmarkId: 'recent'
});

/**
 * Command shortcut mapping.
 * @const
 */
var commandShortcutMap = cr.isMac ? {
  'edit': 'Enter',
  // On Mac we also allow Meta+Backspace.
  'delete': 'U+007F  U+0008 Meta-U+0008',
  'open-in-background-tab': 'Meta-Enter',
  'open-in-new-tab': 'Shift-Meta-Enter',
  'open-in-same-window': 'Meta-Down',
  'open-in-new-window': 'Shift-Enter',
  'rename-folder': 'Enter',
  // Global undo is Command-Z. It is not in any menu.
  'undo': 'Meta-U+005A',
} : {
  'edit': 'F2',
  'delete': 'U+007F',
  'open-in-background-tab': 'Ctrl-Enter',
  'open-in-new-tab': 'Shift-Ctrl-Enter',
  'open-in-same-window': 'Enter',
  'open-in-new-window': 'Shift-Enter',
  'rename-folder': 'F2',
  // Global undo is Ctrl-Z. It is not in any menu.
  'undo': 'Ctrl-U+005A',
};

/**
 * Adds an event listener to a node that will remove itself after firing once.
 * @param {!Element} node The DOM node to add the listener to.
 * @param {string} name The name of the event listener to add to.
 * @param {function(Event)} handler Function called when the event fires.
 */
function addOneShotEventListener(node, name, handler) {
  var f = function(e) {
    handler(e);
    node.removeEventListener(name, f);
  };
  node.addEventListener(name, f);
}

// Get the localized strings from the backend via bookmakrManagerPrivate API.
function loadLocalizedStrings(data) {
  // The strings may contain & which we need to strip.
  for (var key in data) {
    data[key] = data[key].replace(/&/, '');
  }

  loadTimeData.data = data;
  i18nTemplate.process(document, loadTimeData);

  recentTreeItem.label = loadTimeData.getString('recent');
  searchTreeItem.label = loadTimeData.getString('search');
  searchTreeItem.icon = isRTL() ? 'images/bookmark_manager_search_rtl.png' :
                                  'images/bookmark_manager_search.png';
}

/**
 * Updates the location hash to reflect the current state of the application.
 */
function updateHash() {
  window.location.hash = tree.selectedItem.bookmarkId;
}

/**
 * Navigates to a bookmark ID.
 * @param {string} id The ID to navigate to.
 * @param {function()} callback Function called when list view loaded or
 *     displayed specified folder.
 */
function navigateTo(id, callback) {
  if (list.parentId == id) {
    callback();
    return;
  }

  addOneShotEventListener(list, 'load', callback);
  updateParentId(id);
}

/**
 * Updates the parent ID of the bookmark list and selects the correct tree item.
 * @param {string} id The id.
 */
function updateParentId(id) {
  // Setting list.parentId fires 'load' event.
  list.parentId = id;

  // When tree.selectedItem changed, tree view calls navigatTo() then it
  // calls updateHash() when list view displayed specified folder.
  tree.selectedItem = bmm.treeLookup[id] || tree.selectedItem;
}

// Process the location hash. This is called by onhashchange and when the page
// is first loaded.
function processHash() {
  var id = window.location.hash.slice(1);
  if (!id) {
    // If we do not have a hash, select first item in the tree.
    id = tree.items[0].bookmarkId;
  }

  var valid = false;
  if (/^e=/.test(id)) {
    id = id.slice(2);

    // If hash contains e=, edit the item specified.
    chrome.bookmarks.get(id, function(bookmarkNodes) {
      // Verify the node to edit is a valid node.
      if (!bookmarkNodes || bookmarkNodes.length != 1)
        return;
      var bookmarkNode = bookmarkNodes[0];

      // After the list reloads, edit the desired bookmark.
      var editBookmark = function(e) {
        var index = list.dataModel.findIndexById(bookmarkNode.id);
        if (index != -1) {
          var sm = list.selectionModel;
          sm.anchorIndex = sm.leadIndex = sm.selectedIndex = index;
          scrollIntoViewAndMakeEditable(index);
        }
      };

      navigateTo(bookmarkNode.parentId, editBookmark);
    });

    // We handle the two cases of navigating to the bookmark to be edited
    // above. Don't run the standard navigation code below.
    return;
  } else if (/^q=/.test(id)) {
    // In case we got a search hash, update the text input and the
    // bmm.treeLookup to use the new id.
    setSearch(id.slice(2));
    valid = true;
  } else if (id == 'recent') {
    valid = true;
  }

  // Navigate to bookmark 'id' (which may be a query of the form q=query).
  if (valid) {
    updateParentId(id);
  } else {
    // We need to verify that this is a correct ID.
    chrome.bookmarks.get(id, function(items) {
      if (items && items.length == 1)
        updateParentId(id);
    });
  }
}

// Activate is handled by the open-in-same-window-command.
function handleDoubleClickForList(e) {
  if (e.button == 0)
    $('open-in-same-window-command').execute();
}

// The list dispatches an event when the user clicks on the URL or the Show in
// folder part.
function handleUrlClickedForList(e) {
  getLinkController().openUrlFromEvent(e.url, e.originalEvent);
  chrome.bookmarkManagerPrivate.recordLaunch();
}

function handleSearch(e) {
  setSearch(this.value);
}

/**
 * Navigates to the search results for the search text.
 * @param {string} searchText The text to search for.
 */
function setSearch(searchText) {
  if (searchText) {
    // Only update search item if we have a search term. We never want the
    // search item to be for an empty search.
    delete bmm.treeLookup[searchTreeItem.bookmarkId];
    var id = searchTreeItem.bookmarkId = 'q=' + searchText;
    bmm.treeLookup[searchTreeItem.bookmarkId] = searchTreeItem;
  }

  var input = $('term');
  // Do not update the input if the user is actively using the text input.
  if (document.activeElement != input)
    input.value = searchText;

  if (searchText) {
    tree.add(searchTreeItem);
    tree.selectedItem = searchTreeItem;
  } else {
    // Go "home".
    tree.selectedItem = tree.items[0];
    id = tree.selectedItem.bookmarkId;
  }

  // Navigate now and update hash immediately.
  navigateTo(id, updateHash);
}

// Handle the logo button UI.
// When the user clicks the button we should navigate "home" and focus the list.
function handleClickOnLogoButton(e) {
  setSearch('');
  $('list').focus();
}

/**
 * This returns the user visible path to the folder where the bookmark is
 * located.
 * @param {number} parentId The ID of the parent folder.
 * @return {string} The path to the the bookmark,
 */
function getFolder(parentId) {
  var parentNode = tree.getBookmarkNodeById(parentId);
  if (parentNode) {
    var s = parentNode.title;
    if (parentNode.parentId != bmm.ROOT_ID) {
      return getFolder(parentNode.parentId) + '/' + s;
    }
    return s;
  }
}

function handleLoadForTree(e) {
  // Add hard coded tree items.
  tree.add(recentTreeItem);
  processHash();
}

var dnd = {
  dragData: null,

  getBookmarkElement: function(el) {
    while (el && !el.bookmarkNode) {
      el = el.parentNode;
    }
    return el;
  },

  // If we are over the list and the list is showing recent or search result,
  // we cannot drop.
  isOverRecentOrSearch: function(overElement) {
    return (list.isRecent() || list.isSearch()) && list.contains(overElement);
  },

  checkEvery_: function(f, overBookmarkNode, overElement) {
    return this.dragData.elements.every(function(element) {
      return f.call(this, element, overBookmarkNode, overElement);
    }, this);
  },

  /**
   * @return {boolean} Whether we are currently dragging any folders.
   */
  isDraggingFolders: function() {
    return !!this.dragData && this.dragData.elements.some(function(node) {
      return !node.url;
    });
  },

  /**
   * This is a first pass whether we can drop the dragged items.
   *
   * @param {!BookmarkTreeNode} overBookmarkNode The bookmark that we are
   *     currently dragging over.
   * @param {!HTMLElement} overElement The element that we are currently
   *     dragging over.
   * @return {boolean} If this returns false then we know we should not drop
   *     the items. If it returns true we still have to call canDropOn,
   *     canDropAbove and canDropBelow.
   */
  canDrop: function(overBookmarkNode, overElement) {
    var dragData = this.dragData;
    if (!dragData)
      return false;

    if (this.isOverRecentOrSearch(overElement))
      return false;

    if (!dragData.sameProfile)
      return true;

    return this.checkEvery_(this.canDrop_, overBookmarkNode, overElement);
  },

  /**
   * Helper for canDrop that only checks one bookmark node.
   * @private
   * @return {boolean} False if dragNode is overBookmarkNode or dragNode is
   *     a folder and overBookmarkNode is descendant of dragNode, otherwise
   *     true.
   */
  canDrop_: function(dragNode, overBookmarkNode, overElement) {
    var dragId = dragNode.id;

    if (overBookmarkNode.id == dragId)
      return false;

    // If we are dragging a folder, we cannot drop it on any of its descendants.
    var dragBookmarkItem = bmm.treeLookup[dragId];
    var dragBookmarkNode = dragBookmarkItem && dragBookmarkItem.bookmarkNode;
    if (dragBookmarkNode && bmm.contains(dragBookmarkNode, overBookmarkNode)) {
      return false;
    }

    return true;
  },

  /**
   * Whether we can drop the dragged items above the drop target.
   *
   * @param {!BookmarkTreeNode} overBookmarkNode The bookmark that we are
   *     currently dragging over.
   * @param {!HTMLElement} overElement The element that we are currently
   *     dragging over.
   * @return {boolean} Whether we can drop the dragged items above the drop
   *     target.
   */
  canDropAbove: function(overBookmarkNode, overElement) {
    if (overElement instanceof BookmarkList)
      return false;

    // We cannot drop between Bookmarks bar and Other bookmarks.
    if (overBookmarkNode.parentId == bmm.ROOT_ID)
      return false;

    var isOverTreeItem = overElement instanceof TreeItem;

    // We can only drop between items in the tree if we have any folders.
    if (isOverTreeItem && !this.isDraggingFolders())
      return false;

    if (!this.dragData.sameProfile)
      return this.isDraggingFolders() || !isOverTreeItem;

    return this.checkEvery_(this.canDropAbove_, overBookmarkNode, overElement);
  },

  /**
   * Helper for canDropAbove that only checks one bookmark node.
   * @private
   * @return {boolean} True if we can drop dragNode above overBookmarkNode,
   *     otherwise false.
   */
  canDropAbove_: function(dragNode, overBookmarkNode, overElement) {
    var dragId = dragNode.id;

    // We cannot drop above if the item below is already in the drag source.
    var previousElement = overElement.previousElementSibling;
    if (previousElement &&
        previousElement.bookmarkId == dragId)
      return false;

    return true;
  },

  /**
   * Whether we can drop the dragged items below the drop target.
   *
   * @param {!BookmarkTreeNode} overBookmarkNode The bookmark that we are
   *     currently dragging over.
   * @param {!HTMLElement} overElement The element that we are currently
   *     dragging over.
   * @return {boolean} Whether we can drop the dragged items below the drop
   *     target.
   */
  canDropBelow: function(overBookmarkNode, overElement) {
    if (overElement instanceof BookmarkList)
      return false;

    // We cannot drop between Bookmarks bar and Other bookmarks.
    if (overBookmarkNode.parentId == bmm.ROOT_ID)
      return false;

    // We can only drop between items in the tree if we have any folders.
    if (!this.isDraggingFolders() && overElement instanceof TreeItem)
      return false;

    var isOverTreeItem = overElement instanceof TreeItem;

    // Don't allow dropping below an expanded tree item since it is confusing
    // to the user anyway.
    if (isOverTreeItem && overElement.expanded)
      return false;

    if (!this.dragData.sameProfile)
      return this.isDraggingFolders() || !isOverTreeItem;

    return this.checkEvery_(this.canDropBelow_, overBookmarkNode, overElement);
  },

  /**
   * Helper for canDropBelow that only checks one bookmark node.
   * @private
   * @return {boolean} True if we can drop dragNode below overBookmarkNode,
   *     otherwise false.
   */
  canDropBelow_: function(dragNode, overBookmarkNode, overElement) {
    var dragId = dragNode.id;

    // We cannot drop below if the item below is already in the drag source.
    var nextElement = overElement.nextElementSibling;
    if (nextElement &&
        nextElement.bookmarkId == dragId)
      return false;

    return true;
  },

  /**
   * Whether we can drop the dragged items on the drop target.
   *
   * @param {!BookmarkTreeNode} overBookmarkNode The bookmark that we are
   *     currently dragging over.
   * @param {!HTMLElement} overElement The element that we are currently
   *     dragging over.
   * @return {boolean} Whether we can drop the dragged items on the drop
   *     target.
   */
  canDropOn: function(overBookmarkNode, overElement) {
    // We can only drop on a folder.
    if (!bmm.isFolder(overBookmarkNode))
      return false;

    if (!this.dragData.sameProfile)
      return true;

    return this.checkEvery_(this.canDropOn_, overBookmarkNode, overElement);
  },

  /**
   * Helper for canDropOn that only checks one bookmark node.
   * @private
   * @return {boolean} True if dragNode can drop on overBookmarkNode.
   */
  canDropOn_: function(dragNode, overBookmarkNode, overElement) {
    var dragId = dragNode.id;

    if (overElement instanceof BookmarkList) {
      // We are trying to drop an item after the last item in the list. This
      // is allowed if the item is different from the last item in the list.
      var listItems = list.items;
      var len = listItems.length;
      if (len == 0 ||
          listItems[len - 1].bookmarkId != dragId) {
        return true;
      }
    }

    // Cannot drop on current parent.
    if (overBookmarkNode.id == dragNode.parentId)
      return false;

    return true;
  },

  /**
   * Callback for the dragstart event.
   * @param {Event} e The dragstart event.
   */
  handleDragStart: function(e) {
    // Determine the selected bookmarks.
    var target = e.target;
    var draggedNodes = [];
    if (target instanceof ListItem) {
      // Use selected items.
      draggedNodes = target.parentNode.selectedItems;
    } else if (target instanceof TreeItem) {
      draggedNodes.push(target.bookmarkNode);
    }

    // We manage starting the drag by using the extension API.
    e.preventDefault();

    if (draggedNodes.length) {
      // If we are dragging a single link, we can do the *Link* effect.
      // Otherwise, we only allow copy and move.
      var effectAllowed;
      if (draggedNodes.length == 1 &&
          !bmm.isFolder(draggedNodes[0])) {
        effectAllowed = 'copyMoveLink';
      } else {
        effectAllowed = 'copyMove';
      }
      e.dataTransfer.effectAllowed = effectAllowed;

      var ids = draggedNodes.map(function(node) {
        return node.id;
      });

      chrome.bookmarkManagerPrivate.startDrag(ids);
    }
  },

  handleDragEnter: function(e) {
    e.preventDefault();
  },

  /**
   * Calback for the dragover event.
   * @param {Event} e The dragover event.
   */
  handleDragOver: function(e) {
    // TODO(arv): This function is way too long. Please refactor it.

    // Allow DND on text inputs.
    if (e.target.tagName != 'INPUT') {
      // The default operation is to allow dropping links etc to do navigation.
      // We never want to do that for the bookmark manager.
      e.preventDefault();

      // Set to none. This will get set to something if we can do the drop.
      e.dataTransfer.dropEffect = 'none';
    }

    if (!this.dragData)
      return;

    var overElement = this.getBookmarkElement(e.target);
    if (!overElement && e.target == list)
      overElement = list;

    if (!overElement)
      return;

    var overBookmarkNode = overElement.bookmarkNode;

    // Expands a folder in tree view when pointer hovers on it longer than
    // EXPAND_FOLDER_DELAY.
    var hoverOnFolderTimeStamp = lastHoverOnFolderTimeStamp;
    lastHoverOnFolderTimeStamp = 0;
    if (hoverOnFolderTimeStamp) {
      if (e.timeStamp - hoverOnFolderTimeStamp >= EXPAND_FOLDER_DELAY)
        overElement.expanded = true;
      else
        lastHoverOnFolderTimeStamp = hoverOnFolderTimeStamp;
    } else if (overElement instanceof TreeItem &&
               bmm.isFolder(overBookmarkNode) &&
               overElement.hasChildren &&
               !overElement.expanded) {
      lastHoverOnFolderTimeStamp = e.timeStamp;
    }

    if (!this.canDrop(overBookmarkNode, overElement))
      return;

    var bookmarkNode = overElement.bookmarkNode;

    var canDropAbove = this.canDropAbove(overBookmarkNode, overElement);
    var canDropOn = this.canDropOn(overBookmarkNode, overElement);
    var canDropBelow = this.canDropBelow(overBookmarkNode, overElement);

    if (!canDropAbove && !canDropOn && !canDropBelow)
      return;

    // Now we know that we can drop. Determine if we will drop above, on or
    // below based on mouse position etc.

    var dropPos;

    e.dataTransfer.dropEffect = this.dragData.sameProfile ? 'move' : 'copy';

    var rect;
    if (overElement instanceof TreeItem) {
      // We only want the rect of the row representing the item and not
      // its children.
      rect = overElement.rowElement.getBoundingClientRect();
    } else {
      rect = overElement.getBoundingClientRect();
    }

    var dy = e.clientY - rect.top;
    var yRatio = dy / rect.height;

    // above
    if (canDropAbove &&
        (yRatio <= .25 || yRatio <= .5 && !(canDropBelow && canDropOn))) {
      dropPos = 'above';

    // below
    } else if (canDropBelow &&
               (yRatio > .75 || yRatio > .5 && !(canDropAbove && canDropOn))) {
      dropPos = 'below';

    // on
    } else if (canDropOn) {
      dropPos = 'on';

    // none
    } else {
      // No drop can happen. Exit now.
      e.dataTransfer.dropEffect = 'none';
      return;
    }

    function cloneClientRect(rect) {
      var newRect = {};
      for (var key in rect) {
        newRect[key] = rect[key];
      }
      return newRect;
    }

    // If we are dropping above or below a tree item, adjust the width so
    // that it is clearer where the item will be dropped.
    if ((dropPos == 'above' || dropPos == 'below') &&
        overElement instanceof TreeItem) {
      // ClientRect is read only so clone into a read-write object.
      rect = cloneClientRect(rect);
      var rtl = getComputedStyle(overElement).direction == 'rtl';
      var labelElement = overElement.labelElement;
      var labelRect = labelElement.getBoundingClientRect();
      if (rtl) {
        rect.width = labelRect.left + labelRect.width - rect.left;
      } else {
        rect.left = labelRect.left;
        rect.width -= rect.left;
      }
    }

    var overlayType = dropPos;

    // If we are dropping on a list we want to show an overlay drop line after
    // the last element.
    if (overElement instanceof BookmarkList) {
      overlayType = 'below';

      // Get the rect of the last list item.
      var length = overElement.dataModel.length;
      if (length) {
        dropPos = 'below';
        overElement = overElement.getListItemByIndex(length - 1);
        rect = overElement.getBoundingClientRect();
      } else {
        // If there are no items, collapse the height of the rect.
        rect = cloneClientRect(rect);
        rect.height = 0;
        // We do not use bottom so we don't care to adjust it.
      }
    }

    this.showDropOverlay_(rect, overlayType);

    this.dropDestination = {
      dropPos: dropPos,
      relatedNode: overElement.bookmarkNode
    };
  },

  /**
   * Shows and positions the drop marker overlay.
   * @param {ClientRect} targetRect The drop target rect.
   * @param {string} overlayType The position relative to the target rect.
   * @private
   */
  showDropOverlay_: function(targetRect, overlayType) {
    window.clearTimeout(this.hideDropOverlayTimer_);
    var overlay = $('drop-overlay');
    if (overlayType == 'on') {
      overlay.className = '';
      overlay.style.top = targetRect.top + 'px';
      overlay.style.height = targetRect.height + 'px';
    } else {
      overlay.className = 'line';
      overlay.style.height = '';
    }
    overlay.style.width = targetRect.width + 'px';
    overlay.style.left = targetRect.left + 'px';
    overlay.style.display = 'block';

    if (overlayType != 'on') {
      var overlayRect = overlay.getBoundingClientRect();
      if (overlayType == 'above') {
        overlay.style.top = targetRect.top - overlayRect.height / 2 + 'px';
      } else {
        overlay.style.top = targetRect.top + targetRect.height -
            overlayRect.height / 2 + 'px';
      }
    }
  },

  /**
   * Hides the drop overlay element.
   * @private
   */
  hideDropOverlay_: function() {
    // Hide the overlay in a timeout to reduce flickering as we move between
    // valid drop targets.
    window.clearTimeout(this.hideDropOverlayTimer_);
    this.hideDropOverlayTimer_ = window.setTimeout(function() {
      $('drop-overlay').style.display = '';
    }, 100);
  },

  handleDragLeave: function(e) {
    this.hideDropOverlay_();
  },

  handleDrop: function(e) {
    if (this.dropDestination && this.dragData) {
      var dropPos = this.dropDestination.dropPos;
      var relatedNode = this.dropDestination.relatedNode;
      var parentId = dropPos == 'on' ? relatedNode.id : relatedNode.parentId;

      var selectTarget;
      var selectedTreeId;
      var index;
      var relatedIndex;
      // Try to find the index in the dataModel so we don't have to always keep
      // the index for the list items up to date.
      var overElement = this.getBookmarkElement(e.target);
      if (overElement instanceof ListItem) {
        relatedIndex = overElement.parentNode.dataModel.indexOf(relatedNode);
        selectTarget = list;
      } else if (overElement instanceof BookmarkList) {
        relatedIndex = overElement.dataModel.length - 1;
        selectTarget = list;
      } else {
        // Tree
        relatedIndex = relatedNode.index;
        selectTarget = tree;
        selectedTreeId =
            tree.selectedItem ? tree.selectedItem.bookmarkId : null;
      }

      if (dropPos == 'above')
        index = relatedIndex;
      else if (dropPos == 'below')
        index = relatedIndex + 1;

      selectItemsAfterUserAction(selectTarget, selectedTreeId);

      if (index != undefined && index != -1)
        chrome.bookmarkManagerPrivate.drop(parentId, index);
      else
        chrome.bookmarkManagerPrivate.drop(parentId);

      e.preventDefault();

      // TODO(arv): Select the newly dropped items.
    }
    this.dropDestination = null;
    this.hideDropOverlay_();
  },

  clearDragData: function() {
    this.dragData = null;
  },

  handleChromeDragEnter: function(dragData) {
    this.dragData = dragData;
  },

  init: function() {
    var boundClearData = this.clearDragData.bind(this);
    function deferredClearData() {
      setTimeout(boundClearData);
    }

    document.addEventListener('dragstart', this.handleDragStart.bind(this));
    document.addEventListener('dragenter', this.handleDragEnter.bind(this));
    document.addEventListener('dragover', this.handleDragOver.bind(this));
    document.addEventListener('dragleave', this.handleDragLeave.bind(this));
    document.addEventListener('drop', this.handleDrop.bind(this));
    document.addEventListener('dragend', deferredClearData);
    document.addEventListener('mouseup', deferredClearData);

    chrome.bookmarkManagerPrivate.onDragEnter.addListener(
        this.handleChromeDragEnter.bind(this));
    chrome.bookmarkManagerPrivate.onDragLeave.addListener(
        deferredClearData);
    chrome.bookmarkManagerPrivate.onDrop.addListener(deferredClearData);
  }
};

/**
 * Helper function that updates the canExecute and labels for the open-like
 * commands.
 * @param {!cr.ui.CanExecuteEvent} e The event fired by the command system.
 * @param {!cr.ui.Command} command The command we are currently processing.
 */
function updateOpenCommands(e, command) {
  var selectedItems = getSelectedBookmarkNodes(e.target);
  var isFolder = selectedItems.length == 1 && bmm.isFolder(selectedItems[0]);
  var multiple = selectedItems.length > 1 || isFolder;

  function hasBookmarks(node) {
    for (var i = 0; i < node.children.length; i++) {
      if (!bmm.isFolder(node.children[i]))
        return true;
    }
    return false;
  }

  var commandDisabled = false;
  switch (command.id) {
    case 'open-in-new-tab-command':
      command.label = loadTimeData.getString(multiple ?
          'open_all' : 'open_in_new_tab');
      break;

    case 'open-in-new-window-command':
      command.label = loadTimeData.getString(multiple ?
          'open_all_new_window' : 'open_in_new_window');
      // Disabled when incognito is forced.
      commandDisabled = incognitoModeAvailability == 'forced' ||
          !canOpenNewWindows;
      break;
    case 'open-incognito-window-command':
      command.label = loadTimeData.getString(multiple ?
          'open_all_incognito' : 'open_incognito');
      // Not available withn incognito is disabled.
      commandDisabled = incognitoModeAvailability == 'disabled';
      break;
  }
  e.canExecute = selectedItems.length > 0 && !commandDisabled;
  if (isFolder && e.canExecute) {
    // We need to get all the bookmark items in this tree. If the tree does not
    // contain any non-folders, we need to disable the command.
    var p = bmm.loadSubtree(selectedItems[0].id);
    p.addListener(function(node) {
      command.disabled = !node || !hasBookmarks(node);
    });
  }
}

/**
 * Calls the backend to figure out if we can paste the clipboard into the active
 * folder.
 * @param {Function=} opt_f Function to call after the state has been updated.
 */
function updatePasteCommand(opt_f) {
  function update(canPaste) {
    var organizeMenuCommand = $('paste-from-organize-menu-command');
    var contextMenuCommand = $('paste-from-context-menu-command');
    organizeMenuCommand.disabled = !canPaste;
    contextMenuCommand.disabled = !canPaste;
    if (opt_f)
      opt_f();
  }
  // We cannot paste into search and recent view.
  if (list.isSearch() || list.isRecent()) {
    update(false);
  } else {
    chrome.bookmarkManagerPrivate.canPaste(list.parentId, update);
  }
}

function handleCanExecuteForDocument(e) {
  var command = e.command;
  switch (command.id) {
    case 'import-menu-command':
      e.canExecute = canEdit;
      break;
    case 'export-menu-command':
      // We can always execute the export-menu command.
      e.canExecute = true;
      break;
    case 'sort-command':
      e.canExecute = !list.isRecent() && !list.isSearch() &&
          list.dataModel.length > 1;
      break;
    case 'undo-command':
      // The global undo command has no visible UI, so always enable it, and
      // just make it a no-op if undo is not possible.
      e.canExecute = true;
      break;
    default:
      canExecuteForList(e);
      break;
  }
}

/**
 * Helper function for handling canExecute for the list and the tree.
 * @param {!Event} e Can execute event object.
 * @param {boolean} isRecentOrSearch Whether the user is trying to do a command
 *     on recent or search.
 */
function canExecuteShared(e, isRecentOrSearch) {
  var command = e.command;
  var commandId = command.id;
  switch (commandId) {
    case 'paste-from-organize-menu-command':
    case 'paste-from-context-menu-command':
      updatePasteCommand();
      break;

    case 'add-new-bookmark-command':
    case 'new-folder-command':
      e.canExecute = !isRecentOrSearch && canEdit;
      break;

    case 'open-in-new-tab-command':
    case 'open-in-background-tab-command':
    case 'open-in-new-window-command':
    case 'open-incognito-window-command':
      updateOpenCommands(e, command);
      break;

    case 'undo-delete-command':
      e.canExecute = !!lastDeletedNodes;
      break;
  }
}

/**
 * Helper function for handling canExecute for the list and document.
 * @param {!Event} e Can execute event object.
 */
function canExecuteForList(e) {
  var command = e.command;
  var commandId = command.id;

  function hasSelected() {
    return !!list.selectedItem;
  }

  function hasSingleSelected() {
    return list.selectedItems.length == 1;
  }

  function canCopyItem(item) {
    return item.id != 'new';
  }

  function canCopyItems() {
    var selectedItems = list.selectedItems;
    return selectedItems && selectedItems.some(canCopyItem);
  }

  function isRecentOrSearch() {
    return list.isRecent() || list.isSearch();
  }

  switch (commandId) {
    case 'rename-folder-command':
      // Show rename if a single folder is selected.
      var items = list.selectedItems;
      if (items.length != 1) {
        e.canExecute = false;
        command.hidden = true;
      } else {
        var isFolder = bmm.isFolder(items[0]);
        e.canExecute = isFolder && canEdit;
        command.hidden = !isFolder;
      }
      break;

    case 'edit-command':
      // Show the edit command if not a folder.
      var items = list.selectedItems;
      if (items.length != 1) {
        e.canExecute = false;
        command.hidden = false;
      } else {
        var isFolder = bmm.isFolder(items[0]);
        e.canExecute = !isFolder && canEdit;
        command.hidden = isFolder;
      }
      break;

    case 'show-in-folder-command':
      e.canExecute = isRecentOrSearch() && hasSingleSelected();
      break;

    case 'delete-command':
    case 'cut-command':
      e.canExecute = canCopyItems() && canEdit;
      break;

    case 'copy-command':
      e.canExecute = canCopyItems();
      break;

    case 'open-in-same-window-command':
      e.canExecute = hasSelected();
      break;

    default:
      canExecuteShared(e, isRecentOrSearch());
  }
}

// Update canExecute for the commands when the list is the active element.
function handleCanExecuteForList(e) {
  if (e.target != list) return;
  canExecuteForList(e);
}

// Update canExecute for the commands when the tree is the active element.
function handleCanExecuteForTree(e) {
  if (e.target != tree) return;

  var command = e.command;
  var commandId = command.id;

  function hasSelected() {
    return !!e.target.selectedItem;
  }

  function isRecentOrSearch() {
    var item = e.target.selectedItem;
    return item == recentTreeItem || item == searchTreeItem;
  }

  function isTopLevelItem() {
    return e.target.selectedItem.parentNode == tree;
  }

  switch (commandId) {
    case 'rename-folder-command':
      command.hidden = false;
      e.canExecute = hasSelected() && !isTopLevelItem() && canEdit;
      break;

    case 'edit-command':
      command.hidden = true;
      e.canExecute = false;
      break;

    case 'delete-command':
    case 'cut-command':
      e.canExecute = hasSelected() && !isTopLevelItem() && canEdit;
      break;

    case 'copy-command':
      e.canExecute = hasSelected() && !isTopLevelItem();
      break;

    default:
      canExecuteShared(e, isRecentOrSearch());
  }
}

/**
 * Update the canExecute state of the commands when the selection changes.
 * @param {Event} e The change event object.
 */
function updateCommandsBasedOnSelection(e) {
  if (e.target == document.activeElement) {
    // Paste only needs to be updated when the tree selection changes.
    var commandNames = ['copy', 'cut', 'delete', 'rename-folder', 'edit',
      'add-new-bookmark', 'new-folder', 'open-in-new-tab',
      'open-in-new-window', 'open-incognito-window', 'open-in-same-window',
      'show-in-folder'];

    if (e.target == tree) {
      commandNames.push('paste-from-context-menu', 'paste-from-organize-menu',
                        'sort');
    }

    commandNames.forEach(function(baseId) {
      $(baseId + '-command').canExecuteChange();
    });
  }
}

function updateEditingCommands() {
  var editingCommands = ['cut', 'delete', 'rename-folder', 'edit',
      'add-new-bookmark', 'new-folder', 'sort',
      'paste-from-context-menu', 'paste-from-organize-menu'];

  chrome.bookmarkManagerPrivate.canEdit(function(result) {
    if (result != canEdit) {
      canEdit = result;
      editingCommands.forEach(function(baseId) {
        $(baseId + '-command').canExecuteChange();
      });
    }
  });
}

function handleChangeForTree(e) {
  updateCommandsBasedOnSelection(e);
  navigateTo(tree.selectedItem.bookmarkId, updateHash);
}

function handleOrganizeButtonClick(e) {
  updateEditingCommands();
  $('add-new-bookmark-command').canExecuteChange();
  $('new-folder-command').canExecuteChange();
  $('sort-command').canExecuteChange();
}

function handleRename(e) {
  var item = e.target;
  var bookmarkNode = item.bookmarkNode;
  chrome.bookmarks.update(bookmarkNode.id, {title: item.label});
  performGlobalUndo = null;  // This can't be undone, so disable global undo.
}

function handleEdit(e) {
  var item = e.target;
  var bookmarkNode = item.bookmarkNode;
  var context = {
    title: bookmarkNode.title
  };
  if (!bmm.isFolder(bookmarkNode))
    context.url = bookmarkNode.url;

  if (bookmarkNode.id == 'new') {
    selectItemsAfterUserAction(list);

    // New page
    context.parentId = bookmarkNode.parentId;
    chrome.bookmarks.create(context, function(node) {
      // A new node was created and will get added to the list due to the
      // handler.
      var dataModel = list.dataModel;
      var index = dataModel.indexOf(bookmarkNode);
      dataModel.splice(index, 1);

      // Select new item.
      var newIndex = dataModel.findIndexById(node.id);
      if (newIndex != -1) {
        var sm = list.selectionModel;
        list.scrollIndexIntoView(newIndex);
        sm.leadIndex = sm.anchorIndex = sm.selectedIndex = newIndex;
      }
    });
  } else {
    // Edit
    chrome.bookmarks.update(bookmarkNode.id, context);
  }
  performGlobalUndo = null;  // This can't be undone, so disable global undo.
}

function handleCancelEdit(e) {
  var item = e.target;
  var bookmarkNode = item.bookmarkNode;
  if (bookmarkNode.id == 'new') {
    var dataModel = list.dataModel;
    var index = dataModel.findIndexById('new');
    dataModel.splice(index, 1);
  }
}

/**
 * Navigates to the folder that the selected item is in and selects it. This is
 * used for the show-in-folder command.
 */
function showInFolder() {
  var bookmarkNode = list.selectedItem;
  if (!bookmarkNode)
    return;
  var parentId = bookmarkNode.parentId;

  // After the list is loaded we should select the revealed item.
  function selectItem() {
    var index = list.dataModel.findIndexById(bookmarkNode.id);
    if (index == -1)
      return;
    var sm = list.selectionModel;
    sm.anchorIndex = sm.leadIndex = sm.selectedIndex = index;
    list.scrollIndexIntoView(index);
  }

  var treeItem = bmm.treeLookup[parentId];
  treeItem.reveal();

  navigateTo(parentId, selectItem);
}

/**
 * @return {!cr.LinkController} The link controller used to open links based on
 *     user clicks and keyboard actions.
 */
function getLinkController() {
  return linkController ||
      (linkController = new cr.LinkController(loadTimeData));
}

/**
 * Returns the selected bookmark nodes of the provided tree or list.
 * If |opt_target| is not provided or null the active element is used.
 * Only call this if the list or the tree is focused.
 * @param {BookmarkList|BookmarkTree} opt_target The target list or tree.
 * @return {!Array} Array of bookmark nodes.
 */
function getSelectedBookmarkNodes(opt_target) {
  return (opt_target || document.activeElement) == tree ?
      tree.selectedFolders : list.selectedItems;
}

/**
 * @return {!Array.<string>} An array of the selected bookmark IDs.
 */
function getSelectedBookmarkIds() {
  return getSelectedBookmarkNodes().map(function(node) {
    return node.id;
  });
}

/**
 * Opens the selected bookmarks.
 * @param {LinkKind} kind The kind of link we want to open.
 */
function openBookmarks(kind) {
  // If we have selected any folders, we need to find all items recursively.
  // We use multiple async calls to getSubtree instead of getting the whole
  // tree since we would like to minimize the amount of data sent.

  var urls = [];

  // Adds the node and all its children.
  function addNodes(node) {
    if (node.children) {
      node.children.forEach(function(child) {
        if (!bmm.isFolder(child))
          urls.push(child.url);
      });
    } else {
      urls.push(node.url);
    }
  }

  var nodes = getSelectedBookmarkNodes();

  // Get a future promise for every selected item.
  var promises = nodes.map(function(node) {
    if (bmm.isFolder(node))
      return bmm.loadSubtree(node.id);
    // Not a folder so we already have all the data we need.
    return new Promise(node.url);
  });

  var p = Promise.all.apply(null, promises);
  p.addListener(function(values) {
    values.forEach(function(v) {
      if (typeof v == 'string')
        urls.push(v);
      else
        addNodes(v);
    });
    getLinkController().openUrls(urls, kind);
    chrome.bookmarkManagerPrivate.recordLaunch();
  });
}

/**
 * Opens an item in the list.
 */
function openItem() {
  var bookmarkNodes = getSelectedBookmarkNodes();
  // If we double clicked or pressed enter on a single folder, navigate to it.
  if (bookmarkNodes.length == 1 && bmm.isFolder(bookmarkNodes[0])) {
    navigateTo(bookmarkNodes[0].id, updateHash);
  } else {
    openBookmarks(LinkKind.FOREGROUND_TAB);
  }
}

/**
 * Deletes the selected bookmarks. The bookmarks are saved in memory in case
 * the user needs to undo the deletion.
 */
function deleteBookmarks() {
  var selectedIds = getSelectedBookmarkIds();
  lastDeletedNodes = [];

  function performDelete() {
    selectedIds.forEach(function(id) {
      chrome.bookmarks.removeTree(id);
    });
    $('undo-delete-command').canExecuteChange();
    performGlobalUndo = undoDelete;
  }

  // First, store information about the bookmarks being deleted.
  selectedIds.forEach(function(id) {
    chrome.bookmarks.getSubTree(id, function(results) {
      lastDeletedNodes.push(results);

      // When all nodes have been saved, perform the deletion.
      if (lastDeletedNodes.length === selectedIds.length)
        performDelete();
    });
  });
}

/**
 * Restores a tree of bookmarks under a specified folder.
 * @param {BookmarkTreeNode} node The node to restore.
 * @param {=string} parentId The ID of the folder to restore under. If not
 *     specified, the original parentId of the node will be used.
 */
function restoreTree(node, parentId) {
  var bookmarkInfo = {
    parentId: parentId || node.parentId,
    title: node.title,
    index: node.index,
    url: node.url
  };

  chrome.bookmarks.create(bookmarkInfo, function(result) {
    if (!result) {
      console.error('Failed to restore bookmark.');
      return;
    }

    if (node.children) {
      // Restore the children using the new ID for this node.
      node.children.forEach(function(child) {
        restoreTree(child, result.id);
      });
    }
  });
}

/**
 * Restores the last set of bookmarks that was deleted.
 */
function undoDelete() {
  lastDeletedNodes.forEach(function(arr) {
    arr.forEach(restoreTree);
  });
  lastDeletedNodes = null;
  $('undo-delete-command').canExecuteChange();

  // Only a single level of undo is supported, so disable global undo now.
  performGlobalUndo = null;
}

/**
 * Computes folder for "Add Page" and "Add Folder".
 * @return {string} The id of folder node where we'll create new page/folder.
 */
function computeParentFolderForNewItem() {
  if (document.activeElement == tree)
    return list.parentId;
  var selectedItem = list.selectedItem;
  return selectedItem && bmm.isFolder(selectedItem) ?
      selectedItem.id : list.parentId;
}

/**
 * Callback for rename folder and edit command. This starts editing for
 * selected item.
 */
function editSelectedItem() {
  if (document.activeElement == tree) {
    tree.selectedItem.editing = true;
  } else {
    var li = list.getListItem(list.selectedItem);
    if (li)
      li.editing = true;
  }
}

/**
 * Callback for the new folder command. This creates a new folder and starts
 * a rename of it.
 */
function newFolder() {
  performGlobalUndo = null;  // This can't be undone, so disable global undo.

  var parentId = computeParentFolderForNewItem();

  // Callback is called after tree and list data model updated.
  function createFolder(callback) {
    chrome.bookmarks.create({
      title: loadTimeData.getString('new_folder_name'),
      parentId: parentId
    }, callback);
  }

  if (document.activeElement == tree) {
    createFolder(function(newNode) {
      navigateTo(newNode.id, function() {
        bmm.treeLookup[newNode.id].editing = true;
      });
    });
    return;
  }

  function editNewFolderInList() {
    createFolder(function() {
      var index = list.dataModel.length - 1;
      var sm = list.selectionModel;
      sm.anchorIndex = sm.leadIndex = sm.selectedIndex = index;
      scrollIntoViewAndMakeEditable(index);
    });
  }

  navigateTo(parentId, editNewFolderInList);
}

/**
 * Scrolls the list item into view and makes it editable.
 * @param {number} index The index of the item to make editable.
 */
function scrollIntoViewAndMakeEditable(index) {
  list.scrollIndexIntoView(index);
  // onscroll is now dispatched asynchronously so we have to postpone
  // the rest.
  setTimeout(function() {
    var item = list.getListItemByIndex(index);
    if (item)
      item.editing = true;
  });
}

/**
 * Adds a page to the current folder. This is called by the
 * add-new-bookmark-command handler.
 */
function addPage() {
  var parentId = computeParentFolderForNewItem();

  function editNewBookmark() {
    var fakeNode = {
      title: '',
      url: '',
      parentId: parentId,
      id: 'new'
    };
    var dataModel = list.dataModel;
    var length = dataModel.length;
    dataModel.splice(length, 0, fakeNode);
    var sm = list.selectionModel;
    sm.anchorIndex = sm.leadIndex = sm.selectedIndex = length;
    scrollIntoViewAndMakeEditable(length);
  };

  navigateTo(parentId, editNewBookmark);
}

/**
 * This function is used to select items after a user action such as paste, drop
 * add page etc.
 * @param {BookmarkList|BookmarkTree} target The target of the user action.
 * @param {=string} opt_selectedTreeId If provided, then select that tree id.
 */
function selectItemsAfterUserAction(target, opt_selectedTreeId) {
  // We get one onCreated event per item so we delay the handling until we get
  // no more events coming.

  var ids = [];
  var timer;

  function handle(id, bookmarkNode) {
    clearTimeout(timer);
    if (opt_selectedTreeId || list.parentId == bookmarkNode.parentId)
      ids.push(id);
    timer = setTimeout(handleTimeout, 50);
  }

  function handleTimeout() {
    chrome.bookmarks.onCreated.removeListener(handle);
    chrome.bookmarks.onMoved.removeListener(handle);

    if (opt_selectedTreeId && ids.indexOf(opt_selectedTreeId) != -1) {
      var index = ids.indexOf(opt_selectedTreeId);
      if (index != -1 && opt_selectedTreeId in bmm.treeLookup) {
        tree.selectedItem = bmm.treeLookup[opt_selectedTreeId];
      }
    } else if (target == list) {
      var dataModel = list.dataModel;
      var firstIndex = dataModel.findIndexById(ids[0]);
      var lastIndex = dataModel.findIndexById(ids[ids.length - 1]);
      if (firstIndex != -1 && lastIndex != -1) {
        var selectionModel = list.selectionModel;
        selectionModel.selectedIndex = -1;
        selectionModel.selectRange(firstIndex, lastIndex);
        selectionModel.anchorIndex = selectionModel.leadIndex = lastIndex;
        list.focus();
      }
    }

    list.endBatchUpdates();
  }

  list.startBatchUpdates();

  chrome.bookmarks.onCreated.addListener(handle);
  chrome.bookmarks.onMoved.addListener(handle);
  timer = setTimeout(handleTimeout, 300);
}

/**
 * Record user action.
 * @param {string} name An user action name.
 */
function recordUserAction(name) {
  chrome.metricsPrivate.recordUserAction('BookmarkManager_Command_' + name);
}

/**
 * The currently selected bookmark, based on where the user is clicking.
 * @return {string} The ID of the currently selected bookmark (could be from
 *     tree view or list view).
 */
function getSelectedId() {
  if (document.activeElement == tree)
    return tree.selectedItem.bookmarkId;
  var selectedItem = list.selectedItem;
  return selectedItem && bmm.isFolder(selectedItem) ?
      selectedItem.id : tree.selectedItem.bookmarkId;
}

/**
 * Pastes the copied/cutted bookmark into the right location depending whether
 * if it was called from Organize Menu or from Context Menu.
 * @param {string} id The id of the element being pasted from.
 */
function pasteBookmark(id) {
  recordUserAction('Paste');
  selectItemsAfterUserAction(list);
  chrome.bookmarkManagerPrivate.paste(id, getSelectedBookmarkIds());
}

/**
 * Handler for the command event. This is used for context menu of list/tree
 * and organized menu.
 * @param {!Event} e The event object.
 */
function handleCommand(e) {
  var command = e.command;
  var commandId = command.id;
  switch (commandId) {
    case 'import-menu-command':
      recordUserAction('Import');
      chrome.bookmarks.import();
      break;
    case 'export-menu-command':
      recordUserAction('Export');
      chrome.bookmarks.export();
      break;
    case 'undo-command':
      if (performGlobalUndo) {
        recordUserAction('UndoGlobal');
        performGlobalUndo();
      } else {
        recordUserAction('UndoNone');
      }
      break;
    case 'show-in-folder-command':
      recordUserAction('ShowInFolder');
      showInFolder();
      break;
    case 'open-in-new-tab-command':
    case 'open-in-background-tab-command':
      recordUserAction('OpenInNewTab');
      openBookmarks(LinkKind.BACKGROUND_TAB);
      break;
    case 'open-in-new-window-command':
    recordUserAction('OpenInNewWindow');
      openBookmarks(LinkKind.WINDOW);
      break;
    case 'open-incognito-window-command':
      recordUserAction('OpenIncognito');
      openBookmarks(LinkKind.INCOGNITO);
      break;
    case 'delete-command':
      recordUserAction('Delete');
      deleteBookmarks();
      break;
    case 'copy-command':
      recordUserAction('Copy');
      chrome.bookmarkManagerPrivate.copy(getSelectedBookmarkIds(),
                                         updatePasteCommand);
      break;
    case 'cut-command':
      recordUserAction('Cut');
      chrome.bookmarkManagerPrivate.cut(getSelectedBookmarkIds(),
                                        updatePasteCommand);
      break;
    case 'paste-from-organize-menu-command':
      pasteBookmark(list.parentId);
      break;
    case 'paste-from-context-menu-command':
      pasteBookmark(getSelectedId());
      break;
    case 'sort-command':
      recordUserAction('Sort');
      chrome.bookmarkManagerPrivate.sortChildren(list.parentId);
      break;
    case 'rename-folder-command':
      editSelectedItem();
      break;
    case 'edit-command':
      recordUserAction('Edit');
      editSelectedItem();
      break;
    case 'new-folder-command':
      recordUserAction('NewFolder');
      newFolder();
      break;
    case 'add-new-bookmark-command':
      recordUserAction('AddPage');
      addPage();
      break;
    case 'open-in-same-window-command':
      recordUserAction('OpenInSame');
      openItem();
      break;
    case 'undo-delete-command':
      recordUserAction('UndoDelete');
      undoDelete();
      break;
  }
}

// Execute the copy, cut and paste commands when those events are dispatched by
// the browser. This allows us to rely on the browser to handle the keyboard
// shortcuts for these commands.
function installEventHandlerForCommand(eventName, commandId) {
  function handle(e) {
    var command = $(commandId);
    if (!command.disabled) {
      command.execute();
      if (e)
        e.preventDefault();  // Prevent the system beep.
    }
  }
  if (eventName == 'paste') {
    // Paste is a bit special since we need to do an async call to see if we
    // can paste because the paste command might not be up to date.
    document.addEventListener(eventName, function(e) {
      updatePasteCommand(handle);
    });
  } else {
    document.addEventListener(eventName, handle);
  }
}

function initializeSplitter() {
  var splitter = document.querySelector('.main > .splitter');
  Splitter.decorate(splitter);

  // The splitter persists the size of the left component in the local store.
  if ('treeWidth' in localStorage)
    splitter.previousElementSibling.style.width = localStorage['treeWidth'];

  splitter.addEventListener('resize', function(e) {
    localStorage['treeWidth'] = splitter.previousElementSibling.style.width;
  });
}

function initializeBookmarkManager() {
  // Sometimes the extension API is not initialized.
  if (!chrome.bookmarks)
    console.error('Bookmarks extension API is not available');

  chrome.bookmarkManagerPrivate.getStrings(loadLocalizedStrings);

  bmm.treeLookup[searchTreeItem.bookmarkId] = searchTreeItem;
  bmm.treeLookup[recentTreeItem.bookmarkId] = recentTreeItem;

  cr.ui.decorate('menu', Menu);
  cr.ui.decorate('button[menu]', MenuButton);
  cr.ui.decorate('command', Command);
  BookmarkList.decorate(list);
  BookmarkTree.decorate(tree);

  list.addEventListener('canceledit', handleCancelEdit);
  list.addEventListener('canExecute', handleCanExecuteForList);
  list.addEventListener('change', updateCommandsBasedOnSelection);
  list.addEventListener('contextmenu', updateEditingCommands);
  list.addEventListener('dblclick', handleDoubleClickForList);
  list.addEventListener('edit', handleEdit);
  list.addEventListener('rename', handleRename);
  list.addEventListener('urlClicked', handleUrlClickedForList);

  tree.addEventListener('canExecute', handleCanExecuteForTree);
  tree.addEventListener('change', handleChangeForTree);
  tree.addEventListener('contextmenu', updateEditingCommands);
  tree.addEventListener('rename', handleRename);
  tree.addEventListener('load', handleLoadForTree);

  cr.ui.contextMenuHandler.addContextMenuProperty(tree);
  list.contextMenu = $('context-menu');
  tree.contextMenu = $('context-menu');

  // We listen to hashchange so that we can update the currently shown folder
  // when // the user goes back and forward in the history.
  window.addEventListener('hashchange', processHash);

  $('term').addEventListener('search', handleSearch);

  document.querySelector('.summary > button').addEventListener(
      'click', handleOrganizeButtonClick);

  document.querySelector('button.logo').addEventListener(
      'click', handleClickOnLogoButton);

  document.addEventListener('canExecute', handleCanExecuteForDocument);
  document.addEventListener('command', handleCommand);

  // Listen to copy, cut and paste events and execute the associated commands.
  installEventHandlerForCommand('copy', 'copy-command');
  installEventHandlerForCommand('cut', 'cut-command');
  installEventHandlerForCommand('paste', 'paste-from-organize-menu-command');

  // Install shortcuts
  for (var name in commandShortcutMap) {
    $(name + '-command').shortcut = commandShortcutMap[name];
  }

  // Disable almost all commands at startup.
  var commands = document.querySelectorAll('command');
  for (var i = 0, command; command = commands[i]; ++i) {
    if (command.id != 'import-menu-command' &&
        command.id != 'export-menu-command') {
      command.disabled = true;
    }
  }

  chrome.bookmarkManagerPrivate.canEdit(function(result) {
    canEdit = result;
  });

  chrome.systemPrivate.getIncognitoModeAvailability(function(result) {
    // TODO(rustema): propagate policy value to the bookmark manager when it
    // changes.
    incognitoModeAvailability = result;
  });

  chrome.bookmarkManagerPrivate.canOpenNewWindows(function(result) {
    canOpenNewWindows = result;
  });

  initializeSplitter();
  bmm.addBookmarkModelListeners();
  dnd.init();
  tree.reload();
}

initializeBookmarkManager();
})();
