// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enumeration of valid drop locations relative to an element. These are
 * bit masks to allow combining multiple locations in a single value.
 * @enum {number}
 * @const
 */
var DropPosition = {
  NONE: 0,
  ABOVE: 1,
  ON: 2,
  BELOW: 4,
};

/** @typedef {{element: BookmarkElement, position: DropPosition}} */
var DropDestination;

cr.define('bookmarks', function() {
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
   * @param {Array<!Element>|undefined} path
   * @return {BookmarkElement}
   */
  function getBookmarkElement(path) {
    if (!path)
      return null;

    for (var i = 0; i < path.length; i++) {
      if (isBookmarkItem(path[i]) || isBookmarkFolderNode(path[i]))
        return path[i];
    }
    return null;
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
    /** @type {DragData} */
    this.dragData = null;
  }

  DragInfo.prototype = {
    /** @param {DragData} newDragData */
    handleChromeDragEnter: function(newDragData) {
      this.dragData = newDragData;
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

      var parentId = nodes[itemId].parentId;
      var parents = {};
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
   * Encapsulates the behavior of the drag and drop indicator which puts a line
   * between items or highlights folders which are valid drop targets.
   * @constructor
   */
  function DropIndicator() {
    /**
     * @private {number|null} Timer id used to help minimize flicker.
     */
    this.removeDropIndicatorTimer_ = null;

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
     * @private {function((Function|null|string), number)}
     */
    this.setTimeout_ = window.setTimeout.bind(window);
  }

  DropIndicator.prototype = {
    /**
     * Applies the drop indicator style on the target element and stores that
     * information to easily remove the style in the future.
     * @param {HTMLElement} indicatorElement
     * @param {DropPosition} position
     */
    addDropIndicatorStyle: function(indicatorElement, position) {
      var indicatorStyleName = position == DropPosition.ABOVE ?
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
      window.clearTimeout(this.removeDropIndicatorTimer_);

      var indicatorElement = dropDest.element.getDropTarget();
      var position = dropDest.position;

      this.removeDropIndicatorStyle();
      this.addDropIndicatorStyle(indicatorElement, position);
    },

    /**
     * Stop displaying the drop indicator.
     */
    finish: function() {
      // The use of a timeout is in order to reduce flickering as we move
      // between valid drop targets.
      window.clearTimeout(this.removeDropIndicatorTimer_);
      this.removeDropIndicatorTimer_ = this.setTimeout_(function() {
        this.removeDropIndicatorStyle();
      }.bind(this), 100);
    },

    disableTimeoutForTesting: function() {
      this.setTimeout_ = function(fn, timeout) {
        fn();
      };
    },
  };

  /**
   * Manages drag and drop events for the bookmarks-app.
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
  }

  DNDManager.prototype = {
    init: function() {
      this.dragInfo_ = new DragInfo();
      this.dropIndicator_ = new DropIndicator();

      this.documentListeners_ = {
        'dragstart': this.onDragStart_.bind(this),
        'dragenter': this.onDragEnter_.bind(this),
        'dragover': this.onDragOver_.bind(this),
        'dragleave': this.onDragLeave_.bind(this),
        'drop': this.onDrop_.bind(this),
        'dragend': this.clearDragData_.bind(this),
        'mouseup': this.clearDragData_.bind(this),
        // TODO(calamity): Add touch support.
      };
      for (var event in this.documentListeners_)
        document.addEventListener(event, this.documentListeners_[event]);

      chrome.bookmarkManagerPrivate.onDragEnter.addListener(
          this.dragInfo_.handleChromeDragEnter.bind(this.dragInfo_));
      chrome.bookmarkManagerPrivate.onDragLeave.addListener(
          this.clearDragData_.bind(this));
      chrome.bookmarkManagerPrivate.onDrop.addListener(
          this.clearDragData_.bind(this));
    },

    destroy: function() {
      for (var event in this.documentListeners_)
        document.removeEventListener(event, this.documentListeners_[event]);
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
      if (this.dropDestination_)
        e.preventDefault();

      this.dropDestination_ = null;
      this.dropIndicator_.finish();
    },

    /** @private */
    clearDragData_: function() {
      this.dragInfo_.clearDragData();
      this.dropDestination_ = null;
      this.dropIndicator_.finish();
    },

    /**
     * @private
     * @param {Event} e
     */
    onDragStart_: function(e) {
      var dragElement = getBookmarkElement(e.path);
      if (!dragElement)
        return;

      // Determine the selected bookmarks.
      var state = bookmarks.Store.getInstance().data;
      var draggedNodes = Object.keys(state.selection.items);

      if (isBookmarkFolderNode(dragElement) ||
          draggedNodes.indexOf(dragElement.itemId) == -1) {
        // TODO(calamity): clear current selection.
        draggedNodes = [dragElement.itemId];
      }

      e.preventDefault();

      // If we are dragging a single link, we can do the *Link* effect.
      // Otherwise, we only allow copy and move.
      if (e.dataTransfer) {
        e.dataTransfer.effectAllowed =
            draggedNodes.length == 1 && state.nodes[draggedNodes[0]].url ?
            'copyLink' :
            'copyMove';
      }

      // TODO(calamity): account for touch.
      chrome.bookmarkManagerPrivate.startDrag(draggedNodes, false);
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
      this.dropDestination_ = null;

      // This is necessary to actually trigger the 'none' effect, even though
      // the event will have this set to 'none' already.
      if (e.dataTransfer)
        e.dataTransfer.dropEffect = 'none';

      // Allow normal DND on text inputs.
      if (e.path[0].tagName == 'INPUT')
        return;

      // The default operation is to allow dropping links etc to do
      // navigation. We never want to do that for the bookmark manager.
      e.preventDefault();

      if (!this.dragInfo_.isDragValid())
        return;

      var overElement = getBookmarkElement(e.path);
      if (!overElement)
        return;

      // TODO(calamity): open folders on hover.

      // Now we know that we can drop. Determine if we will drop above, on or
      // below based on mouse position etc.
      this.dropDestination_ =
          this.calculateDropDestination_(e.clientY, overElement);
      if (!this.dropDestination_)
        return;

      if (e.dataTransfer) {
        e.dataTransfer.dropEffect =
            this.dragInfo_.isSameProfile() ? 'move' : 'copy';
      }

      this.dropIndicator_.update(this.dropDestination_);
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
      var validDropPositions = this.calculateValidDropPositions_(overElement);
      if (validDropPositions == DropPosition.NONE)
        return null;

      var above = validDropPositions & DropPosition.ABOVE;
      var below = validDropPositions & DropPosition.BELOW;
      var on = validDropPositions & DropPosition.ON;
      var rect = overElement.getDropTarget().getBoundingClientRect();
      var yRatio = (elementClientY - rect.top) / rect.height;

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
     * @return {DropPosition} An bit field enumeration of valid drop locations.
     */
    calculateValidDropPositions_: function(overElement) {
      var dragInfo = this.dragInfo_;
      var state = bookmarks.Store.getInstance().data;

      // Drags aren't allowed onto the search result list.
      if (isBookmarkItem(overElement) &&
          bookmarks.util.isShowingSearch(state)) {
        return DropPosition.NONE;
      }

      // Drags of a bookmark onto itself or of a folder into its children aren't
      // allowed.
      if (dragInfo.isDraggingBookmark(overElement.itemId) ||
          dragInfo.isDraggingFolderToDescendant(
              overElement.itemId, state.nodes)) {
        return DropPosition.NONE;
      }

      var validDropPositions = this.calculateDropAboveBelow_(overElement);
      if (this.canDropOn_(overElement))
        validDropPositions |= DropPosition.ON;

      return validDropPositions;
    },

    /**
     * @private
     * @param {BookmarkElement} overElement
     * @return {DropPosition}
     */
    calculateDropAboveBelow_: function(overElement) {
      var dragInfo = this.dragInfo_;
      var state = bookmarks.Store.getInstance().data;

      // We cannot drop between Bookmarks bar and Other bookmarks.
      if (getBookmarkNode(overElement).parentId == bookmarks.util.ROOT_NODE_ID)
        return DropPosition.NONE;

      var isOverFolderNode = isBookmarkFolderNode(overElement);

      // We can only drop between items in the tree if we have any folders.
      if (isOverFolderNode && !dragInfo.isDraggingFolders())
        return DropPosition.NONE;

      var validDropPositions = DropPosition.NONE;

      // Cannot drop above if the item above is already in the drag source.
      var previousElem = overElement.previousElementSibling;
      if (!previousElem || !dragInfo.isDraggingBookmark(previousElem.itemId))
        validDropPositions |= DropPosition.ABOVE;

      // Don't allow dropping below an expanded sidebar folder item since it is
      // confusing to the user anyway.
      if (isOverFolderNode && !state.closedFolders[overElement.itemId] &&
          bookmarks.util.hasChildFolders(overElement.itemId, state.nodes)) {
        return validDropPositions;
      }

      var nextElement = overElement.nextElementSibling;

      // The template element sits past the end of the last bookmark element.
      if (!isBookmarkItem(nextElement) && !isBookmarkFolderNode(nextElement))
        nextElement = null;

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
      // We can only drop on a folder.
      if (getBookmarkNode(overElement).url)
        return false;

      return !this.dragInfo_.isDraggingChildBookmark(overElement.itemId)
    },
  };

  return {
    DNDManager: DNDManager,
    DragInfo: DragInfo,
    DropIndicator: DropIndicator,
  };
});
