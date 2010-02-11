// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  const Event = cr.Event;
  const EventTarget = cr.EventTarget;

  /**
   * Creates a new selection model that is to be used with lists. This is
   * implemented for vertical lists but changing the behavior for horizontal
   * lists or icon views is a matter of overriding {@code getItemBefore},
   * {@code getItemAfter}, {@code getItemAbove} as well as {@code getItemBelow}.
   *
   * @constructor
   * @extends {!cr.EventTarget}
   */
  function ListSelectionModel(list) {
    this.list = list;
    this.selectedItems_ = {};
  }

  ListSelectionModel.prototype = {
    __proto__: EventTarget.prototype,

    /**
     * Returns the item below (y axis) the given element.
     * @param {*} item The item to get the item below.
     * @return {*} The item below or null if not found.
     */
    getItemBelow: function(item) {
      return item.nextElementSibling;
    },

    /**
     * Returns the item above (y axis) the given element.
     * @param {*} item The item to get the item above.
     * @return {*} The item below or null if not found.
     */
    getItemAbove: function(item) {
      return item.previousElementSibling;
    },

    /**
     * Returns the item before (x axis) the given element. This returns null
     * by default but override this for icon view and horizontal selection
     * models.
     *
     * @param {*} item The item to get the item before.
     * @return {*} The item before or null if not found.
     */
    getItemBefore: function(item) {
      return null;
    },

    /**
     * Returns the item after (x axis) the given element. This returns null
     * by default but override this for icon view and horizontal selection
     * models.
     *
     * @param {*} item The item to get the item after.
     * @return {*} The item after or null if not found.
     */
    getItemAfter: function(item) {
      return null;
    },

    /**
     * Returns the next list item. This is the next logical and should not
     * depend on any kind of layout of the list.
     * @param {*} item The item to get the next item for.
     * @return {*} The next item or null if not found.
     */
    getNextItem: function(item) {
      return item.nextElementSibling;
    },

    /**
     * Returns the prevous list item. This is the previous logical and should
     * not depend on any kind of layout of the list.
     * @param {*} item The item to get the previous item for.
     * @return {*} The previous item or null if not found.
     */
    getPreviousItem: function(item) {
      return item.previousElementSibling;
    },

    /**
     * @return {*} The first item.
     */
    getFirstItem: function() {
      return this.list.firstElementChild;
    },

    /**
     * @return {*} The last item.
     */
    getLastItem: function() {
      return this.list.lastElementChild;
    },

    /**
     * Called by the view when the user does a mousedown or mouseup on the list.
     * @param {!Event} e The browser mousedown event.
     * @param {*} item The item that was under the mouse pointer, null if none.
     */
    handleMouseDownUp: function(e, item) {
      var anchorItem = this.anchorItem;

      this.beginChange_();

      if (!item && !e.ctrlKey && !e.shiftKey && !e.metaKey) {
        this.clear();
      } else {
        var isDown = e.type == 'mousedown';
        if (!cr.isMac && e.ctrlKey) {
          // Handle ctrlKey on mouseup
          if (!isDown) {
            // toggle the current one and make it anchor item
            this.setItemSelected(item, !this.getItemSelected(item));
            this.leadItem = item;
            this.anchorItem = item;
          }
        } else if (e.shiftKey && anchorItem && anchorItem != item) {
          // Shift is done in mousedown
          if (isDown) {
            this.clearAllSelected_();
            this.leadItem = item;
            this.selectRange(anchorItem, item);
          }
        } else {
          // Right click for a context menu need to not clear the selection.
          var isRightClick = e.button == 2;

          // If the item is selected this is handled in mouseup.
          var itemSelected = this.getItemSelected(item);
          if ((itemSelected && !isDown || !itemSelected && isDown) &&
              !(itemSelected && isRightClick)) {
            this.clearAllSelected_();
            this.setItemSelected(item, true);
            this.leadItem = item;
            this.anchorItem = item;
          }
        }
      }

      this.endChange_();
    },

    /**
     * Called by the view when it recieves a keydown event.
     * @param {Event} e The keydown event.
     */
    handleKeyDown: function(e) {
      var newItem = null;
      var leadItem = this.leadItem;
      var prevent = true;

      // Ctrl/Meta+A
      if (e.keyCode == 65 &&
          (cr.isMac && e.metaKey || !cr.isMac && e.ctrlKey)) {
        this.selectAll();
        e.preventDefault();
        return;
      }

      // Space
      if (e.keyCode == 32) {
        if (leadItem != null) {
          var selected = this.getItemSelected(leadItem);
          if (e.ctrlKey || !selected) {
            this.beginChange_();
            this.setItemSelected(leadItem, !selected);
            this.endChange_();
            return;
          }
        }
      }

      switch (e.keyIdentifier) {
        case 'Home':
          newItem = this.getFirstItem();
          break;
        case 'End':
          newItem = this.getLastItem();
          break;
        case 'Up':
          newItem = !leadItem  ?
              this.getLastItem() : this.getItemAbove(leadItem);
          break;
        case 'Down':
          newItem = !leadItem ?
              this.getFirstItem() : this.getItemBelow(leadItem);
          break;
        case 'Left':
          newItem = !leadItem ?
              this.getLastItem() : this.getItemBefore(leadItem);
          break;
        case 'Right':
          newItem = !leadItem ?
              this.getFirstItem() : this.getItemAfter(leadItem);
          break;
        default:
          prevent = false;
      }

      if (newItem) {
        this.beginChange_();

        this.leadItem = newItem;
        if (e.shiftKey) {
          var anchorItem = this.anchorItem;
          this.clearAllSelected_();
          if (!anchorItem) {
            this.setItemSelected(newItem, true);
            this.anchorItem = newItem;
          } else {
            this.selectRange(anchorItem, newItem);
          }
        } else if (e.ctrlKey && !cr.isMac) {
          // Setting the lead item is done above
          // Mac does not allow you to change the lead.
        } else {
          this.clearAllSelected_();
          this.setItemSelected(newItem, true);
          this.anchorItem = newItem;
        }

        this.endChange_();

        if (prevent)
          e.preventDefault();
      }
    },

    /**
     * @type {!Array} The selected items.
     */
    get selectedItems() {
      return Object.keys(this.selectedItems_).map(function(uid) {
        return this.selectedItems_[uid];
      }, this);
    },
    set selectedItems(selectedItems) {
      this.beginChange_();
      this.clearAllSelected_();
      for (var i = 0; i < selectedItems.length; i++) {
        this.setItemSelected(selectedItems[i], true);
      }
      this.leadItem = this.anchorItem = selectedItems[0] || null;
      this.endChange_();
    },

    /**
     * Convenience getter which returns the first selected item.
     * @type {*}
     */
    get selectedItem() {
      for (var uid in this.selectedItems_) {
        return this.selectedItems_[uid];
      }
      return null;
    },
    set selectedItem(selectedItem) {
      this.beginChange_();
      this.clearAllSelected_();
      if (selectedItem) {
        this.selectedItems = [selectedItem];
      } else {
        this.leadItem = this.anchorItem = null;
      }
      this.endChange_();
    },

    /**
     * Selects a range of items, starting with {@code start} and ends with
     * {@code end}.
     * @param {*} start The first item to select.
     * @param {*} end The last item to select.
     */
    selectRange: function(start, end) {
      // Swap if starts comes after end.
      if (start.compareDocumentPosition(end) & Node.DOCUMENT_POSITION_PRECEDING) {
        var tmp = start;
        start = end;
        end = tmp;
      }

      this.beginChange_();

      for (var item = start; item != end; item = this.getNextItem(item)) {
        this.setItemSelected(item, true);
      }
      this.setItemSelected(end, true);

      this.endChange_();
    },

    /**
     * Selects all items.
     */
    selectAll: function() {
      this.selectRange(this.getFirstItem(), this.getLastItem());
    },

    /**
     * Clears the selection
     */
    clear: function() {
      this.beginChange_();
      this.clearAllSelected_();
      this.endChange_();
    },

    /**
     * Clears the selection and updates the view.
     * @private
     */
    clearAllSelected_: function() {
      for (var uid in this.selectedItems_) {
        this.setItemSelected(this.selectedItems_[uid], false);
      }
    },

    /**
     * Sets the selecte state for an item.
     * @param {*} item The item to set the selected state for.
     * @param {boolean} b Whether to select the item or not.
     */
    setItemSelected: function(item, b) {
      var uid = this.list.itemToUid(item);
      var oldSelected = uid in this.selectedItems_;
      if (oldSelected == b)
        return;

      if (b)
        this.selectedItems_[uid] = item;
      else
        delete this.selectedItems_[uid];

      this.beginChange_();

      // Changing back?
      if (uid in this.changedUids_ && this.changedUids_[uid] == !b) {
        delete this.changedUids_[uid];
      } else {
        this.changedUids_[uid] = b;
      }

      // End change dispatches an event which in turn may update the view.
      this.endChange_();
    },

    /**
     * Whether a given item is selected or not.
     * @param {*} item The item to check.
     * @return {boolean} Whether an item is selected.
     */
    getItemSelected: function(item) {
      var uid = this.list.itemToUid(item);
      return uid in this.selectedItems_;
    },

    /**
     * This is used to begin batching changes. Call {@code endChange_} when you
     * are done making changes.
     * @private
     */
    beginChange_: function() {
      if (!this.changeCount_) {
        this.changeCount_ = 0;
        this.changedUids_ = {};
      }
      this.changeCount_++;
    },

    /**
     * Call this after changes are done and it will dispatch a change event if
     * any changes were actually done.
     * @private
     */
    endChange_: function() {
      this.changeCount_--;
      if (!this.changeCount_) {
        var uids = Object.keys(this.changedUids_);
        if (uids.length) {
          var e = new Event('change');
          e.changes = uids.map(function(uid) {
            return {
              uid: uid,
              selected: this.changedUids_[uid]
            };
          }, this);
          this.dispatchEvent(e);
        }
        delete this.changedUids_;
        delete this.changeCount_;
      }
    },

    /**
     * Called when an item is removed from the lisst.
     * @param {cr.ui.ListItem} item The list item that was removed.
     */
    remove: function(item) {
      if (item == this.leadItem)
        this.leadItem = this.getNextItem(item) || this.getPreviousItem(item);
      if (item == this.anchorItem)
        this.anchorItem = this.getNextItem(item) || this.getPreviousItem(item);

      // Deselect when removing items.
      if (this.getItemSelected(item))
        this.setItemSelected(item, false);
    },

    /**
     * Called when an item was added to the list.
     * @param {cr.ui.ListItem} item The list item to add.
     */
    add: function(item) {
      // We could (should?) check if the item is selected here and update the
      // selection model.
    }
  };

  /**
   * The anchorItem is used with multiple selection.
   * @type {*}
   */
  cr.defineProperty(ListSelectionModel, 'anchorItem', cr.PropertyKind.JS, null);

  /**
   * The leadItem is used with multiple selection and it is the item that the
   * user is moving uysing the arrow keys.
   * @type {*}
   */
  cr.defineProperty(ListSelectionModel, 'leadItem', cr.PropertyKind.JS, null);

  return {
    ListSelectionModel: ListSelectionModel
  };
});
