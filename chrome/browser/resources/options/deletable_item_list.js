// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;

  /**
   * Wraps a list item to make it deletable, adding a button that will trigger a
   * call to deleteItemAtIndex(index) in the list.
   * @param {!ListItem} baseItem The list element to wrap.
   */
  function DeletableListItem(baseItem) {
    var el = cr.doc.createElement('div');
    el.baseItem_ = baseItem;
    DeletableListItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a deletable list item.
   * @param {!HTMLElement} el The element to decorate.
   */
  DeletableListItem.decorate = function(el) {
    el.__proto__ = DeletableListItem.prototype;
    el.decorate();
  };

  DeletableListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * The list item being wrapped to make it deletable.
     * @type {!ListItem}
     * @private
     */
    baseItem_: null,

    /** @inheritDoc */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      this.baseItem_.classList.add('deletable-item');
      this.appendChild(this.baseItem_);

      var closeButtonEl = this.ownerDocument.createElement('button');
      closeButtonEl.className = 'close-button';
      closeButtonEl.disabled = this.baseItem_.undeletable;
      closeButtonEl.addEventListener('mousedown', this.handleMouseDownOnClose_);
      this.appendChild(closeButtonEl);
    },

    /** @inheritDoc */
    selectionChanged: function() {
      // Forward the selection state to the |baseItem_|.
      // TODO(jhawkins): This is terrible.
      this.baseItem_.selected = this.selected;
      this.baseItem_.selectionChanged();
    },

    /**
     * Returns the list item being wrapped to make it deletable.
     * @return {!ListItem} The list item being wrapped
     */
    get contentItem() {
      return this.baseItem_;
    },

    /**
     * Don't let the list have a crack at the event. We don't want clicking the
     * close button to select the list.
     * @param {Event} e The mouse down event object.
     * @private
     */
    handleMouseDownOnClose_: function(e) {
      if (!e.target.disabled)
        e.stopPropagation();
    },
  };

  var DeletableItemList = cr.ui.define('list');

  DeletableItemList.prototype = {
    __proto__: List.prototype,

    /** @inheritDoc */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.addEventListener('click', this.handleClick_);
    },

    /** @inheritDoc */
    createItem: function(value) {
      var baseItem = this.createItemContents(value);
      return new DeletableListItem(baseItem);
    },

    /**
     * Creates a new list item to use as the main row contents for |value|.
     * Subclasses should override this instead of createItem.
     * @param {*} value The value to use for the item.
     * @return {!ListItem} The newly created list item.
     */
    createItemContents: function(value) {
      List.prototype.createItem.call(this, value);
    },

    /**
     * Callback for onclick events.
     * @param {Event} e The click event object.
     * @private
     */
    handleClick_: function(e) {
      if (this.disabled)
        return;

      var target = e.target;
      if (target.className == 'close-button') {
        var listItem = this.getListItemAncestor(target);
        if (listItem)
          this.deleteItemAtIndex(this.getIndexOfListItem(listItem));
      }
    },

    /**
     * Called when an item should be deleted; subclasses are responsible for
     * implementing.
     * @param {number} index The index of the item that is being deleted.
     */
    deleteItemAtIndex: function(index) {
    },
  };

  return {
    DeletableItemList: DeletableItemList
  };
});
