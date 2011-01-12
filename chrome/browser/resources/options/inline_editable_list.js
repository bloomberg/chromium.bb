// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const DeletableItem = options.DeletableItem;
  const DeletableItemList = options.DeletableItemList;

  /**
   * Creates a new list item with support for inline editing.
   * @constructor
   * @extends {options.DeletableListItem}
   */
  function InlineEditableItem() {
    var el = cr.doc.createElement('div');
    InlineEditableItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a inline-editable list item. Note that this is
   * a subclass of DeletableItem.
   * @param {!HTMLElement} el The element to decorate.
   */
  InlineEditableItem.decorate = function(el) {
    el.__proto__ = InlineEditableItem.prototype;
    el.decorate();
  };

  InlineEditableItem.prototype = {
    __proto__: DeletableItem.prototype,

    /**
     * Whether or not this item can be edited.
     * @type {boolean}
     * @private
     */
    editable_: true,

    /**
     * Whether or not the current edit should be considered cancelled, rather
     * than committed, when editing ends.
     * @type {boolean}
     * @private
     */
    editCancelled_: true,

    /** @inheritDoc */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      this.addEventListener('keydown', this.handleKeyDown_.bind(this));
    },

    /** @inheritDoc */
    selectionChanged: function() {
      if (this.editable)
        this.editing = this.selected;
    },

    /**
     * Whether the user is currently editing the list item.
     * @type {boolean}
     */
    get editing() {
      return this.hasAttribute('editing');
    },
    set editing(editing) {
      if (this.editing == editing)
        return;

      if (editing)
        this.setAttribute('editing', '');
      else
        this.removeAttribute('editing');

      if (editing) {
        this.editCancelled_ = false;

        cr.dispatchSimpleEvent(this, 'edit', true);

        var focusElement = this.initialFocusElement;
        // When this is called in response to the selectedChange event,
        // the list grabs focus immediately afterwards. Thus we must delay
        // our focus grab.
        if (focusElement) {
          window.setTimeout(function() {
            focusElement.focus();
            focusElement.select();
          }, 50);
        }
      } else {
        if (!this.editCancelled_ && this.hasBeenEdited() &&
            this.currentInputIsValid) {
          cr.dispatchSimpleEvent(this, 'commitedit', true);
        } else {
          cr.dispatchSimpleEvent(this, 'canceledit', true);
        }
      }
    },

    /**
     * Whether the item is editable.
     * @type {boolean}
     */
    get editable() {
      return this.editable_;
    },
    set editable(editable) {
      this.editable_ = editable;
      if (!editable)
        this.editing = false;
    },

    /**
     * The HTML element that should have focus initially when editing starts.
     * Should be overriden by subclasses.
     * @type {HTMLElement}
     */
    get initialFocusElement() {
      return null;
    },

    /**
     * Whether the input in currently valid to submit. If this returns false
     * when editing would be submitted, either editing will not be ended,
     * or it will be cancelled, depending on the context.
     * Can be overrided by subclasses to perform input validation.
     */
    get currentInputIsValid() {
      return true;
    },

    /**
     * Returns true if the item has been changed by an edit.
     * Can be overrided by subclasses to return false when nothing has changed
     * to avoid unnecessary commits.
     */
    hasBeenEdited: function() {
      return true;
    },

    /**
     * Called a key is pressed. Handles committing and cancelling edits.
     * @param {Event} e The key down event.
     * @private
     */
    handleKeyDown_: function(e) {
      if (!this.editing)
        return;

      var endEdit = false;
      switch (e.keyIdentifier) {
        case 'U+001B':  // Esc
          this.editCancelled_ = true;
          endEdit = true;
          break;
        case 'Enter':
          if (this.currentInputIsValid)
            endEdit = true;
          break;
      }

      if (endEdit) {
        // Blurring will trigger the edit to end; see InlineEditableItemList.
        this.ownerDocument.activeElement.blur();
        // Make sure that handled keys aren't passed on and double-handled.
        // (e.g., esc shouldn't both cancel an edit and close a subpage)
        e.stopPropagation();
      }
    },
  };

  var InlineEditableItemList = cr.ui.define('list');

  InlineEditableItemList.prototype = {
    __proto__: DeletableItemList.prototype,

    /** @inheritDoc */
    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);
      this.addEventListener('blur', this.handleBlur_.bind(this), true);
    },

    /**
     * Called when an element in the list is blurred. Removes selection (thus
     * ending edit) if focus moves outside the list.
     * @param {Event} e The blur event.
     * @private
     */
    handleBlur_: function(e) {
      // When the blur event happens we do not know who is getting focus so we
      // delay this a bit until we know if the new focus node is outside the
      // list.
      var list = this;
      var doc = e.target.ownerDocument;
      window.setTimeout(function() {
        var activeElement = doc.activeElement;
        if (!list.contains(activeElement))
          list.selectionModel.unselectAll();
      }, 50);
    },
  };

  // Export
  return {
    InlineEditableItem: InlineEditableItem,
    InlineEditableItemList: InlineEditableItemList,
  };
});
