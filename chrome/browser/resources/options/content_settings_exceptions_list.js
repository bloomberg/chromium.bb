// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.contentSettings', function() {
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;
  const ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Creates a new exceptions list item.
   * @param {Array} exception A pair of the form [filter, setting].
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function ExceptionsListItem(exception) {
    var el = cr.doc.createElement('li');
    el.exceptionsPattern = exception[0];
    el.__proto__ = ExceptionsListItem.prototype;
    el.decorate();

    if (exception[1] == 'allow')
      el.option_allow.selected = 'selected';
    else if (exception[1] == 'block')
      el.option_block.selected = 'selected';

    return el;
  }

  ExceptionsListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * Called when an element is decorated as a list item.
     */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      var input = cr.doc.createElement('input');
      input.type = 'text';
      input.value = this.exceptionsPattern;
      this.appendChild(input);

      var select = cr.doc.createElement('select');
      var option_allow = cr.doc.createElement('option');
      option_allow.textContent = templateData.allowException;
      var option_block = cr.doc.createElement('option');
      option_block.textContent = templateData.blockException;

      select.appendChild(option_allow);
      select.appendChild(option_block);
      this.appendChild(select);

      this.input = input;
      this.select = select;
      this.option_allow = option_allow;
      this.option_block = option_block
    }
  };

  /**
   * Creates a new exceptions list.
   * @constructor
   * @extends {cr.ui.List}
   */
  var ExceptionsList = cr.ui.define('list');

  ExceptionsList.prototype = {
    __proto__: List.prototype,

    /**
     * Called when an element is decorated as a list.
     */
    decorate: function() {
      List.prototype.decorate.call(this);

      this.dataModel = new ArrayDataModel([]);
    },

    /**
     * Creates an item to go in the list.
     * @param {Object} entry The element from the data model for this row.
     */
    createItem: function(entry) {
      return new ExceptionsListItem(entry);
    },

    /**
     * Adds an exception to the model.
     * @param {Array} entry A pair of the form [filter, setting].
     */
    addException: function(entry) {
      this.dataModel.push(entry);
    }
  };

  return {
    ExceptionsListItem: ExceptionsListItem,
    ExceptionsList: ExceptionsList
  };
});
