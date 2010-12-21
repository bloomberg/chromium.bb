// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;

  /**
   * Creates a deletable list item, which has a button that will trigger a call
   * to deleteItemAtIndex(index) in the list.
   */
  function DeletableItem(value) {
    var el = cr.doc.createElement('div');
    DeletableItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a deletable list item.
   * @param {!HTMLElement} el The element to decorate.
   */
  DeletableItem.decorate = function(el) {
    el.__proto__ = DeletableItem.prototype;
    el.decorate();
  };

  DeletableItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * The element subclasses should populate with content.
     * @type {HTMLElement}
     * @private
     */
    contentElement_: null,

    /**
     * The close button element.
     * @type {HTMLElement}
     * @private
     */
    closeButtonElement_: null,

    /**
     * Whether or not this item can be deleted.
     * @type {boolean}
     * @private
     */
    deletable_: true,

    /** @inheritDoc */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      this.classList.add('deletable-item');

      this.contentElement_ = this.ownerDocument.createElement('div');
      this.appendChild(this.contentElement_);

      this.closeButtonElement_ = this.ownerDocument.createElement('button');
      this.closeButtonElement_.className = 'close-button';
      this.closeButtonElement_.addEventListener('mousedown',
                                                this.handleMouseDownOnClose_);
      this.appendChild(this.closeButtonElement_);
    },

    /**
     * Returns the element subclasses should add content to.
     * @return {HTMLElement} The element subclasses should popuplate.
     */
    get contentElement() {
      return this.contentElement_;
    },

    /* Gets/sets the deletable property. An item that is not deletable doesn't
     * show the delete button (although space is still reserved for it).
     */
    get deletable() {
      return this.deletable_;
    },
    set deletable(value) {
      this.deletable_ = value;
      this.closeButtonElement_.disabled = !value;
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
    DeletableItemList: DeletableItemList,
    DeletableItem: DeletableItem,
  };
});
