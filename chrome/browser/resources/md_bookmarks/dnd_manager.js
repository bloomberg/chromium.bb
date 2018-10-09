// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/** @typedef {?{elements: !Array<BookmarkNode>, sameProfile: boolean}} */
let NormalizedDragData;

cr.define('bookmarks', function() {
  /** @const {number} */
  const DRAG_THRESHOLD = 15;

  /**
   * @param {BookmarkElement} element
   * @return {boolean}
   */
  function isBookmarkItem(element) {
    return element.tagName == 'BOOKMARKS-ITEM';
  }

  /**
   * @param {BookmarkElement} element
   * @return {boolean}
   */
  function isBookmarkFolderNode(element) {
    return element.tagName == 'BOOKMARKS-FOLDER-NODE';
  }

  /**
   * @param {BookmarkElement} element
   * @return {boolean}
   */
  function isBookmarkList(element) {
    return element.tagName == 'BOOKMARKS-LIST';
  }

  /**
   * @param {BookmarkElement} element
   * @return {boolean}
   */
  function isClosedBookmarkFolderNode(element) {
    return isBookmarkFolderNode(element) &&
        !(/** @type {BookmarksFolderNodeElement} */ (element).isOpen);
  }

  /**
   * @param {Array<!Element>|undefined} path
   * @return {BookmarkElement}
   */
  function getBookmarkElement(path) {
    if (!path)
      return null;

    for (let i = 0; i < path.length; i++) {
      if (isBookmarkItem(path[i]) || isBookmarkFolderNode(path[i]) ||
          isBookmarkList(path[i])) {
        return path[i];
      }
    }
    return null;
  }

  /**
   * @param {Array<!Element>|undefined} path
   * @return {BookmarkElement}
   */
  function getDragElement(path) {
    const dragElement = getBookmarkElement(path);
    for (let i = 0; i < path.length; i++) {
      if (path[i].tagName == 'BUTTON')
        return null;
    }
    return dragElement && dragElement.getAttribute('draggable') ? dragElement :
                                                                  null;
  }

  /**
   * @param {BookmarkElement} bookmarkElement
   * @return {BookmarkNode}
   */
  function getBookmarkNode(bookmarkElement) {
    return bookmarks.Store.getInstance().data.nodes[bookmarkElement.itemId];
  }

  /**
   * Contains and provides utility methods for drag data sent by the
   * bookmarkManagerPrivate API.
   * @constructor
   */
  function DragInfo() {
    /** @type {NormalizedDragData} */
    this.dragData = null;
  }

  DragInfo.prototype = {
    /** @param {DragData} newDragData */
    setNativeDragData: function(newDragData) {
      this.dragData = {
        sameProfile: newDragData.sameProfile,
        elements:
            newDragData.elements.map((x) => bookmarks.util.normalizeNode(x))
      };
    },

    clearDragData: function() {
      this.dragData = null;
    },

    /** @return {boolean} */
    isDragValid: function() {
      return !!this.dragData;
    },

    /** @return {boolean} */
    isSameProfile: function() {
      return !!this.dragData && this.dragData.sameProfile;
    },

    /** @return {boolean} */
    isDraggingFolders: function() {
      return !!this.dragData && this.dragData.elements.some(function(node) {
        return !node.url;
      });
    },

    /** @return {boolean} */
    isDraggingBookmark: function(bookmarkId) {
      return !!this.dragData && this.isSameProfile() &&
          this.dragData.elements.some(function(node) {
            return node.id == bookmarkId;
          });
    },

    /** @return {boolean} */
    isDraggingChildBookmark: function(folderId) {
      return !!this.dragData && this.isSameProfile() &&
          this.dragData.elements.some(function(node) {
            return node.parentId == folderId;
          });
    },

    /** @return {boolean} */
    isDraggingFolderToDescendant: function(itemId, nodes) {
      if (!this.isSameProfile())
        return false;

      let parentId = nodes[itemId].parentId;
      const parents = {};
      while (parentId) {
        parents[parentId] = true;
        parentId = nodes[parentId].parentId;
      }

      return !!this.dragData && this.dragData.elements.some(function(node) {
        return parents[node.id];
      });
    },
  };


  /**
   * Manages auto expanding of sidebar folders on hover while dragging.
   * @constructor
   */
  function AutoExpander() {
    /** @const {number} */
    this.EXPAND_FOLDER_DELAY = 400;

    /** @private {?BookmarkElement} */
    this.lastElement_ = null;

    /** @type {!bookmarks.Debouncer} */
    this.debouncer_ = new bookmarks.Debouncer(() => {
      const store = bookmarks.Store.getInstance();
      store.dispatch(
          bookmarks.actions.changeFolderOpen(this.lastElement_.itemId, true));
      this.reset();
    });
  }

  AutoExpander.prototype = {
    /**
     * @param {Event} e
     * @param {?BookmarkElement} overElement
     */
    update: function(e, overElement) {
      const itemId = overElement ? overElement.itemId : null;
      const store = bookmarks.Store.getInstance();

      // If dragging over a new closed folder node with children reset the
      // expander. Falls through to reset the expander delay.
      if (overElement && overElement != this.lastElement_ &&
          isClosedBookmarkFolderNode(overElement) &&
          bookmarks.util.hasChildFolders(itemId, store.data.nodes)) {
        this.reset();
        this.lastElement_ = overElement;
      }

      // If dragging over the same node, reset the expander delay.
      if (overElement && overElement == this.lastElement_) {
        this.debouncer_.restartTimeout(this.EXPAND_FOLDER_DELAY);
        return;
      }

      // Otherwise, cancel the expander.
      this.reset();
    },

    reset: function() {
      this.debouncer_.reset();
      this.lastElement_ = null;
    },
  };

  /**
   * Encapsulates the behavior of the drag and drop indicator which puts a line
   * between items or highlights folders which are valid drop targets.
   * @constructor
   */
  function DropIndicator() {
    /**
     * @private {number|null} Timer id used to help minimize flicker.
     */
    this.removeDropIndicatorTimeoutId_ = null;

    /**
     * The element that had a style applied it to indicate the drop location.
     * This is used to easily remove the style when necessary.
     * @private {BookmarkElement|null}
     */
    this.lastIndicatorElement_ = null;

    /**
     * The style that was applied to indicate the drop location.
     * @private {?string|null}
     */
    this.lastIndicatorClassName_ = null;

    /**
     * Used to instantly remove the indicator style in tests.
     * @private {!Object}
     */
    this.timerProxy = window;
  }

  DropIndicator.prototype = {
    /**
     * Applies the drop indicator style on the target element and stores that
     * information to easily remove the style in the future.
     * @param {HTMLElement} indicatorElement
     * @param {DropPosition} position
     */
    addDropIndicatorStyle: function(indicatorElement, position) {
      const indicatorStyleName = position == DropPosition.ABOVE ?
          'drag-above' :
          position == DropPosition.BELOW ? 'drag-below' : 'drag-on';

      this.lastIndicatorElement_ = indicatorElement;
      this.lastIndicatorClassName_ = indicatorStyleName;

      indicatorElement.classList.add(indicatorStyleName);
    },

    /**
     * Clears the drop indicator style from the last drop target.
     */
    removeDropIndicatorStyle: function() {
      if (!this.lastIndicatorElement_ || !this.lastIndicatorClassName_)
        return;

      this.lastIndicatorElement_.classList.remove(this.lastIndicatorClassName_);
      this.lastIndicatorElement_ = null;
      this.lastIndicatorClassName_ = null;
    },

    /**
     * Displays the drop indicator on the current drop target to give the
     * user feedback on where the drop will occur.
     * @param {DropDestination} dropDest
     */
    update: function(dropDest) {
      this.timerProxy.clearTimeout(this.removeDropIndicatorTimeoutId_);
      this.removeDropIndicatorTimeoutId_ = null;

      const indicatorElement = dropDest.element.getDropTarget();
      const position = dropDest.position;

      this.removeDropIndicatorStyle();
      this.addDropIndicatorStyle(indicatorElement, position);
    },

    /**
     * Stop displaying the drop indicator.
     */
    finish: function() {
      if (this.removeDropIndicatorTimeoutId_)
        return;

      // The use of a timeout is in order to reduce flickering as we move
      // between valid drop targets.
      this.removeDropIndicatorTimeoutId_ = this.timerProxy.setTimeout(() => {
        this.removeDropIndicatorStyle();
      }, 100);
    },
  };

  /**
   * Manages drag and drop events for the bookmarks-app.
   *
   * @constructor
   */
  function DNDManager() {
    /** @private {bookmarks.DragInfo} */
    this.dragInfo_ = null;

    /** @private {?DropDestination} */
    this.dropDestination_ = null;

    /** @private {bookmarks.DropIndicator} */
    this.dropIndicator_ = null;

    /** @private {Object<string, function(!Event)>} */
    this.documentListeners_ = null;

    /** @private {?bookmarks.AutoExpander} */
    this.autoExpander_ = null;

    /**
     * Used to instantly clearDragData in tests.
     * @private {!Object}
     */
    this.timerProxy_ = window;
  }

  DNDManager.prototype = {
    init: function() {
      this.dragInfo_ = new DragInfo();
      this.dropIndicator_ = new DropIndicator();
      this.autoExpander_ = new AutoExpander();

      this.documentListeners_ = {
        'dragstart': this.onDragStart_.bind(this),
        'dragenter': this.onDragEnter_.bind(this),
        'dragover': this.onDragOver_.bind(this),
        'dragleave': this.onDragLeave_.bind(this),
        'drop': this.onDrop_.bind(this),
        'dragend': this.clearDragData_.bind(this),
        // TODO(calamity): Add touch support.
      };
      for (const event in this.documentListeners_)
        document.addEventListener(event, this.documentListeners_[event]);

      chrome.bookmarkManagerPrivate.onDragEnter.addListener(
          this.handleChromeDragEnter_.bind(this));
      chrome.bookmarkManagerPrivate.onDragLeave.addListener(
          this.clearDragData_.bind(this));
    },

    destroy: function() {
      if (this.chip_ && this.chip_.parentElement)
        document.body.removeChild(this.chip_);

      for (const event in this.documentListeners_)
        document.removeEventListener(event, this.documentListeners_[event]);
    },

    ////////////////////////////////////////////////////////////////////////////
    // DragEvent handlers:

    /**
     * @private
     * @param {Event} e
     */
    onDragStart_: function(e) {
      const dragElement = getDragElement(e.path);
      if (!dragElement)
        return;

      e.preventDefault();

      const dragData = this.calculateDragData_(dragElement);
      if (!dragData) {
        this.clearDragData_();
        return;
      }

      const state = bookmarks.Store.getInstance().data;
      const draggedNodes = dragData.elements.map((item) => item.id);
      const dragNodeIndex = draggedNodes.indexOf(dragElement.itemId);
      assert(dragNodeIndex != -1);

      // TODO(calamity): account for touch.
      chrome.bookmarkManagerPrivate.startDrag(
          draggedNodes, dragNodeIndex, false);
    },

    /** @private */
    onDragLeave_: function() {
      this.dropIndicator_.finish();
    },

    /**
     * @private
     * @param {!Event} e
     */
    onDrop_: function(e) {
      e.preventDefault();

      if (this.dropDestination_) {
        const dropInfo = this.calculateDropInfo_(this.dropDestination_);
        const index = dropInfo.index != -1 ? dropInfo.index : undefined;
        const shouldHighlight = this.shouldHighlight_(this.dropDestination_);

        if (shouldHighlight)
          bookmarks.ApiListener.trackUpdatedItems();

        chrome.bookmarkManagerPrivate.drop(
            dropInfo.parentId, index,
            shouldHighlight ? bookmarks.ApiListener.highlightUpdatedItems :
                              undefined);
      }
      this.clearDragData_();
    },

    /**
     * @private
     * @param {Event} e
     */
    onDragEnter_: function(e) {
      e.preventDefault();
    },

    /**
     * @private
     * @param {Event} e
     */
    onDragOver_: function(e) {
      // The default operation is to allow dropping links etc to do
      // navigation. We never want to do that for the bookmark manager.
      e.preventDefault();

      this.dropDestination_ = null;

      // Allow normal DND on text inputs.
      if (e.path[0].tagName == 'INPUT')
        return;

      if (!this.dragInfo_.isDragValid())
        return;

      const state = bookmarks.Store.getInstance().data;
      const items = this.dragInfo_.dragData.elements;

      const overElement = getBookmarkElement(e.path);
      this.autoExpander_.update(e, overElement);
      if (!overElement) {
        this.dropIndicator_.finish();
        return;
      }

      // Now we know that we can drop. Determine if we will drop above, on or
      // below based on mouse position etc.
      this.dropDestination_ =
          this.calculateDropDestination_(e.clientY, overElement);
      if (!this.dropDestination_) {
        this.dropIndicator_.finish();
        return;
      }

      this.dropIndicator_.update(this.dropDestination_);
    },

    /**
     * @private
     * @param {DragData} dragData
     */
    handleChromeDragEnter_: function(dragData) {
      this.dragInfo_.setNativeDragData(dragData);
    },

    ////////////////////////////////////////////////////////////////////////////
    // Helper methods:

    /** @private */
    clearDragData_: function() {
      this.autoExpander_.reset();

      // Defer the clearing of the data so that the bookmark manager API's drop
      // event doesn't clear the drop data before the web drop event has a
      // chance to execute (on Mac).
      this.timerProxy_.setTimeout(() => {
        this.dragInfo_.clearDragData();
        this.dropDestination_ = null;
        this.dropIndicator_.finish();
      }, 0);
    },

    /**
     * @param {DropDestination} dropDestination
     * @return {{parentId: string, index: number}}
     */
    calculateDropInfo_: function(dropDestination) {
      if (isBookmarkList(dropDestination.element)) {
        return {
          index: 0,
          parentId: bookmarks.Store.getInstance().data.selectedFolder,
        };
      }

      const node = getBookmarkNode(dropDestination.element);
      const position = dropDestination.position;
      let index = -1;
      let parentId = node.id;

      if (position != DropPosition.ON) {
        const state = bookmarks.Store.getInstance().data;

        // Drops between items in the normal list and the sidebar use the drop
        // destination node's parent.
        parentId = assert(node.parentId);
        index = state.nodes[parentId].children.indexOf(node.id);

        if (position == DropPosition.BELOW)
          index++;
      }

      return {
        index: index,
        parentId: parentId,
      };
    },

    /**
     * Calculates which items should be dragged based on the initial drag item
     * and the current selection. Dragged items will end up selected.
     * @param {!BookmarkElement} dragElement
     * @private
     */
    calculateDragData_: function(dragElement) {
      const dragId = dragElement.itemId;
      const store = bookmarks.Store.getInstance();
      const state = store.data;

      // Determine the selected bookmarks.
      let draggedNodes = Array.from(state.selection.items);

      // Change selection to the dragged node if the node is not part of the
      // existing selection.
      if (isBookmarkFolderNode(dragElement) ||
          draggedNodes.indexOf(dragId) == -1) {
        store.dispatch(bookmarks.actions.deselectItems());
        if (!isBookmarkFolderNode(dragElement)) {
          store.dispatch(bookmarks.actions.selectItem(dragId, state, {
            clear: false,
            range: false,
            toggle: false,
          }));
        }
        draggedNodes = [dragId];
      }

      // If any node can't be dragged, end the drag.
      const anyUnmodifiable = draggedNodes.some(
          (itemId) => !bookmarks.util.canEditNode(state, itemId));

      if (anyUnmodifiable)
        return null;

      return {
        elements: draggedNodes.map((id) => state.nodes[id]),
        sameProfile: true,
      };
    },

    /**
     * This function determines where the drop will occur.
     * @private
     * @param {number} elementClientY
     * @param {!BookmarkElement} overElement
     * @return {?DropDestination} If no valid drop position is found, null,
     *   otherwise:
     *       element - The target element that will receive the drop.
     *       position - A |DropPosition| relative to the |element|.
     */
    calculateDropDestination_: function(elementClientY, overElement) {
      const validDropPositions = this.calculateValidDropPositions_(overElement);
      if (validDropPositions == DropPosition.NONE)
        return null;

      const above = validDropPositions & DropPosition.ABOVE;
      const below = validDropPositions & DropPosition.BELOW;
      const on = validDropPositions & DropPosition.ON;
      const rect = overElement.getDropTarget().getBoundingClientRect();
      const yRatio = (elementClientY - rect.top) / rect.height;

      if (above && (yRatio <= .25 || yRatio <= .5 && (!below || !on)))
        return {element: overElement, position: DropPosition.ABOVE};

      if (below && (yRatio > .75 || yRatio > .5 && (!above || !on)))
        return {element: overElement, position: DropPosition.BELOW};

      if (on)
        return {element: overElement, position: DropPosition.ON};

      return null;
    },

    /**
     * Determines the valid drop positions for the given target element.
     * @private
     * @param {!BookmarkElement} overElement The element that we are currently
     *     dragging over.
     * @return {number} An bit field enumeration of valid drop locations.
     */
    calculateValidDropPositions_: function(overElement) {
      const dragInfo = this.dragInfo_;
      const state = bookmarks.Store.getInstance().data;
      let itemId = overElement.itemId;

      // Drags aren't allowed onto the search result list.
      if ((isBookmarkList(overElement) || isBookmarkItem(overElement)) &&
          bookmarks.util.isShowingSearch(state)) {
        return DropPosition.NONE;
      }

      if (isBookmarkList(overElement))
        itemId = state.selectedFolder;

      if (!bookmarks.util.canReorderChildren(state, itemId))
        return DropPosition.NONE;

      // Drags of a bookmark onto itself or of a folder into its children aren't
      // allowed.
      if (dragInfo.isDraggingBookmark(itemId) ||
          dragInfo.isDraggingFolderToDescendant(itemId, state.nodes)) {
        return DropPosition.NONE;
      }

      let validDropPositions = this.calculateDropAboveBelow_(overElement);
      if (this.canDropOn_(overElement))
        validDropPositions |= DropPosition.ON;

      return validDropPositions;
    },

    /**
     * @private
     * @param {BookmarkElement} overElement
     * @return {number}
     */
    calculateDropAboveBelow_: function(overElement) {
      const dragInfo = this.dragInfo_;
      const state = bookmarks.Store.getInstance().data;

      if (isBookmarkList(overElement))
        return DropPosition.NONE;

      // We cannot drop between Bookmarks bar and Other bookmarks.
      if (getBookmarkNode(overElement).parentId == ROOT_NODE_ID)
        return DropPosition.NONE;

      const isOverFolderNode = isBookmarkFolderNode(overElement);

      // We can only drop between items in the tree if we have any folders.
      if (isOverFolderNode && !dragInfo.isDraggingFolders())
        return DropPosition.NONE;

      let validDropPositions = DropPosition.NONE;

      // Cannot drop above if the item above is already in the drag source.
      const previousElem = overElement.previousElementSibling;
      if (!previousElem || !dragInfo.isDraggingBookmark(previousElem.itemId))
        validDropPositions |= DropPosition.ABOVE;

      // Don't allow dropping below an expanded sidebar folder item since it is
      // confusing to the user anyway.
      if (isOverFolderNode && !isClosedBookmarkFolderNode(overElement) &&
          bookmarks.util.hasChildFolders(overElement.itemId, state.nodes)) {
        return validDropPositions;
      }

      const nextElement = overElement.nextElementSibling;

      // Cannot drop below if the item below is already in the drag source.
      if (!nextElement || !dragInfo.isDraggingBookmark(nextElement.itemId))
        validDropPositions |= DropPosition.BELOW;

      return validDropPositions;
    },

    /**
     * Determine whether we can drop the dragged items on the drop target.
     * @private
     * @param {!BookmarkElement} overElement The element that we are currently
     *     dragging over.
     * @return {boolean} Whether we can drop the dragged items on the drop
     *     target.
     */
    canDropOn_: function(overElement) {
      // Allow dragging onto empty bookmark lists.
      if (isBookmarkList(overElement)) {
        const state = bookmarks.Store.getInstance().data;
        return state.selectedFolder &&
            state.nodes[state.selectedFolder].children.length == 0;
      }

      // We can only drop on a folder.
      if (getBookmarkNode(overElement).url)
        return false;

      return !this.dragInfo_.isDraggingChildBookmark(overElement.itemId);
    },

    /**
     * @param {DropDestination} dropDestination
     * @private
     */
    shouldHighlight_: function(dropDestination) {
      return isBookmarkItem(dropDestination.element) ||
          isBookmarkList(dropDestination.element);
    },

    /** @param {!Object} timerProxy */
    setTimerProxyForTesting: function(timerProxy) {
      this.timerProxy_ = timerProxy;
      this.dropIndicator_.timerProxy = timerProxy;
    },
  };

  return {
    AutoExpander: AutoExpander,
    DNDManager: DNDManager,
    DragInfo: DragInfo,
    DropIndicator: DropIndicator,
  };
});
