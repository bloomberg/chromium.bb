// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The |name| is shown in the gui.  The |value| us use to set or compare with
 * the preference value.
 * @typedef {{
 *   name: string,
 *   value: (number|string)
 * }}
 */
var DropdownMenuOption;

/**
 * @typedef {!Array<!DropdownMenuOption>}
 */
var DropdownMenuOptionList;

/**
 * 'settings-dropdown-menu' is a control for displaying options
 * in the settings.
 *
 * Example:
 *
 *   <settings-dropdown-menu pref="{{prefs.foo}}">
 *   </settings-dropdown-menu>
 */
Polymer({
  is: 'settings-dropdown-menu',

  properties: {
    /**
     * List of options for the drop-down menu.
     * @type {?DropdownMenuOptionList}
     */
    menuOptions: {
      type: Array,
      value: null,
    },

    /** Whether the dropdown menu should be disabled. */
    disabled: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
    },

    /**
     * The current selected value, as a string.
     * @private
     */
    selected_: String,

    /**
     * The value of the 'custom' item.
     * @private
     */
    notFoundValue_: {
      type: String,
      value: 'SETTINGS_DROPDOWN_NOT_FOUND_ITEM',
    },
  },

  behaviors: [
    PrefControlBehavior,
  ],

  observers: [
    'updateSelected_(menuOptions, pref.value)',
  ],

  /**
   * Pass the selection change to the pref value.
   * @private
   */
  onChange_: function() {
    this.selected_ = this.$.dropdownMenu.value;

    if (this.selected_ == this.notFoundValue_)
      return;

    var prefValue = Settings.PrefUtil.stringToPrefValue(
        this.selected_, assert(this.pref));
    if (prefValue !== undefined)
      this.set('pref.value', prefValue);
  },

  /**
   * Updates the selected item when the pref or menuOptions change.
   * @private
   */
  updateSelected_: function() {
    if (this.menuOptions === null || !this.menuOptions.length)
      return;

    var prefValue = this.pref.value;
    var option = this.menuOptions.find(function(menuItem) {
      return menuItem.value == prefValue;
    });

    // Need to wait for the dom-repeat to render, before assigning a value to
    // |selected_|, otherwise select#value is not populated correctly.
    this.async(function() {
      this.selected_ = option == undefined ?
          this.notFoundValue_ :
          Settings.PrefUtil.prefToString(assert(this.pref));
    }.bind(this));
  },

  /**
   * @param {string} selected
   * @return {boolean}
   * @private
   */
  isSelectedNotFound_: function(selected) {
    return this.menuOptions.length > 0 && selected == this.notFoundValue_;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldDisableMenu_: function() {
    return this.disabled || this.menuOptions === null ||
        this.menuOptions.length == 0;
  },
});
