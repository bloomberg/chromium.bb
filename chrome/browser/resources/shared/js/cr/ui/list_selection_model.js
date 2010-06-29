// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(arv): Refactor parts of this into a SelectionController.

cr.define('cr.ui', function() {
  const Event = cr.Event;
  const EventTarget = cr.EventTarget;

  /**
   * Creates a new selection model that is to be used with lists. This is
   * implemented for vertical lists but changing the behavior for horizontal
   * lists or icon views is a matter of overriding {@code getIndexBefore},
   * {@code getIndexAfter}, {@code getIndexAbove} as well as
   * {@code getIndexBelow}.
   *
   * @param {number=} opt_length The number items in the selection.
   *
   * @constructor
   * @extends {!cr.EventTarget}
   */
  function ListSelectionModel(opt_length) {
    this.length_ = opt_length || 0;
    // Even though selectedIndexes_ is really a map we use an array here to get
    // iteration in the order of the indexes.
    this.selectedIndexes_ = [];
  }

  ListSelectionModel.prototype = {
    __proto__: EventTarget.prototype,

    /**
     * The number of items in the model.
     * @type {number}
     */
    get length() {
      return this.length_;
    },

    /**
     * Returns the index below (y axis) the given element.
     * @param {*} index The index to get the index below.
     * @return {*} The index below or -1 if not found.
     */
    getIndexBelow: function(index) {
      if (index == this.getLastIndex())
        return -1;
      return index + 1;
    },

    /**
     * Returns the index above (y axis) the given element.
     * @param {*} index The index to get the index above.
     * @return {*} The index below or -1 if not found.
     */
    getIndexAbove: function(index) {
      return index - 1;
    },

    /**
     * Returns the index before (x axis) the given element. This returns -1
     * by default but override this for icon view and horizontal selection
     * models.
     *
     * @param {*} index The index to get the index before.
     * @return {*} The index before or -1 if not found.
     */
    getIndexBefore: function(index) {
      return -1;
    },

    /**
     * Returns the index after (x axis) the given element. This returns -1
     * by default but override this for icon view and horizontal selection
     * models.
     *
     * @param {*} index The index to get the index after.
     * @return {*} The index after or -1 if not found.
     */
    getIndexAfter: function(index) {
      return -1;
    },

    /**
     * Returns the next list index. This is the next logical and should not
     * depend on any kind of layout of the list.
     * @param {*} index The index to get the next index for.
     * @return {*} The next index or -1 if not found.
     */
    getNextIndex: function(index) {
      if (index == this.getLastIndex())
        return -1;
      return index + 1;
    },

    /**
     * Returns the prevous list index. This is the previous logical and should
     * not depend on any kind of layout of the list.
     * @param {*} index The index to get the previous index for.
     * @return {*} The previous index or -1 if not found.
     */
    getPreviousIndex: function(index) {
      return index - 1;
    },

    /**
     * @return {*} The first index.
     */
    getFirstIndex: function() {
      return 0;
    },

    /**
     * @return {*} The last index.
     */
    getLastIndex: function() {
      return this.length_ - 1;
    },

    /**
     * Called by the view when the user does a mousedown or mouseup on the list.
     * @param {!Event} e The browser mousedown event.
     * @param {*} index The index that was under the mouse pointer, -1 if none.
     */
    handleMouseDownUp: function(e, index) {
      var anchorIndex = this.anchorIndex;
      var isDown = e.type == 'mousedown';

      this.beginChange_();

      if (index == -1) {
        // On Mac we always clear the selection if the user clicks a blank area.
        // On Windows, we only clear the selection if neither Shift nor Ctrl are
        // pressed.
        if (cr.isMac) {
          this.clear_();
        } else if (!isDown && !e.shiftKey && !e.ctrlKey)
          // Keep anchor and lead indexes. Note that this is intentionally
          // different than on the Mac.
          this.clearAllSelected_();
      } else {
        if (cr.isMac ? e.metaKey : e.ctrlKey) {
          // Selection is handled at mouseUp on windows/linux, mouseDown on mac.
          if (cr.isMac? isDown : !isDown) {
            // toggle the current one and make it anchor index
            this.setIndexSelected(index, !this.getIndexSelected(index));
            this.leadIndex = index;
            this.anchorIndex = index;
          }
        } else if (e.shiftKey && anchorIndex != -1 && anchorIndex != index) {
          // Shift is done in mousedown
          if (isDown) {
            this.clearAllSelected_();
            this.leadIndex = index;
            this.selectRange(anchorIndex, index);
          }
        } else {
          // Right click for a context menu need to not clear the selection.
          var isRightClick = e.button == 2;

          // If the index is selected this is handled in mouseup.
          var indexSelected = this.getIndexSelected(index);
          if ((indexSelected && !isDown || !indexSelected && isDown) &&
              !(indexSelected && isRightClick)) {
            this.clearAllSelected_();
            this.setIndexSelected(index, true);
            this.leadIndex = index;
            this.anchorIndex = index;
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
      var newIndex = -1;
      var leadIndex = this.leadIndex;
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
        if (leadIndex != -1) {
          var selected = this.getIndexSelected(leadIndex);
          if (e.ctrlKey || !selected) {
            this.beginChange_();
            this.setIndexSelected(leadIndex, !selected);
            this.endChange_();
            return;
          }
        }
      }

      switch (e.keyIdentifier) {
        case 'Home':
          newIndex = this.getFirstIndex();
          break;
        case 'End':
          newIndex = this.getLastIndex();
          break;
        case 'Up':
          newIndex = leadIndex == -1 ?
              this.getLastIndex() : this.getIndexAbove(leadIndex);
          break;
        case 'Down':
          newIndex = leadIndex == -1 ?
              this.getFirstIndex() : this.getIndexBelow(leadIndex);
          break;
        case 'Left':
          newIndex = leadIndex == -1 ?
              this.getLastIndex() : this.getIndexBefore(leadIndex);
          break;
        case 'Right':
          newIndex = leadIndex == -1 ?
              this.getFirstIndex() : this.getIndexAfter(leadIndex);
          break;
        default:
          prevent = false;
      }

      if (newIndex != -1) {
        this.beginChange_();

        this.leadIndex = newIndex;
        if (e.shiftKey) {
          var anchorIndex = this.anchorIndex;
          this.clearAllSelected_();
          if (anchorIndex == -1) {
            this.setIndexSelected(newIndex, true);
            this.anchorIndex = newIndex;
          } else {
            this.selectRange(anchorIndex, newIndex);
          }
        } else if (e.ctrlKey && !cr.isMac) {
          // Setting the lead index is done above
          // Mac does not allow you to change the lead.
        } else {
          this.clearAllSelected_();
          this.setIndexSelected(newIndex, true);
          this.anchorIndex = newIndex;
        }

        this.endChange_();

        if (prevent)
          e.preventDefault();
      }
    },

    /**
     * @type {!Array} The selected indexes.
     */
    get selectedIndexes() {
      return Object.keys(this.selectedIndexes_).map(Number);
    },
    set selectedIndexes(selectedIndexes) {
      this.beginChange_();
      this.clearAllSelected_();
      for (var i = 0; i < selectedIndexes.length; i++) {
        this.setIndexSelected(selectedIndexes[i], true);
      }
      if (selectedIndexes.length) {
        this.leadIndex = this.anchorIndex = selectedIndexes[0];
      } else {
        this.leadIndex = this.anchorIndex = -1;
      }
      this.endChange_();
    },

    /**
     * Convenience getter which returns the first selected index.
     * @type {*}
     */
    get selectedIndex() {
      for (var i in this.selectedIndexes_) {
        return Number(i);
      }
      return -1;
    },
    set selectedIndex(selectedIndex) {
      this.beginChange_();
      this.clearAllSelected_();
      if (selectedIndex != -1) {
        this.selectedIndexes = [selectedIndex];
      } else {
        this.leadIndex = this.anchorIndex = -1;
      }
      this.endChange_();
    },

    /**
     * Selects a range of indexes, starting with {@code start} and ends with
     * {@code end}.
     * @param {*} start The first index to select.
     * @param {*} end The last index to select.
     */
    selectRange: function(start, end) {
      // Swap if starts comes after end.
      if (start > end) {
        var tmp = start;
        start = end;
        end = tmp;
      }

      this.beginChange_();

      for (var index = start; index != end; index++) {
        this.setIndexSelected(index, true);
      }
      this.setIndexSelected(end, true);

      this.endChange_();
    },

    /**
     * Selects all indexes.
     */
    selectAll: function() {
      this.selectRange(this.getFirstIndex(), this.getLastIndex());
    },

    /**
     * Clears the selection
     */
    clear: function() {
      this.beginChange_();
      this.length_ = 0;
      this.clear_();
      this.endChange_();
    },

    /**
     * Clears all selected as well as the lead and anchor index.
     * @private
     */
    clear_: function() {
      this.anchorIndex = this.leadIndex = -1;
      this.clearAllSelected_();
    },

    /**
     * Clears the selection and updates the view.
     * @private
     */
    clearAllSelected_: function() {
      for (var i in this.selectedIndexes_) {
        this.setIndexSelected(i, false);
      }
    },

    /**
     * Sets the selected state for an index.
     * @param {*} index The index to set the selected state for.
     * @param {boolean} b Whether to select the index or not.
     */
    setIndexSelected: function(index, b) {
      var oldSelected = index in this.selectedIndexes_;
      if (oldSelected == b)
        return;

      if (b)
        this.selectedIndexes_[index] = true;
      else
        delete this.selectedIndexes_[index];

      this.beginChange_();

      // Changing back?
      if (index in this.changedIndexes_ && this.changedIndexes_[index] == !b) {
        delete this.changedIndexes_[index];
      } else {
        this.changedIndexes_[index] = b;
      }

      // End change dispatches an event which in turn may update the view.
      this.endChange_();
    },

    /**
     * Whether a given index is selected or not.
     * @param {*} index The index to check.
     * @return {boolean} Whether an index is selected.
     */
    getIndexSelected: function(index) {
      return index in this.selectedIndexes_;
    },

    /**
     * This is used to begin batching changes. Call {@code endChange_} when you
     * are done making changes.
     * @private
     */
    beginChange_: function() {
      if (!this.changeCount_) {
        this.changeCount_ = 0;
        this.changedIndexes_ = {};
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
        var indexes = Object.keys(this.changedIndexes_);
        if (indexes.length) {
          var e = new Event('change');
          e.changes = indexes.map(function(index) {
            return {
              index: index,
              selected: this.changedIndexes_[index]
            };
          }, this);
          this.dispatchEvent(e);
        }
        delete this.changedIndexes_;
        delete this.changeCount_;
      }
    },

    leadIndex_: -1,

    /**
     * The leadIndex is used with multiple selection and it is the index that
     * the user is moving using the arrow keys.
     * @type {*}
     */
    get leadIndex() {
      return this.leadIndex_;
    },
    set leadIndex(leadIndex) {
      var li = Math.max(-1, Math.min(this.length_ - 1, leadIndex));
      if (li != this.leadIndex_) {
        var oldLeadIndex = this.leadIndex_;
        this.leadIndex_ = li;
        cr.dispatchPropertyChange(this, 'leadIndex', li, oldLeadIndex);
      }
    },

    anchorIndex_: -1,

    /**
     * The anchorIndex is used with multiple selection.
     * @type {*}
     */
    get anchorIndex() {
      return this.anchorIndex_;
    },
    set anchorIndex(anchorIndex) {
      var ai = Math.max(-1, Math.min(this.length_ - 1, anchorIndex));
      if (ai != this.anchorIndex_) {
        var oldAnchorIndex = this.anchorIndex_;
        this.anchorIndex_ = ai;
        cr.dispatchPropertyChange(this, 'anchorIndex', ai, oldAnchorIndex);
      }
    },

    /**
     * Adjust the selection by adding or removing a certain numbers of items.
     * This should be called by the owner of the selection model as items are
     * added and removed from the underlying data model.
     * @param {number} index The index of the first change.
     * @param {number} itemsRemoved Number of items removed.
     * @param {number} itemsAdded Number of items added.
     */
    adjust: function(index, itemsRemoved, itemsAdded) {
      function getNewAdjustedIndex(i) {
        if (i > index && i < index + itemsRemoved) {
          return index
        } else if (i >= index) {
          return i + itemsAdded - itemsRemoved;
        }
        return i;
      }

      this.length_ += itemsAdded - itemsRemoved;

      var newMap = [];
      for (var i in this.selectedIndexes_) {
        if (i < index) {
          newMap[i] = true;
        } else if (i < index + itemsRemoved) {
          // noop
        } else {
          newMap[Number(i) + itemsAdded - itemsRemoved] = true;
        }
      }
      this.selectedIndexes_ = newMap;

      this.leadIndex = getNewAdjustedIndex(this.leadIndex);
      this.anchorIndex = getNewAdjustedIndex(this.anchorIndex);
    }
  };

  return {
    ListSelectionModel: ListSelectionModel
  };
});
