// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-known-networks' is the settings subpage listing the
 * known networks for a type (currently always WiFi).
 */
Polymer({
  is: 'settings-internet-known-networks-page',

  behaviors: [CrPolicyNetworkBehavior],

  properties: {
    /**
     * The type of networks to list.
     * @type {CrOnc.Type}
     */
    networkType: {
      type: String,
      observer: 'networkTypeChanged_',
    },

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: Object,

    /**
     * List of all network state data for the network type.
     * @private {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList_: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /** @private */
    showAddPreferred_: Boolean,

    /** @private */
    showRemovePreferred_: Boolean,

    /**
     * We always show 'Forget' since we do not know whether or not to enable
     * it until we fetch the managed properties, and we do not want an empty
     * menu.
     * @private
     */
    enableForget_: Boolean,
  },

  /** @private {string} */
  selectedGuid_: '',

  /**
   * Listener function for chrome.networkingPrivate.onNetworksChanged event.
   * @type {function(!Array<string>)}
   * @private
   */
  networksChangedListener_: function() {},

  /** @override */
  attached: function() {
    this.networksChangedListener_ = this.onNetworksChangedEvent_.bind(this);
    this.networkingPrivate.onNetworksChanged.addListener(
        this.networksChangedListener_);
  },

  /** @override */
  detached: function() {
    this.networkingPrivate.onNetworksChanged.removeListener(
        this.networksChangedListener_);
  },

  /** @private */
  networkTypeChanged_: function() {
    this.refreshNetworks_();
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
    if (!this.networkType)
      return;
    var filter = {
      networkType: this.networkType,
      visible: false,
      configured: true
    };
    this.networkingPrivate.getNetworks(filter, states => {
      this.networkStateList_ = states;
    });
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} state
   * @return {boolean}
   * @private
   */
  networkIsPreferred_: function(state) {
    // Currently we treat NetworkStateProperties.Priority as a boolean.
    return state.Priority > 0;
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} networkState
   * @return {boolean}
   * @private
   */
  networkIsNotPreferred_: function(networkState) {
    return networkState.Priority == 0;
  },

  /**
   * @return {boolean}
   * @private
   */
  havePreferred_: function() {
    return this.networkStateList_.find(
               state => this.networkIsPreferred_(state)) !== undefined;
  },

  /**
   * @return {boolean}
   * @private
   */
  haveNotPreferred_: function() {
    return this.networkStateList_.find(
               state => this.networkIsNotPreferred_(state)) !== undefined;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onMenuButtonTap_: function(event) {
    var button = /** @type {!HTMLElement} */ (event.target);
    this.selectedGuid_ =
        /** @type {!{model: !{item: !CrOnc.NetworkStateProperties}}} */ (event)
            .model.item.GUID;
    // We need to make a round trip to Chrome in order to retrieve the managed
    // properties for the network. The delay is not noticeable (~5ms) and is
    // preferable to initiating a query for every known network at load time.
    this.networkingPrivate.getManagedProperties(
        this.selectedGuid_, properties => {
          if (chrome.runtime.lastError || !properties) {
            console.error(
                'Unexpected error: ' + chrome.runtime.lastError.message);
            return;
          }
          var preferred = button.hasAttribute('preferred');
          if (this.isNetworkPolicyEnforced(properties.Priority)) {
            this.showAddPreferred_ = false;
            this.showRemovePreferred_ = false;
          } else {
            this.showAddPreferred_ = !preferred;
            this.showRemovePreferred_ = preferred;
          }
          this.enableForget_ = !this.isPolicySource(properties.Source);
          /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).showAt(button);
        });
    event.stopPropagation();
  },

  /** @private */
  onRemovePreferredTap_: function() {
    this.networkingPrivate.setProperties(this.selectedGuid_, {Priority: 0});
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  },

  /** @private */
  onAddPreferredTap_: function() {
    this.networkingPrivate.setProperties(this.selectedGuid_, {Priority: 1});
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  },

  /** @private */
  onForgetTap_: function() {
    this.networkingPrivate.forgetNetwork(this.selectedGuid_);
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  },

  /**
   * Fires a 'show-details' event with an item containing a |networkStateList_|
   * entry in the event model.
   * @param {!Event} event
   * @private
   */
  fireShowDetails_: function(event) {
    var state =
        /** @type {!{model: !{item: !CrOnc.NetworkStateProperties}}} */ (event)
            .model.item;
    this.fire('show-detail', state);
    event.stopPropagation();
  },
});
