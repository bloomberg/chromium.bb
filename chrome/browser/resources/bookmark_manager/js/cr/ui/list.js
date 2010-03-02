// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: listselectionmodel.js

/**
 * @fileoverview This implements a list control.
 */

cr.define('cr.ui', function() {
  const ListSelectionModel = cr.ui.ListSelectionModel;

  /**
   * Creates a new list element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLUListElement}
   */
  var List = cr.ui.define('list');

  List.prototype = {
    __proto__: HTMLUListElement.prototype,

    /**
     * The selection model to use.
     * @type {cr.ui.ListSelectionModel}
     */
    get selectionModel() {
      return this.selectionModel_;
    },
    set selectionModel(sm) {
      var oldSm = this.selectionModel_;
      if (oldSm == sm)
        return;

      if (!this.boundHandleOnChange_) {
        this.boundHandleOnChange_ = cr.bind(this.handleOnChange_, this);
        this.boundHandleLeadChange_ = cr.bind(this.handleLeadChange_, this);
      }

      if (oldSm) {
        oldSm.removeEventListener('change', this.boundHandleOnChange_);
        oldSm.removeEventListener('leadItemChange', this.boundHandleLeadChange_);
      }

      this.selectionModel_ = sm;

      if (sm) {
        sm.addEventListener('change', this.boundHandleOnChange_);
        sm.addEventListener('leadItemChange', this.boundHandleLeadChange_);
      }
    },

    /**
     * Convenience alias for selectionModel.selectedItem
     * @type {cr.ui.ListItem}
     */
    get selectedItem() {
      return this.selectionModel.selectedItem;
    },
    set selectedItem(selectedItem) {
      this.selectionModel.selectedItem = selectedItem;
    },

    /**
     * Convenience alias for selectionModel.selectedItems
     * @type {!Array<cr.ui.ListItem>}
     */
    get selectedItems() {
      return this.selectionModel.selectedItems;
    },

    /**
     * The HTML elements representing the items. This is just all the element
     * children but subclasses may override this to filter out certain elements.
     * @type {HTMLCollection}
     */
    get items() {
      return this.children;
    },

    add: function(listItem) {
      this.appendChild(listItem);

      var uid = cr.getUid(listItem);
      this.uidToListItem_[uid] = listItem;

      this.selectionModel.add(listItem);
    },

    addAt: function(listItem, index) {
      this.insertBefore(listItem, this.items[index]);

      var uid = cr.getUid(listItem);
      this.uidToListItem_[uid] = listItem;

      this.selectionModel.add(listItem);
    },

    remove: function(listItem) {
      this.selectionModel.remove(listItem);

      this.removeChild(listItem);

      var uid = cr.getUid(listItem);
      delete this.uidToListItem_[uid];
    },

    clear: function() {
      this.innerHTML = '';
      this.selectionModel.clear();
    },

    /**
     * Initializes the element.
     */
    decorate: function() {
      this.uidToListItem_ = {};

      this.selectionModel = new ListSelectionModel(this);

      this.addEventListener('mousedown', this.handleMouseDownUp_);
      this.addEventListener('mouseup', this.handleMouseDownUp_);
      this.addEventListener('keydown', this.handleKeyDown);
      this.addEventListener('dblclick', this.handleDoubleClick_);

      // Make list focusable
      if (!this.hasAttribute('tabindex'))
        this.tabIndex = 0;
    },

    /**
     * Callback for mousedown and mouseup events.
     * @param {Event} e The mouse event object.
     * @private
     */
    handleMouseDownUp_: function(e) {
      var target = e.target;
      while (target && target.parentNode != this) {
        target = target.parentNode;
      }
      this.selectionModel.handleMouseDownUp(e, target);
    },

    /**
     * Callback for mousedown events.
     * @param {Event} e The mouse event object.
     * @private
     */
    handleMouseUp_: function(e) {
      var target = e.target;
      while (target && target.parentNode != this) {
        target = target.parentNode;
      }
      if (target) {
        this.selectionModel.handleMouseDown(e, target);
      } else {
        this.selectionModel.clear();
      }
    },


    /**
     * Handle a keydown event.
     * @param {Event} e The keydown event.
     * @return {boolean} Whether the key event was handled.
     */
    handleKeyDown: function(e) {
      if (this.selectionModel.handleKeyDown(e))
        return true;
      if (e.keyIdentifier == 'Enter' && this.selectionModel.selectedItem) {
        cr.dispatchSimpleEvent(this, 'activate');
        return true;
      }
      return false;
    },

    /**
     * Handler for double clicking. When the user double clicks on a selected
     * item we dispatch an {@code activate} event.
     * @param {Event} e The mouse event object.
     * @private
     */
    handleDoubleClick_: function(e) {
      if (e.button == 0 && this.selectionModel.selectedItem) {
        cr.dispatchSimpleEvent(this, 'activate');
      }
    },

    /**
     * Callback from the selection model. We dispatch {@code change} events
     * when the selection changes.
     * @param {!cr.Event} e Event with change info.
     * @private
     */
    handleOnChange_: function(ce) {
      ce.changes.forEach(function(change) {
        var listItem = this.uidToListItem_[change.uid];
        listItem.selected = change.selected;
      }, this);

      cr.dispatchSimpleEvent(this, 'change');
    },

    /**
     * Handles a change of the lead item from the selection model.
     * @property {Event} pe The property change event.
     * @private
     */
    handleLeadChange_: function(pe) {
      if (pe.oldValue) {
        pe.oldValue.lead = false;
      }
      if (pe.newValue) {
        pe.newValue.lead = true;
      }
    },

    /**
     * Gets a unique ID for an item. This needs to be unique to the list but
     * does not have to be gloabally unique. This uses {@code cr.getUid} by
     * default. Override to provide a more efficient way to get the unique ID.
     * @param {cr.ui.ListItem} item The item to get the unique ID for.
     * @return
     */
    itemToUid: function(item) {
      return cr.getUid(item);
    }
  };

  return {
    List: List
  }
});
