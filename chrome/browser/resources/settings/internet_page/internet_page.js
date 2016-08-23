// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-page' is the settings page containing internet
 * settings.
 */
Polymer({
  is: 'settings-internet-page',

  properties: {
    /** The network type for the known networks subpage. */
    knownNetworksType: {
      type: String,
    },

    /** Whether the 'Add connection' section is expanded. */
    addConnectionExpanded: {
      type: Boolean,
      value: false,
    },

    /**
     * Interface for networkingPrivate calls. May be overriden by tests.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: {
      type: Object,
      value: chrome.networkingPrivate,
    },
  },

  /**
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onShowDetail_: function(event) {
    settings.navigateTo(
        settings.Route.NETWORK_DETAIL,
        new URLSearchParams('guid=' + event.detail.GUID));
  },

  /**
   * @param {!{detail: {type: string}}} event
   * @private
   */
  onShowKnownNetworks_: function(event) {
    this.knownNetworksType = event.detail.type;
    settings.navigateTo(settings.Route.KNOWN_NETWORKS);
  },

  /**
   * Event triggered when the 'Add connections' div is tapped.
   * @param {Event} event
   * @private
   */
  onExpandAddConnectionsTap_: function(event) {
    if (event.target.id == 'expandAddConnections')
      return;
    this.addConnectionExpanded = !this.addConnectionExpanded;
  },

  /*** @private */
  onAddWiFiTap_: function() {
    chrome.send('addNetwork', [CrOnc.Type.WI_FI]);
  },

  /*** @private */
  onAddVPNTap_: function() {
    chrome.send('addNetwork', [CrOnc.Type.VPN]);
  },
});
