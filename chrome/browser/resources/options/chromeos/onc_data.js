// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ONC Data support class. Wraps a dictionary object containing
 * ONC managed or unmanaged dictionaries. Supports nested dictionaries,
 * e.g. data.getManagedProperty('VPN.Type').
 */
cr.define('cr.onc', function() {
  'use strict';

  function OncData(data) {
    this.data_ = data;
  }

  OncData.prototype = {

    /**
     * Returns either a managed property dictionary or an unmanaged value.
     * @param {string} key The property key.
     * @return {*} The property value or dictionary if it exists, otherwise
     *             undefined.
     */
    getManagedProperty: function(key) {
      var data = this.data_;
      while (true) {
        var index = key.indexOf('.');
        if (index < 0)
          break;
        var keyComponent = key.substr(0, index);
        if (!(keyComponent in data))
          return undefined;
        data = data[keyComponent];
        key = key.substr(index + 1);
      }
      return data[key];
    },

    /**
     * Sets the value of a property. Currently only supports unmanaged
     * properties.
     * @param {string} key The property key.
     * @param {string} value The property value to set.
     */
    setManagedProperty: function(key, value) {
      var data = this.data_;
      while (true) {
        var index = key.indexOf('.');
        if (index < 0)
          break;
        var keyComponent = key.substr(0, index);
        if (!(keyComponent in data))
          data[keyComponent] = {};
        data = data[keyComponent];
        key = key.substr(index + 1);
      }
      if (!(key in data) ||
          (typeof data[key] != 'object') ||
          (!('Active' in data[key]) && !('Effective' in data[key]))) {
        data[key] = value;
      } else {
        var effective = data[key]['Effective'];
        assert(effective != 'UserPolicy' || data[key]['UserEditable']);
        assert(effective != 'DevicePolicy' || data[key]['DeviceEditable']);
        // For now, just update the active value. TODO(stevenjb): Eventually we
        // should update the 'UserSetting' and 'Effective' properties correctly
        // and send that back to Chrome.
        data[key]['Active'] = value;
      }
    },

    /**
     * Gets the active value of a property.
     * @param {string} key The property key.
     * @return {*} The property value or undefined.
     */
    getActiveValue: function(key) {
      var property = this.getManagedProperty(key);
      if (Array.isArray(property) || typeof property != 'object')
        return property;
      // Otherwise get the Active value (default behavior).
      if ('Active' in property)
        return property['Active'];
      // If no Active value is defined, return the effective value if present.
      var effective = this.getEffectiveValueFromProperty_(property);
      if (effective != undefined)
        return effective;
      // Otherwise this is an Object but not a Managed one.
      return property;
    },

    /**
     * Gets the translated ONC value from the result of getActiveValue() using
     * loadTimeData. If no translation exists, returns the untranslated value.
     * @param {string} key The property key.
     * @return {*} The translation if available or the value if not.
     */
    getTranslatedValue: function(key) {
      var value = this.getActiveValue(key);
      if (typeof value != 'string')
        return value;
      var oncString = 'Onc' + key + value;
      // Handle special cases
      if (key == 'Name' && this.getActiveValue('Type') == 'Ethernet')
        return loadTimeData.getString('ethernetName');
      if (key == 'VPN.Type' && value == 'L2TP-IPsec') {
        var auth = this.getActiveValue('VPN.IPsec.AuthenticationType');
        if (auth != undefined)
          oncString += auth;
      }
      oncString = oncString.replace(/\./g, '-');
      if (loadTimeData.valueExists(oncString))
        return loadTimeData.getString(oncString);
      return value;
    },

    /**
     * Gets the recommended value of a property.
     * @param {string} key The property key.
     * @return {*} The property value or undefined.
     */
    getRecommendedValue: function(key) {
      var property = this.getManagedProperty(key);
      if (Array.isArray(property) || typeof property != 'object')
        return undefined;
      if (property['UserEditable'])
        return property['UserPolicy'];
      if (property['DeviceEditable'])
        return property['DevicePolicy'];
      // No value recommended by policy.
      return undefined;
    },

    /**
     * Updates the properties of |data_| from the properties in |update|.
     * Note: this only looks at top level entries, so if a dictionary is
     * updated the entire dictionary is written over. TODO(stevenjb):
     * eliminate this function when |data_| contains only ONC entries and
     * any updates consist of complete ONC dictionaries.
     * @param {Object} update Dictionary containing the updated properties.
     */
    updateData: function(update) {
      for (var prop in update) {
        if (prop in this.data_)
          this.data_[prop] = update[prop];
      }
    },

    /**
     * Get the effective value from a Managed property ONC dictionary.
     * @param {object} property The managed property ONC dictionary.
     * @return {*} The effective value or undefined.
     * @private
     */
    getEffectiveValueFromProperty_: function(property) {
      if ('Effective' in property) {
        var effective = property.Effective;
        if (effective in property)
          return property[effective];
      }
      return undefined;
    }
  };

  return {
    OncData: OncData
  };
});
