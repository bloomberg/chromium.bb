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
    el.dataItem = exception;
    el.__proto__ = ExceptionsListItem.prototype;
    el.decorate();

    return el;
  }

  ExceptionsListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * Called when an element is decorated as a list item.
     */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      // Labels for display mode.
      var patternLabel = cr.doc.createElement('span');
      patternLabel.textContent = this.pattern;
      this.appendChild(patternLabel);

      var settingLabel = cr.doc.createElement('span');
      settingLabel.textContent = this.settingForDisplay();
      settingLabel.className = 'exceptionSetting';
      this.appendChild(settingLabel);

      // Elements for edit mode.
      var input = cr.doc.createElement('input');
      input.type = 'text';
      this.appendChild(input);
      input.className = 'exceptionInput hidden';

      var select = cr.doc.createElement('select');
      var optionAllow = cr.doc.createElement('option');
      optionAllow.textContent = templateData.allowException;
      var optionBlock = cr.doc.createElement('option');
      optionBlock.textContent = templateData.blockException;
      select.appendChild(optionAllow);
      select.appendChild(optionBlock);
      this.appendChild(select);
      select.className = 'exceptionSetting hidden';

      this.patternLabel = patternLabel;
      this.settingLabel = settingLabel;
      this.input = input;
      this.select = select;
      this.optionAllow = optionAllow;
      this.optionBlock = optionBlock;

      this.updateEditables();

      // Handle events on the editable nodes.
      var eventsToStop =
          ['mousedown', 'mouseup', 'contextmenu', 'dblclick', 'paste'];
      eventsToStop.forEach(function(type) {
        input.addEventListener(type, function(e) {
            e.stopPropagation();
          });
        }
      );

      var listItem = this;
      // Handles enter and escape which trigger reset and commit respectively.
      function handleKeydown(e) {
        // Make sure that the tree does not handle the key.
        e.stopPropagation();

        // Calling list.focus blurs the input which will stop editing the list
        // item.
        switch (e.keyIdentifier) {
          case 'U+001B':  // Esc
            // Reset the inputs.
            listItem.updateEditables();
          case 'Enter':
            if (listItem.parentNode)
              listItem.parentNode.focus();
        }
      }

      function handleBlur(e) {
        // When the blur event happens we do not know who is getting focus so we
        // delay this a bit since we want to know if the other input got focus
        // before deciding if we should exit edit mode.
        var doc = e.target.ownerDocument;
        window.setTimeout(function() {
          var activeElement = doc.activeElement;
          if (!listItem.contains(activeElement)) {
            listItem.editing = false;
          }
        }, 50);
      }

      input.addEventListener('keydown', handleKeydown);
      input.addEventListener('blur', handleBlur);

      select.addEventListener('keydown', handleKeydown);
      select.addEventListener('blur', handleBlur);
    },

    /**
     * The pattern (e.g., a URL) for the exception.
     * @type {string}
     */
    get pattern() {
      return this.dataItem[0];
    },
    set pattern(pattern) {
      this.dataItem[0] = pattern;
    },

    /**
     * The setting (allow/block) for the exception.
     * @type {string}
     */
    get setting() {
      return this.dataItem[1];
    },
    set setting(setting) {
      this.dataItem[1] = setting;
    },

    /**
     * Gets a human-readable setting string.
     * @type {string}
     */
    settingForDisplay: function() {
      var setting = this.setting;
      if (setting == 'allow')
        return templateData.allowException;
      else if (setting == 'block')
        return templateData.blockException;
    },

    /**
     * Copy the data model values to the editable nodes.
     */
    updateEditables: function() {
      if (!(this.pattern && this.setting))
        return;

      this.input.value = this.pattern;

      if (this.setting == 'allow')
        this.optionAllow.selected = true;
      else if (this.setting == 'block')
        this.optionBlock.selected = true;
    },

    /**
     * Whether the user is currently able to edit the list item.
     * @type {boolean}
     */
    get editing() {
      return this.hasAttribute('editing');
    },
    set editing(editing) {
      var oldEditing = this.editing;
      if (oldEditing == editing)
        return;

      var listItem = this;
      var pattern = this.pattern;
      var setting = this.setting;
      var patternLabel = this.patternLabel;
      var settingLabel = this.settingLabel;
      var input = this.input;
      var select = this.select;
      var optionAllow = this.optionAllow;
      var optionBlock = this.optionBlock;

      patternLabel.classList.toggle('hidden');
      settingLabel.classList.toggle('hidden');
      input.classList.toggle('hidden');
      select.classList.toggle('hidden');

      var doc = this.ownerDocument;
      if (editing) {
        this.setAttribute('editing', '');
        cr.ui.limitInputWidth(input, this, 20);
        input.focus();
        input.select();
      } else {
        // Check that we have a valid pattern and if not we do not change then
        // editing mode.
        var newPattern = input.value;

        // TODO(estade): check for pattern validity.
        if (false) {
          // In case the item was removed before getting here we should
          // not alert.
          if (listItem.parentNode) {
            // TODO(estade): i18n
            alert('invalid pattern');
          }
          input.focus();
          input.select();
          return;
        }

        this.pattern = patternLabel.textContent = newPattern;
        if (optionAllow.selected)
          this.setting = 'allow';
        else if (optionBlock.selected)
          this.setting = 'block';
        settingLabel.textContent = this.settingForDisplay();

        this.removeAttribute('editing');

        if (pattern != this.pattern)
          chrome.send('removeImageExceptions', [pattern]);

        chrome.send('setImageException', [this.pattern, this.setting]);
      }
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
     * Adds an exception to the js model.
     * @param {Array} entry A pair of the form [filter, setting].
     */
    addException: function(entry) {
      this.dataModel.push(entry);

      // When an empty row is added, put it into editing mode.
      if (!entry[0] && !entry[1]) {
        var index = this.dataModel.length - 1;
        var sm = this.selectionModel;
        sm.anchorIndex = sm.leadIndex = sm.selectedIndex = index;
        this.scrollIndexIntoView(index);
        var li = this.getListItemByIndex(index);
        li.editing = true;
      }
    },

    /**
     * Removes all exceptions from the js model.
     */
    clear: function() {
      this.dataModel = new ArrayDataModel([]);
    },

    /**
     * Removes all selected rows from browser's model.
     */
    removeSelectedRows: function() {
      var removePatterns = [];
      var selectedItems = this.selectedItems;
      for (var i = 0; i < selectedItems.length; ++i) {
        removePatterns.push(selectedItems[i][0]);
      }

      chrome.send('removeImageExceptions', removePatterns);
    },

    /**
     * Puts the selected row in editing mode.
     */
    editSelectedRow: function() {
      var li = this.getListItem(this.selectedItem);
      if (li)
        li.editing = true;
    }
  };

  var ExceptionsArea = cr.ui.define('div');

  ExceptionsArea.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      ExceptionsList.decorate($('imagesExceptionsList'));
      this.boundHandleOnSelectionChange_ =
          cr.bind(this.handleOnSelectionChange_, this);
      imagesExceptionsList.selectionModel.addEventListener(
          'change', this.boundHandleOnSelectionChange_);

      var addRow = cr.doc.createElement('button');
      addRow.textContent = templateData.addExceptionRow;
      this.appendChild(addRow);

      var editRow = cr.doc.createElement('button');
      editRow.textContent = templateData.editExceptionRow;
      this.appendChild(editRow);
      this.editRow = editRow;

      var removeRow = cr.doc.createElement('button');
      removeRow.textContent = templateData.removeExceptionRow;
      this.appendChild(removeRow);
      this.removeRow = removeRow;

      addRow.onclick = function(event) {
        imagesExceptionsList.addException(['', '']);
      };

      editRow.onclick = function(event) {
        imagesExceptionsList.editSelectedRow();
      };

      removeRow.onclick = function(event) {
        imagesExceptionsList.removeSelectedRows();
      };

      this.updateButtonSensitivity();
    },

    /**
     * Update the enabled/disabled state of the editing buttons based on which
     * rows are selected.
     */
    updateButtonSensitivity: function() {
      var selectionSize = imagesExceptionsList.selectedItems.length;
      this.editRow.disabled = selectionSize != 1;
      this.removeRow.disabled = selectionSize == 0;
    },

    /**
     * Callback from the selection model.
     * @param {!cr.Event} ce Event with change info.
     * @private
     */
    handleOnSelectionChange_: function(ce) {
      this.updateButtonSensitivity();
   },
  };

  return {
    ExceptionsListItem: ExceptionsListItem,
    ExceptionsList: ExceptionsList,
    ExceptionsArea: ExceptionsArea
  };
});
