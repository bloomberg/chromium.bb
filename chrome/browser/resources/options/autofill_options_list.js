// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.autofillOptions', function() {
  const DeletableItem = options.DeletableItem;
  const DeletableItemList = options.DeletableItemList;
  const InlineEditableItem = options.InlineEditableItem;
  const InlineEditableItemList = options.InlineEditableItemList;

  function AutofillEditProfileButton(guid, edit) {
    var editButtonEl = document.createElement('button');
    editButtonEl.className = 'raw-button';
    editButtonEl.textContent =
        templateData.autofillEditProfileButton;
    editButtonEl.onclick = function(e) { edit(guid); };

    // Don't select the row when clicking the button.
    editButtonEl.onmousedown = function(e) {
      e.stopPropagation();
    };

    return editButtonEl;
  }

  /**
   * Creates a new address list item.
   * @param {Array} entry An array of the form [guid, label].
   * @constructor
   * @extends {options.DeletableItem}
   */
  function AddressListItem(entry) {
    var el = cr.doc.createElement('div');
    el.guid = entry[0];
    el.label = entry[1];
    el.__proto__ = AddressListItem.prototype;
    el.decorate();

    return el;
  }

  AddressListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      // The stored label.
      var label = this.ownerDocument.createElement('div');
      label.className = 'autofill-list-item';
      label.textContent = this.label;
      this.contentElement.appendChild(label);

      // The 'Edit' button.
      var editButtonEl = new AutofillEditProfileButton(
        this.guid,
        AutofillOptions.loadAddressEditor);
      this.contentElement.appendChild(editButtonEl);
    },
  };

  /**
   * Creates a new credit card list item.
   * @param {Array} entry An array of the form [guid, label, icon].
   * @constructor
   * @extends {options.DeletableItem}
   */
  function CreditCardListItem(entry) {
    var el = cr.doc.createElement('div');
    el.guid = entry[0];
    el.label = entry[1];
    el.icon = entry[2];
    el.description = entry[3];
    el.__proto__ = CreditCardListItem.prototype;
    el.decorate();

    return el;
  }

  CreditCardListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      // The stored label.
      var label = this.ownerDocument.createElement('div');
      label.className = 'autofill-list-item';
      label.textContent = this.label;
      this.contentElement.appendChild(label);

      // The credit card icon.
      var icon = this.ownerDocument.createElement('image');
      icon.src = this.icon;
      icon.alt = this.description;
      this.contentElement.appendChild(icon);

      // The 'Edit' button.
      var editButtonEl = new AutofillEditProfileButton(
        this.guid,
        AutofillOptions.loadCreditCardEditor);
      this.contentElement.appendChild(editButtonEl);
    },
  };

  /**
   * Creates a new value list item.
   * @param {AutofillValuesList} list The parent list of this item.
   * @param {String} entry A string value.
   * @constructor
   * @extends {options.InlineEditableItem}
   */
  function ValuesListItem(list, entry) {
    var el = cr.doc.createElement('div');
    el.list = list;
    el.value = entry;
    el.__proto__ = ValuesListItem.prototype;
    el.decorate();

    return el;
  }

  ValuesListItem.prototype = {
    __proto__: InlineEditableItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);

      this.isPlaceholder = !this.value;

      // The stored value.
      var cell = this.createEditableTextCell(this.value);
      this.contentElement.appendChild(cell);
      this.input = cell.querySelector('input');

      this.addEventListener('commitedit', this.onEditCommitted_);
    },

    /**
     * Called when committing an edit.
     * @param {Event} e The end event.
     * @private
     */
    onEditCommitted_: function(e) {
      var i = this.list.items.indexOf(this);
      if (this.input.value == this.list.dataModel.item(i))
        return;

      if (this.input.value &&
          this.list.dataModel.indexOf(this.input.value) == -1) {
        // Update with new value.
        this.list.validateAndSave(i, 1, this.input.value);
      } else {
        // Reject empty values and duplicates.
        this.list.dataModel.splice(i, 1);
      }
    },
  };

  /**
   * Creates a new list item for the Add New Item row, which doesn't represent
   * an actual entry in the values list but allows the user to add new
   * values.
   * @param {AutofillValuesList} entry The parent list of this item.
   * @constructor
   * @extends {cr.ui.ValuesListItem}
   */
  function ValuesAddRowListItem(list) {
    var el = cr.doc.createElement('div');
    el.list = list;
    el.__proto__ = ValuesAddRowListItem.prototype;
    el.decorate();

    return el;
  }

  ValuesAddRowListItem.prototype = {
    __proto__: ValuesListItem.prototype,

    decorate: function() {
      ValuesListItem.prototype.decorate.call(this);
      this.input.value = '';
      this.input.placeholder = this.list.getAttribute('placeholder');
      this.deletable = false;
    },

    /**
     * Called when committing an edit.  Committing a non-empty value adds it
     * to the end of the values list, leaving this "AddRow" in place.
     * @param {Event} e The end event.
     * @extends {options.ValuesListItem}
     * @private
     */
    onEditCommitted_: function(e) {
      var i = this.list.items.indexOf(this);
      if (i < 0 || i >= this.list.dataModel.length || !this.input.value.length)
        return;

      if (this.input.value &&
          this.list.dataModel.indexOf(this.input.value) == -1) {
        // It is important that updateIndex is done before validateAndSave.
        // Otherwise we can not be sure about AddRow index.
        this.list.dataModel.updateIndex(i);
        this.list.validateAndSave(i, 0, this.input.value);
      } else {
        this.input.value = '';
      }
    },
  };

  /**
   * Create a new address list.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var AutofillAddressList = cr.ui.define('list');

  AutofillAddressList.prototype = {
    __proto__: DeletableItemList.prototype,

    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);

      this.addEventListener('blur', this.onBlur_);
    },

    /**
     * When the list loses focus, unselect all items in the list.
     * @private
     */
    onBlur_: function() {
      this.selectionModel.unselectAll();
    },

    /** @inheritDoc */
    createItem: function(entry) {
      return new AddressListItem(entry);
    },

    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      AutofillOptions.removeAddress(this.dataModel.item(index)[0]);
    },
  };

  /**
   * Create a new credit card list.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var AutofillCreditCardList = cr.ui.define('list');

  AutofillCreditCardList.prototype = {
    __proto__: DeletableItemList.prototype,

    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);

      this.addEventListener('blur', this.onBlur_);
    },

    /**
     * When the list loses focus, unselect all items in the list.
     * @private
     */
    onBlur_: function() {
      this.selectionModel.unselectAll();
    },

    /** @inheritDoc */
    createItem: function(entry) {
      return new CreditCardListItem(entry);
    },

    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      AutofillOptions.removeCreditCard(this.dataModel.item(index)[0]);
    },
  };

  /**
   * Create a new value list.
   * @constructor
   * @extends {options.InlineEditableItemList}
   */
  var AutofillValuesList = cr.ui.define('list');

  AutofillValuesList.prototype = {
    __proto__: InlineEditableItemList.prototype,

    decorate: function() {
      InlineEditableItemList.prototype.decorate.call(this);

      var self = this;
      function handleBlur(e) {
        // When the blur event happens we do not know who is getting focus so we
        // delay this a bit until we know if the new focus node is outside the
        // list.
        var doc = e.target.ownerDocument;
        window.setTimeout(function() {
          var activeElement = doc.activeElement;
          if (!self.contains(activeElement))
            self.selectionModel.unselectAll();
        }, 50);
      }

      this.addEventListener('blur', handleBlur, true);
    },

    /** @inheritDoc */
    createItem: function(entry) {
      if (entry != null)
        return new ValuesListItem(this, entry);
      else
        return new ValuesAddRowListItem(this);
    },

    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      this.dataModel.splice(index, 1);
    },

    /** @inheritDoc */
    shouldFocusPlaceholder: function() {
      return false;
    },

    /**
     * Called when a new list item should be validated; subclasses are
     * responsible for implementing if validation is required.
     * @param {number} index The index of the item that was inserted or changed.
     * @param {number} remove The number items to remove.
     * @param {string} value The value of the item to insert.
     */
    validateAndSave: function(index, remove, value) {
      this.dataModel.splice(index, remove, value);
    },
  };

  /**
   * Create a new value list for phone number validation.
   * @constructor
   * @extends {options.AutofillValuesList}
   */
  var AutofillPhoneValuesList = cr.ui.define('list');

  AutofillPhoneValuesList.prototype = {
    __proto__: AutofillValuesList.prototype,

    decorate: function() {
      AutofillValuesList.prototype.decorate.call(this);
    },

    /** @inheritDoc */
    validateAndSave: function(index, remove, value) {
      var numbers = this.dataModel.slice(0, this.dataModel.length - 1);
      numbers.splice(index, remove, value);
      var info = new Array();
      info[0] = index;
      info[1] = numbers;
      info[2] = $('country').value;
      chrome.send('validatePhoneNumbers', info);
    },
  };

  return {
    AddressListItem: AddressListItem,
    CreditCardListItem: CreditCardListItem,
    ValuesListItem: ValuesListItem,
    ValuesAddRowListItem: ValuesAddRowListItem,
    AutofillAddressList: AutofillAddressList,
    AutofillCreditCardList: AutofillCreditCardList,
    AutofillValuesList: AutofillValuesList,
    AutofillPhoneValuesList: AutofillPhoneValuesList,
  };
});
