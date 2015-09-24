// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of network state
 * properties in a list in the format:
 *    Key1.........Value1
 *    KeyTwo.......ValueTwo
 * This also supports editing fields inline for fields listed in editFieldTypes:
 *    KeyThree....._________
 * TODO(stevenjb): Translate the keys and (where appropriate) values.
 */
Polymer({
  is: 'network-property-list',

  properties: {
    /**
     * The network state containing the properties to display.
     * @type {CrOnc.NetworkStateProperties|undefined}
     */
    networkState: {
      type: Object
    },

    /**
     * Fields to display.
     * @type {!Array<string>}
     */
    fields: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * Edit type of editable fields. May contain a property for any field in
     * |fields|. Other properties will be ignored. Property values can be:
     *   'String' - A text input will be displayed.
     *   TODO(stevenjb): Support types with custom validation, e.g. IPAddress.
     *   TODO(stevenjb): Support 'Number'.
     * When a field changes, the 'property-change' event will be fired with
     * the field name and the new value provided in the event detail.
     */
    editFieldTypes: {
      type: Object,
      value: function() { return {}; }
    },
  },

  /**
   * Event triggered when an input field changes. Fires a 'property-change'
   * event with the field (property) name set to the target id, and the value
   * set to the target input value.
   * @param {Event} event The input change event.
   * @private
   */
  onValueChange_: function(event) {
    if (!this.networkState)
      return;
    var field = event.target.id;
    var curValue = CrOnc.getActiveValue(this.networkState, field);
    var newValue = event.target.value;
    if (newValue == curValue)
      return;
    this.fire('property-change', {field: field, value: newValue});
  },

  /**
   * @param {string} key The property key.
   * @return {string} The text to display for the property label.
   * @private
   */
  getPropertyLabel_: function(key) {
    // TODO(stevenjb): Localize.
    return key;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @param {string} key The property key.
   * @return {boolean} Whether or not the property exists in |state|.
   * @private
   */
  hasPropertyValue_: function(state, key) {
    var value = (state && this.get(key, state)) || undefined;
    return (value !== undefined && value !== '');
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @param {Object} editFieldTypes The editFieldTypes object.
   * @param {string} key The property key.
   * @return {boolean} Whether or not to show the property. Editable properties
   *     are always shown.
   * @private
   */
  showProperty_: function(state, editFieldTypes, key) {
    if (editFieldTypes.hasOwnProperty(key))
      return true;
    return this.hasPropertyValue_(state, key);
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @param {Object} editFieldTypes The editFieldTypes object.
   * @param {string} key The property key.
   * @return {boolean} True if |key| exists in |state| and is not editable.
   * @private
   */
  showNoEdit_: function(state, editFieldTypes, key) {
    if (!this.hasPropertyValue_(state, key))
      return false;
    var editType = editFieldTypes[key];
    return !editType;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @param {Object} editFieldTypes The editFieldTypes object.
   * @param {string} key The property key.
   * @param {string} type The field type.
   * @return {boolean} True if |key| exists in |state| and is of editable
   *     type |type|.
   * @private
   */
  showEdit_: function(state, editFieldTypes, key, type) {
    return editFieldTypes[key] == type;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @param {string} key The property key.
   * @return {string} The text to display for the property value.
   * @private
   */
  getPropertyValue_: function(state, key) {
    if (!state)
      return '';
    var value = CrOnc.getActiveValue(state, key);
    if (value === undefined)
      return '';
    // TODO(stevenjb): Localize.
    return /** @type {string} */(value);
  },
});
