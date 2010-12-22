// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.autoFillOptions', function() {
  const DeletableItemList = options.DeletableItemList;
  const DeletableItem = options.DeletableItem;
  const List = cr.ui.List;

  /**
   * Creates a new AutoFill list item.
   * @param {Array} entry An array of the form [guid, label].
   * @constructor
   * @extends {options.DeletableItem}
   */
  function AutoFillListItem(entry) {
    var el = cr.doc.createElement('div');
    el.guid = entry[0];
    el.label = entry[1];
    el.__proto__ = AutoFillListItem.prototype;
    el.decorate();

    return el;
  }

  AutoFillListItem.prototype = {
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
   * Create a new AutoFill list.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var AutoFillList = cr.ui.define('list');

  AutoFillList.prototype = {
    __proto__: DeletableItemList.prototype,

    /** @inheritDoc */
    createItem: function(entry) {
      return new AutoFillListItem(entry);
    },

    /** @inheritDoc */
    activateItemAtIndex: function(index) {
      AutoFillOptions.loadProfileEditor(this.dataModel.item(index)[0]);
    },

    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      AutoFillOptions.removeAutoFillProfile(this.dataModel.item(index)[0]);
    },
  };

  return {
    AutoFillListItem: AutoFillListItem,
    AutoFillList: AutoFillList,
  };
});
