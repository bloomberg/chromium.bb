// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(sergiu): Write a base class such that both this and the content
// settings exceptions inherit from it (http://crbug.com/227520).
cr.define('options.managedUserSettings', function() {
  /** @const */ var ControlledSettingIndicator =
                    options.ControlledSettingIndicator;
  /** @const */ var InlineEditableItemList = options.InlineEditableItemList;
  /** @const */ var InlineEditableItem = options.InlineEditableItem;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Creates a new exceptions list item.
   *
   * @param {Object} exception A dictionary that contains the data of the
   *     exception.
   * @constructor
   * @extends {options.InlineEditableItem}
   */
  function ExceptionsListItem(exception) {
    var el = cr.doc.createElement('div');
    el.dataItem = exception;
    el.__proto__ = ExceptionsListItem.prototype;
    el.decorate();

    return el;
  }

  ExceptionsListItem.prototype = {
    __proto__: InlineEditableItem.prototype,

    /**
     * Called when an element is decorated as a list item.
     */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);

      this.isPlaceholder = !this.pattern;
      var patternCell = this.createEditableTextCell(this.pattern);
      patternCell.className = 'exception-pattern';
      patternCell.classList.add('weakrtl');
      this.contentElement.appendChild(patternCell);
      if (this.pattern)
        this.patternLabel = patternCell.querySelector('.static-text');
      var input = patternCell.querySelector('input');

      // TODO(stuartmorgan): Create an createEditableSelectCell abstracting
      // this code.
      // Setting label for display mode. |pattern| will be null for the 'add new
      // exception' row.
      if (this.pattern) {
        var settingLabel = cr.doc.createElement('span');
        settingLabel.textContent = this.settingForDisplay();
        settingLabel.className = 'exception-setting';
        settingLabel.setAttribute('displaymode', 'static');
        this.contentElement.appendChild(settingLabel);
        this.settingLabel = settingLabel;
      }

      // Setting select element for edit mode.
      var select = cr.doc.createElement('select');
      var optionAllow = cr.doc.createElement('option');
      optionAllow.textContent = loadTimeData.getString('allowException');
      optionAllow.value = 'allow';
      select.appendChild(optionAllow);
      var optionBlock = cr.doc.createElement('option');
      optionBlock.textContent = loadTimeData.getString('blockException');
      optionBlock.value = 'block';
      select.appendChild(optionBlock);

      this.contentElement.appendChild(select);
      select.className = 'exception-setting';
      if (this.pattern)
        select.setAttribute('displaymode', 'edit');

      // Used to track whether the URL pattern in the input is valid.
      // This will be true if the browser process has informed us that the
      // current text in the input is valid. Changing the text resets this to
      // false, and getting a response from the browser sets it back to true.
      // It starts off as false for empty string (new exceptions) or true for
      // already-existing exceptions (which we assume are valid).
      this.inputValidityKnown = this.pattern;
      // This one tracks the actual validity of the pattern in the input. This
      // starts off as true so as not to annoy the user when he adds a new and
      // empty input.
      this.inputIsValid = true;

      this.input = input;
      this.select = select;

      this.updateEditables();

      var listItem = this;
      // Handle events on the editable nodes.
      input.oninput = function(event) {
        listItem.inputValidityKnown = false;
        chrome.send('checkManualExceptionValidity',
                    [input.value]);
      };

      // Listen for edit events.
      this.addEventListener('canceledit', this.onEditCancelled_);
      this.addEventListener('commitedit', this.onEditCommitted_);
    },

    /**
     * The pattern (e.g., a URL) for the exception.
     *
     * @type {string}
     */
    get pattern() {
      return this.dataItem.pattern;
    },
    set pattern(pattern) {
      if (!this.editable)
        console.error('Tried to change uneditable pattern');

      this.dataItem.pattern = pattern;
    },

    /**
     * The setting (allow/block) for the exception.
     *
     * @type {string}
     */
    get setting() {
      return this.dataItem.setting;
    },
    set setting(setting) {
      this.dataItem.setting = setting;
    },

    /**
     * Gets a human-readable setting string.
     *
     * @return {string} The display string.
     */
    settingForDisplay: function() {
      return this.getDisplayStringForSetting(this.setting);
    },

    /**
     * Gets a human-readable display string for setting.
     *
     * @param {string} setting The setting to be displayed.
     * @return {string} The display string.
     */
    getDisplayStringForSetting: function(setting) {
      if (setting == 'allow')
        return loadTimeData.getString('allowException');
      else if (setting == 'block')
        return loadTimeData.getString('blockException');

      console.error('Unknown setting: [' + setting + ']');
      return '';
    },

    /**
     * Update this list item to reflect whether the input is a valid pattern.
     *
     * @param {boolean} valid Whether said pattern is valid in the context of a
     *     content exception setting.
     */
    setPatternValid: function(valid) {
      if (valid || !this.input.value)
        this.input.setCustomValidity('');
      else
        this.input.setCustomValidity(' ');
      this.inputIsValid = valid;
      this.inputValidityKnown = true;
    },

    /**
     * Set the <input> to its original contents. Used when the user quits
     * editing.
     */
    resetInput: function() {
      this.input.value = this.pattern;
    },

    /**
     * Copy the data model values to the editable nodes.
     */
    updateEditables: function() {
      this.resetInput();

      var settingOption =
          this.select.querySelector('[value=\'' + this.setting + '\']');
      if (settingOption)
        settingOption.selected = true;
    },

    /** @override */
    get currentInputIsValid() {
      return this.inputValidityKnown && this.inputIsValid;
    },

    /** @override */
    get hasBeenEdited() {
      var livePattern = this.input.value;
      var liveSetting = this.select.value;
      return livePattern != this.pattern || liveSetting != this.setting;
    },

    /**
     * Called when committing an edit.
     *
     * @param {Event} e The end event.
     * @private
     */
    onEditCommitted_: function(e) {
      var newPattern = this.input.value;
      var newSetting = this.select.value;

      this.finishEdit(newPattern, newSetting);
    },

    /**
     * Called when cancelling an edit; resets the control states.
     *
     * @param {Event} e The cancel event.
     * @private
     */
    onEditCancelled_: function() {
      this.updateEditables();
      this.setPatternValid(true);
    },

    /**
     * Editing is complete; update the model.
     *
     * @param {string} newPattern The pattern that the user entered.
     * @param {string} newSetting The setting the user chose.
     */
    finishEdit: function(newPattern, newSetting) {
      this.setting = newSetting;
      this.patternLabel.textContent = newPattern;
      this.settingLabel.textContent = this.settingForDisplay();
      var oldPattern = this.pattern;
      this.pattern = newPattern;
      var needsUpdate = false;

      if (oldPattern != newPattern) {
        needsUpdate = true;
        chrome.send('removeManualException', [oldPattern]);
      }

      // If only the setting is changed for this pattern and not the pattern
      // itself then the interface is already updated so we don't need to
      // trigger it then processing is completed.
      chrome.send('setManualException', [newPattern, newSetting, needsUpdate]);
    }
  };

  /**
   * Creates a new list item for the Add New Item row, which doesn't represent
   * an actual entry in the exceptions list but allows the user to add new
   * exceptions.
   *
   * @constructor
   * @extends {cr.ui.ExceptionsListItem}
   */
  function ExceptionsAddRowListItem() {
    var el = cr.doc.createElement('div');
    el.dataItem = [];
    el.__proto__ = ExceptionsAddRowListItem.prototype;
    el.decorate();

    return el;
  }

  ExceptionsAddRowListItem.prototype = {
    __proto__: ExceptionsListItem.prototype,

    decorate: function() {
      ExceptionsListItem.prototype.decorate.call(this);

      this.input.placeholder =
          loadTimeData.getString('addNewExceptionInstructions');

      // Set 'Allow' as default for new entries.
      this.setting = 'allow';
    },

    /**
     * Clear the <input> and let the placeholder text show again.
     */
    resetInput: function() {
      this.input.value = '';
    },

    /** @override */
    get hasBeenEdited() {
      return this.input.value != '';
    },

    /**
     * Editing is complete; update the model. As long as the pattern isn't
     * empty, we'll just add it.
     *
     * @param {string} newPattern The pattern that the user entered.
     * @param {string} newSetting The setting the user chose.
     */
    finishEdit: function(newPattern, newSetting) {
      this.resetInput();
      // We're adding a new entry, update the model once that is done.
      chrome.send('setManualException', [newPattern, newSetting, true]);
    },
  };

  /**
   * Compares two elements in the list. Removes the schema if present and then
   * compares the two strings.
   * @param {string} a First element to compare.
   * @param {string} b Second element to compare.
   * @return {int} Result of comparison between a and b.
   */
  function comparePatterns(a, b) {
    // Remove the schema (part before ://) if any.
    var stripString = function(str) {
      var indexStr = str.indexOf('://');
      if (indexStr != -1)
        return str.slice(indexStr + 3);
      return str;
    };
    if (a)
      a = stripString(a['pattern']);
    if (b)
      b = stripString(b['pattern']);
    return this.dataModel.defaultValuesCompareFunction(a, b);
  }

  /**
   * Creates a new exceptions list.
   *
   * @constructor
   * @extends {cr.ui.List}
   */
  var ExceptionsList = cr.ui.define('list');

  ExceptionsList.prototype = {
    __proto__: InlineEditableItemList.prototype,

    /**
     * Called when an element is decorated as a list.
     */
    decorate: function() {
      InlineEditableItemList.prototype.decorate.call(this);

      this.classList.add('settings-list');

      this.autoExpands = true;
      this.reset();
    },

    /**
     * Creates an item to go in the list.
     *
     * @param {Object} entry The element from the data model for this row.
     */
    createItem: function(entry) {
      if (entry) {
        return new ExceptionsListItem(entry);
      } else {
        var addRowItem = new ExceptionsAddRowListItem();
        addRowItem.deletable = false;
        return addRowItem;
      }
    },

    /**
     * Sets the exceptions in the js model.
     *
     * @param {Object} entries A list of dictionaries of values, each dictionary
     *     represents an exception.
     */
    setManualExceptions: function(entries) {
      // We don't want to remove the Add New Exception row.
      var deleteCount = this.dataModel.length - 1;

      var args = [0, deleteCount];
      args.push.apply(args, entries);
      this.dataModel.splice.apply(this.dataModel, args);
    },

    /**
     * The browser has finished checking a pattern for validity. Update the list
     * item to reflect this.
     *
     * @param {string} pattern The pattern.
     * @param {bool} valid Whether said pattern is valid in the context of a
     *     content exception setting.
     */
    patternValidityCheckComplete: function(pattern, valid) {
      var listItems = this.items;
      for (var i = 0; i < listItems.length; i++) {
        var listItem = listItems[i];
        // Don't do anything for messages for the item if it is not the intended
        // recipient, or if the response is stale (i.e. the input value has
        // changed since we sent the request to analyze it).
        if (pattern == listItem.input.value)
          listItem.setPatternValid(valid);
      }
    },

    /**
     * Removes all exceptions from the js model.
     */
    reset: function() {
      // The null creates the Add New Exception row.
      this.dataModel = new ArrayDataModel([null]);

      // Set the initial sort order.
      this.dataModel.setCompareFunction('pattern', comparePatterns.bind(this));
      this.dataModel.sort('pattern', 'asc');
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      var listItem = this.getListItemByIndex(index);
      if (!listItem.deletable)
        return;

      var dataItem = listItem.dataItem;
      var args = [];
      args.push(dataItem.pattern);

      chrome.send('removeManualException', args);
    },
  };

  var OptionsPage = options.OptionsPage;

  /**
   * Encapsulated handling of content settings list subpage.
   * @constructor
   * @class
   */
  function ManagedUserSettingsExceptionsArea() {
    OptionsPage.call(this, 'manualExceptions',
                     loadTimeData.getString('manualExceptionsTabTitle'),
                     'managed-user-exceptions-area');
  }

  cr.addSingletonGetter(ManagedUserSettingsExceptionsArea);

  ManagedUserSettingsExceptionsArea.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var exceptionsLists = this.pageDiv.querySelectorAll('list');
      for (var i = 0; i < exceptionsLists.length; i++) {
        options.managedUserSettings.ExceptionsList.decorate(exceptionsLists[i]);
      }

      $('managed-user-settings-exceptions-overlay-confirm').onclick =
          OptionsPage.closeOverlay.bind(OptionsPage);
    },

    /** @override */
    canShowPage: function() {
      return ManagedUserSettings.getInstance().isAuthenticated;
    },
  };

  return {
    ExceptionsListItem: ExceptionsListItem,
    ExceptionsAddRowListItem: ExceptionsAddRowListItem,
    ExceptionsList: ExceptionsList,
    ManagedUserSettingsExceptionsArea: ManagedUserSettingsExceptionsArea,
  };
});
