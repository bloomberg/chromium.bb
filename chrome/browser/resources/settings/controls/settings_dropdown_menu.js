// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

 /**
 * This tuple is made up of a (value, name, attribute). The value and name are
 * used by the dropdown menu. The attribute is optional 'user data' that is
 * ignored by the dropdown menu.
 * @typedef {{
 *   0: (number|string),
 *   1: string,
 *   2: (string|undefined)
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
 *
 * @group Chrome Settings Elements
 * @element settings-dropdown-menu
 */
Polymer({
  is: 'settings-dropdown-menu',

  properties: {
    /** A text label for the drop-down menu. */
    label: String,

    /**
     * List of options for the drop-down menu.
     * TODO(michaelpg): use named properties instead of indices.
     * @type {DropdownMenuOptionList}
     */
    menuOptions: {
      type: Array,
      value: function() { return []; },
    },

    /**
     * A single Preference object being tracked.
     * @type {!chrome.settingsPrivate.PrefObject|undefined}
     */
    pref: {
      type: Object,
      notify: true,
    },

    /** Whether the dropdown menu should be disabled. */
    disabled: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
    },

    /**
     * Either loading text or the label for the drop-down menu.
     * @private
     */
    menuLabel_: {
      type: String,
      value: function() { return loadTimeData.getString('loading'); },
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
    I18nBehavior,
  ],

  observers: [
    'checkSetup_(menuOptions)',
    'updateSelected_(pref.value)',
  ],

  ready: function() {
    this.checkSetup_(this.menuOptions);
  },

  /**
   * Check to see if we have all the pieces needed to enable the control.
   * @param {DropdownMenuOptionList} menuOptions
   * @private
   */
  checkSetup_: function(menuOptions) {
    if (!this.menuOptions.length)
      return;

    this.menuLabel_ = this.label;
    this.updateSelected_();
  },

  /**
   * Pass the selection change to the pref value.
   * @private
   */
  onSelect_: function() {
    if (!this.pref || this.selected_ == undefined ||
        this.selected_ == this.notFoundValue_) {
      return;
    }
    var prefValue = Settings.PrefUtil.stringToPrefValue(
        this.selected_, this.pref);
    if (prefValue !== undefined)
      this.set('pref.value', prefValue);
  },

  /**
   * Updates the selected item when the pref or menuOptions change.
   * @private
   */
  updateSelected_: function() {
    if (!this.pref)
      return;
    var prefValue = this.pref.value;
    var option = this.menuOptions.find(function(menuItem) {
      return menuItem[0] == prefValue;
    });
    if (option == undefined)
      this.selected_ = this.notFoundValue_;
    else
      this.selected_ = Settings.PrefUtil.prefToString(this.pref);
  },

  /**
   * @param {string} selected
   * @return {boolean}
   * @private
   */
  isSelectedNotFound_: function(selected) {
    return this.menuOptions && selected == this.notFoundValue_;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldDisableMenu_: function() {
    return this.disabled || !this.menuOptions.length;
  },
});
