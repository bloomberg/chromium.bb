// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.autoFillOptions', function() {
  const DeletableItem = options.DeletableItem;
  const DeletableItemList = options.DeletableItemList;

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
      this.contentElement.appendChild(icon);
    },
  };

  /**
   * Create a new address list.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var AutoFillAddressList = cr.ui.define('list');

  AutoFillAddressList.prototype = {
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
    activateItemAtIndex: function(index) {
      AutoFillOptions.loadAddressEditor(this.dataModel.item(index)[0]);
    },

    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      AutoFillOptions.removeAddress(this.dataModel.item(index)[0]);
    },
  };

  /**
   * Create a new credit card list.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var AutoFillCreditCardList = cr.ui.define('list');

  AutoFillCreditCardList.prototype = {
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
    activateItemAtIndex: function(index) {
      AutoFillOptions.loadCreditCardEditor(this.dataModel.item(index)[0]);
    },

    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      AutoFillOptions.removeCreditCard(this.dataModel.item(index)[0]);
    },
  };

  return {
    AddressListItem: AddressListItem,
    CreditCardListItem: CreditCardListItem,
    AutoFillAddressList: AutoFillAddressList,
    AutoFillCreditCardList: AutoFillCreditCardList,
  };
});
