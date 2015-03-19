// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A singleton that keeps track of the VPN providers enabled in
 * the primary user's profile.
 */

cr.define('options', function() {
  /**
   * @constructor
   */
  function VPNProviders() {
  }

  cr.addSingletonGetter(VPNProviders);

  VPNProviders.prototype = {
    /**
     * The VPN providers enabled in the primary user's profile. Each provider
     * has a name. Third-party VPN providers additionally have an extension ID.
     * @type {!Array<{name: string, extensionID: ?string}>}
     * @private
     */
    providers_: [],

    /**
     * Formats a network name for display purposes. If the network belongs to
     * a third-party VPN provider, the provider name is added to the network
     * name.
     * @param {cr.onc.OncData} onc ONC data describing this network.
     * @return {string} The resulting display name.
     * @private
     */
    formatNetworkName_: function(onc) {
      var networkName = onc.getTranslatedValue('Name');
      if (onc.getActiveValue('VPN.Type') != 'ThirdPartyVPN')
        return networkName;
      var extensionID = onc.getActiveValue('VPN.ThirdPartyVPN.ExtensionID');
      for (var i = 0; i < this.providers_.length; ++i) {
        if (extensionID == this.providers_[i].extensionID) {
          return loadTimeData.getStringF('vpnNameTemplate',
                                         this.providers_[i].name,
                                         networkName);
        }
      }
      return networkName;
    },
  };

  /**
   * Returns the list of VPN providers enabled in the primary user's profile.
   * @return {!Array<{name: string, extensionID: ?string}>} The list of VPN
   *     providers enabled in the primary user's profile.
   */
  VPNProviders.getProviders = function() {
    return VPNProviders.getInstance().providers_;
  };

  /**
   * Replaces the list of VPN providers enabled in the primary user's profile.
   * @param {!Array<{name: string, extensionID: ?string}>} providers The list
   *     of VPN providers enabled in the primary user's profile.
   */
  VPNProviders.setProviders = function(providers) {
    VPNProviders.getInstance().providers_ = providers;
  };

  /**
   * Formats a network name for display purposes. If the network belongs to a
   * third-party VPN provider, the provider name is added to the network name.
   * @param {cr.onc.OncData} onc ONC data describing this network.
   * @return {string} The resulting display name.
   */
  VPNProviders.formatNetworkName = function(onc) {
    return VPNProviders.getInstance().formatNetworkName_(onc);
  };

  // Export
  return {
    VPNProviders: VPNProviders
  };
});
