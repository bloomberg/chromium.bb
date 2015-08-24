// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-internet-known-networks' is the settings subpage listing the
 * known networks for a type (currently always WiFi).
 *
 * @group Chrome Settings Elements
 * @element cr-settings-internet-known-networks
 */
Polymer({
  is: 'cr-settings-internet-known-networks-page',

  properties: {
    /**
     * ID of the page.
     */
    PAGE_ID: {
      type: String,
      value: 'internet-known-networks',
      readOnly: true
    },

    /**
     * Route for the page.
     */
    route: {
      type: String,
      value: ''
    },

    /**
     * Whether the page is a subpage.
     */
    subpage: {
      type: Boolean,
      value: false
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() {
        return loadTimeData.getString('internetKnownNetworksPageTitle');
      }
    },

    /**
     * Reflects the selected settings page. We use this to extract networkType
     * from window.location.href when this page is navigated to. This is a
     * workaround for a bug in the 1.0 version of more-routing where
     * selected-params='{{params}}' is not correctly setting params in
     * settings_main.html. TODO(stevenjb): Remove once more-routing is fixed.
     * @type {?{PAGE_ID: string}}
     */
    selectedPage: {
      type: Object,
      value: null,
      observer: 'selectedPageChanged_'
    },

    /**
     * Name of the 'core-icon' to show.
     */
    icon: {
      type: String,
      value: 'settings-ethernet',
      readOnly: true
    },

    /**
     * The type of networks to list.
     * @type {chrome.networkingPrivate.NetworkType}
     */
    networkType: {
      type: String,
      value: CrOnc.Type.WI_FI,
      observer: 'networkTypeChanged_',
    },

    /**
     * The maximum height in pixels for the list of networks.
     */
    maxHeight: {
      type: Number,
      value: 500
    },

    /**
     * List of all network state data for the network type.
     * @type {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList: {
      type: Array,
      value: function() { return []; }
    }
  },

  /**
   * Listener function for chrome.networkingPrivate.onNetworksChanged event.
   * @type {function(!Array<string>)}
   * @private
   */
  networksChangedListener_: function() {},

  /** @override */
  attached: function() {
    this.networksChangedListener_ = this.onNetworksChangedEvent_.bind(this);
    chrome.networkingPrivate.onNetworksChanged.addListener(
        this.networksChangedListener_);
  },

  /** @override */
  detached: function() {
    chrome.networkingPrivate.onNetworksChanged.removeListener(
        this.networksChangedListener_);
  },

  /**
   * Polymer type changed method.
   */
  networkTypeChanged_: function() {
    if (!this.networkType)
      return;
    this.refreshNetworks_();
  },

  /**
   * Polymer selectedPage changed method. TODO(stevenjb): Remove, see above.
   */
  selectedPageChanged_: function() {
    if ((this.selectedPage && this.selectedPage.PAGE_ID) != this.PAGE_ID)
      return;
    var href = window.location.href;
    var idx = href.lastIndexOf('/');
    var type = href.slice(idx + 1);
    if (type) {
      this.networkType =
          /** @type {chrome.networkingPrivate.NetworkType} */(type);
    }
  },

  /**
   * networkingPrivate.onNetworksChanged event callback.
   * @param {!Array<string>} networkIds The list of changed network GUIDs.
   * @private
   */
  onNetworksChangedEvent_: function(networkIds) {
    this.refreshNetworks_();
  },

  /**
   * Requests the list of network states from Chrome. Updates networkStates
   * once the results are returned from Chrome.
   * @private
   */
  refreshNetworks_: function() {
    var filter = {
      networkType: this.networkType,
      visible: false,
      configured: true
    };
    chrome.networkingPrivate.getNetworks(
        filter,
        function(states) { this.networkStateList = states; }.bind(this));
  },

  /**
   * Event triggered when a cr-network-list item is selected.
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onListItemSelected_: function(event) {
    var state = event.detail;
    MoreRouting.navigateTo('internet-detail', {guid: state.GUID});
  },

  /**
   * Event triggered when a cr-network-list item 'remove' button is pressed.
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onRemove_: function(event) {
    var state = event.detail;
    if (!state.GUID)
      return;
    chrome.networkingPrivate.forgetNetwork(state.GUID);
  },

  /**
   * Event triggered when a cr-network-list item 'preferred' button is toggled.
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onTogglePreferred_: function(event) {
    var state = event.detail;
    if (!state.GUID)
      return;
    var preferred = state.Priority > 0;
    var onc = { Priority: preferred ? 0 : 1 };
    chrome.networkingPrivate.setProperties(state.GUID, onc);
  },

  /**
   * Navigate to the previous page.
   * @private
   */
  navigateBack_: function() {
    MoreRouting.navigateTo('internet');
  }
});
