// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.internet', function() {
  /** @const */ var InlineEditableItem = options.InlineEditableItem;
  /** @const */ var InlineEditableItemList = options.InlineEditableItemList;

  /**
   * Creates a new ip config list item.
   * @param {Object} fieldInfo The ip config field this item represents.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function IPConfigListItem(fieldInfo) {
    var el = cr.doc.createElement('div');
    el.fieldInfo_ = fieldInfo;
    IPConfigListItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a ip config list item.
   * @param {!HTMLElement} el The element to decorate.
   */
  IPConfigListItem.decorate = function(el) {
    el.__proto__ = IPConfigListItem.prototype;
    el.decorate();
  };

  IPConfigListItem.prototype = {
    __proto__: InlineEditableItem.prototype,

    /**
     * Input field for editing the ip config values.
     * @type {HTMLElement}
     * @private
     */
    valueField_: null,

    /** @inheritDoc */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);
      this.deletable = false;

      var fieldInfo = this.fieldInfo_;

      var nameEl = this.ownerDocument.createElement('div');
      nameEl.className = 'name';
      nameEl.textContent = fieldInfo['name'];

      this.contentElement.appendChild(nameEl);

      var valueEl = this.createEditableTextCell(fieldInfo['value']);
      valueEl.className = 'value';
      this.contentElement.appendChild(valueEl);

      var valueField = valueEl.querySelector('input');
      valueField.required = true;
      this.valueField_ = valueField;

      this.addEventListener('commitedit', this.onEditCommitted_);
    },

    /** @inheritDoc */
    get currentInputIsValid() {
      return this.valueField_.validity.valid;
    },

    /** @inheritDoc */
    get hasBeenEdited() {
      return this.valueField_.value != this.fieldInfo_['value'];
    },

    /**
     * Called when committing an edit; updates the model.
     * @param {Event} e The end event.
     * @private
     */
    onEditCommitted_: function(e) {
      this.fieldInfo_['value'] = this.valueField_.value;
    },
  };

  var IPConfigList = cr.ui.define('list');

  IPConfigList.prototype = {
    __proto__: InlineEditableItemList.prototype,

    /** @inheritDoc */
    createItem: function(fieldInfo) {
      return new IPConfigListItem(fieldInfo);
    },
  };

  /**
   * Whether the IP configuration list is disabled. Only used for display
   * purpose.
   * @type {boolean}
   */
  cr.defineProperty(IPConfigList, 'disabled', cr.PropertyKind.BOOL_ATTR);

  return {
    IPConfigList: IPConfigList
  };
});
