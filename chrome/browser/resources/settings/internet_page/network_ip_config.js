// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the IP Config properties for
 * a network state. TODO(stevenjb): Allow editing of static IP configurations
 * when 'editable' is true.
 */
Polymer({
  is: 'network-ip-config',

  properties: {
    /**
     * The network properties dictionary containing the IP Config properties to
     * display and modify.
     * @type {!CrOnc.NetworkProperties|undefined}
     */
    networkProperties: {
      type: Object,
      observer: 'networkPropertiesChanged_',
    },

    /**
     * Whether or not the IP Address can be edited.
     */
    editable: {
      type: Boolean,
      value: false,
    },

    /**
     * State of 'Configure IP Addresses Automatically'.
     * @private
     */
    automatic_: {
      type: Boolean,
      value: true,
      observer: 'automaticChanged_',
    },

    /**
     * The currently visible IP Config property dictionary. The 'RoutingPrefix'
     * property is a human-readable mask instead of a prefix length.
     * @private {?{
     *   ipv4: !CrOnc.IPConfigUIProperties,
     *   ipv6: !CrOnc.IPConfigUIProperties
     * }}
     */
    ipConfig_: {
      type: Object,
      value: null,
    },

    /**
     * Array of properties to pass to the property list.
     * @private {!Array<string>}
     */
    ipConfigFields_: {
      type: Array,
      value: function() {
        return [
          'ipv4.IPAddress',
          'ipv4.RoutingPrefix',
          'ipv4.Gateway',
          'ipv6.IPAddress',
        ];
      },
      readOnly: true
    },
  },

  /**
   * Saved static IP configuration properties when switching to 'automatic'.
   * @private {!CrOnc.IPConfigUIProperties|undefined}
   */
  savedStaticIp_: undefined,

  /**
   * Polymer networkProperties changed method.
   */
  networkPropertiesChanged_: function(newValue, oldValue) {
    if (!this.networkProperties)
      return;

    if (newValue.GUID != (oldValue && oldValue.GUID))
      this.savedStaticIp_ = undefined;

    // Update the 'automatic' property.
    if (this.networkProperties.IPAddressConfigType) {
      var ipConfigType =
          CrOnc.getActiveValue(this.networkProperties.IPAddressConfigType);
      this.automatic_ = (ipConfigType != CrOnc.IPConfigType.STATIC);
    }

    if (this.networkProperties.IPConfigs) {
      // Update the 'ipConfig' property.
      var ipv4 =
          CrOnc.getIPConfigForType(this.networkProperties, CrOnc.IPType.IPV4);
      var ipv6 =
          CrOnc.getIPConfigForType(this.networkProperties, CrOnc.IPType.IPV6);
      this.ipConfig_ = {
        ipv4: this.getIPConfigUIProperties_(ipv4),
        ipv6: this.getIPConfigUIProperties_(ipv6)
      };
    }
  },

  /** @private */
  automaticChanged_: function() {
    if (!this.automatic_ || !this.ipConfig_) {
      // When switching from automatic, don't send any changes, ip-change will
      // be fired in onIPChange when a field is changed.
      return;
    }
    // Save the static IP configuration when switching to automatic.
    this.savedStaticIp_ = this.ipConfig_.ipv4;
    // Send the change.
    this.fire('ip-change', {
      field: 'IPAddressConfigType',
      value: CrOnc.IPConfigType.DHCP,
    });
  },

  /**
   * @param {!CrOnc.IPConfigProperties|undefined} ipconfig
   * @return {!CrOnc.IPConfigUIProperties} A new IPConfigUIProperties object
   *     with RoutingPrefix expressed as a string mask instead of a prefix
   *     length. Returns an empty object if |ipconfig| is undefined.
   * @private
   */
  getIPConfigUIProperties_: function(ipconfig) {
    var result = {};
    if (!ipconfig)
      return result;
    for (var key in ipconfig) {
      var value = ipconfig[key];
      if (key == 'RoutingPrefix')
        result.RoutingPrefix = CrOnc.getRoutingPrefixAsNetmask(value);
      else
        result[key] = value;
    }
    return result;
  },

  /**
   * @param {!CrOnc.IPConfigUIProperties} ipconfig The IP Config UI properties.
   * @return {!CrOnc.IPConfigProperties} A new IPConfigProperties object with
   *     RoutingPrefix expressed as a a prefix length.
   * @private
   */
  getIPConfigProperties_: function(ipconfig) {
    var result = {};
    for (var key in ipconfig) {
      var value = ipconfig[key];
      if (key == 'RoutingPrefix')
        result.RoutingPrefix = CrOnc.getRoutingPrefixAsLength(value);
      else
        result[key] = value;
    }
    return result;
  },

  /**
   * @return {boolean}
   * @private
   */
  showIPEditFields_: function() {
    return this.editable && !this.automatic_;
  },

  /**
   * @return {Object} An object with the edit type for each editable field.
   * @private
   */
  getIPEditFields_: function() {
    if (!this.editable || this.automatic_)
      return {};
    return {
      'ipv4.IPAddress': 'String',
      'ipv4.RoutingPrefix': 'String',
      'ipv4.Gateway': 'String'
    };
  },

  /**
   * Event triggered when the network property list changes.
   * @param {!{detail: {field: string, value: string}}} event The
   *     network-property-list change event.
   * @private
   */
  onIPChange_: function(event) {
    if (!this.ipConfig_)
      return;
    var field = event.detail.field;
    var value = event.detail.value;
    // Note: |field| includes the 'ipv4.' prefix.
    this.set('ipConfig_.' + field, value);
    // This will also set IPAddressConfigType to STATIC.
    this.fire('ip-change', {
      field: 'StaticIPConfig',
      value: this.getIPConfigProperties_(this.ipConfig_.ipv4)
    });
  },
});
