// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    /**
     * A text label for the drop-down menu.
     */
    label: {
      type: String,
    },

    /**
     * List of options for the drop-down menu.
     * @type {!Array<{0: (Object|number|string), 1: string,
     *   2: (string|undefined)}>}
     */
    menuOptions: {
      notify: true,
      type: Array,
      value: function() { return []; },
    },

    /**
     * A single Preference object being tracked.
     * @type {?PrefObject}
     */
    pref: {
      type: Object,
      notify: true,
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
     * A reverse lookup from the menu value back to the index in the
     * menuOptions array.
     * @private {!Object<string, string>}
     */
    menuMap_: {
      type: Object,
      value: function() { return {}; },
    },

    /**
     * The current selected item (an index number as a string).
     * @private
     */
    selected_: {
      notify: true,
      observer: 'onSelectedChanged_',
      type: String,
    },

    /**
     * The current selected pref value.
     * @private
     */
    selectedValue_: {
      type: String,
    },

    /**
     * Whether to show the 'custom' item.
     * @private
     */
    showNotFoundValue_: {
      type: Boolean,
    },
  },

  behaviors: [
    I18nBehavior
  ],

  observers: [
    'checkSetup_(menuOptions, selectedValue_)',
    'prefChanged_(pref.value)',
  ],

  /**
   * Check to see if we have all the pieces needed to enable the control.
   * @param {!Array<{0: (Object|number|string), 1: string,
   *   2: (string|undefined)}>} menuOptions
   * @param {string} selectedValue_
   * @private
   */
  checkSetup_: function(menuOptions, selectedValue_) {
    if (!this.menuOptions.length) {
      return;
    }

    if (!Object.keys(this.menuMap_).length) {
      // Create a map from index value [0] back to the index i.
      var result = {};
      for (var i = 0; i < this.menuOptions.length; ++i)
        result[JSON.stringify(this.menuOptions[i][0])] = i.toString();
      this.menuMap_ = result;
    }

    // We need the menuOptions and the selectedValue_.  They may arrive
    // at different times (each is asynchronous).
    this.selected_ = this.getItemIndex(this.selectedValue_);
    this.menuLabel_ = this.label;
    this.$.dropdownMenu.disabled = false;
  },

  /**
   * @param {string} item A value from the menuOptions array.
   * @return {string}
   * @private
   */
  getItemIndex: function(item) {
    var result = this.menuMap_[item];
    if (result)
      return result;
    this.showNotFoundValue_ = true;
    // The 'not found' item is added as the last of the options.
    return (this.menuOptions.length).toString();
  },

  /**
   * @param {string} index An index into the menuOptions array.
   * @return {Object|number|string|undefined}
   * @private
   */
  getItemValue: function(index) {
    if (this.menuOptions.length) {
      var result = this.menuOptions[index];
      if (result)
        return result[0];
    }
    return undefined;
  },

  /**
   * Pass the selection change to the pref value.
   * @private
   */
  onSelectedChanged_: function() {
    var prefValue = this.getItemValue(this.selected_);
    if (prefValue !== undefined) {
      this.selectedValue_ = JSON.stringify(prefValue);
      this.set('pref.value', prefValue);
    }
  },

  /**
   * @param {number|string} value A value from the menuOptions array.
   * @private
   */
  prefChanged_: function(value) {
    this.selectedValue_ = JSON.stringify(value);
  },
});
