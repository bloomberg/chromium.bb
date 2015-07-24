// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and modifying a list of cellular
 * access points.
 */
Polymer({
  is: 'network-apnlist',

  properties: {
    /**
     * The current state for the network matching |guid|.
     * @type {?CrOnc.NetworkStateProperties}
     */
    networkState: {
      type: Object,
      value: null,
      observer: 'networkStateChanged_'
    },

    /**
     * The CrOnc.APNProperties.AccessPointName value of the selected APN.
     */
    selectedApn: {
      type: String,
      value: ''
    },

    /**
     * Selectable list of APN dictionaries for the UI. Includes an entry
     * corresponding to |otherApn| (see below).
     * @type {!Array<!CrOnc.APNProperties>}
     */
    apnSelectList: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * The user settable properties for a new ('other') APN. The values for
     * AccessPointName, Username, and Password will be set to the currently
     * active APN if it does not match an existing list entry.
     * @type {!CrOnc.APNProperties}
     */
    otherApn: {
      type: Object,
      value: function() { return {}; }
    },

    /**
     * Array of property names to pass to the Other APN property list.
     * @type {!Array<string>}
     */
    otherApnFields_: {
      type: Array,
      value: function() {
        return ['AccessPointName', 'Username', 'Password'];
      },
      readOnly: true
    },

    /**
     * Array of edit types to pass to the Other APN property list.
     */
    otherApnEditTypes_: {
      type: Object,
      value: function() {
        return {
          'AccessPointName': 'String',
          'Username': 'String',
          'Password': 'String'
        };
      },
      readOnly: true
    },
  },

  /** @const */ DefaultAccessPointName: 'none',

  /**
   * Polymer networkState changed method.
   */
  networkStateChanged_: function() {
    if (!this.networkState || !this.networkState.Cellular)
      return;

    var activeApn = null;
    var cellular = this.networkState.Cellular;
    if (cellular.APN && cellular.APN.AccessPointName)
      activeApn = cellular.APN;
    else if (cellular.LastGoodAPN && cellular.LastGoodAPN.AccessPointName)
      activeApn = cellular.LastGoodAPN;
    this.setApnSelectList_(activeApn);
  },

  /**
   * Sets the list of selectable APNs for the UI. Appends an 'Other' entry
   * (see comments for |otherApn| above).
   * @param {?CrOnc.APNProperties} activeApn The currently active APN value.
   * @private
   */
  setApnSelectList_: function(activeApn) {
    var apnList = this.networkState.Cellular.APNList || [];
    var result = apnList.slice();
    var activeApnInList = result.some(
        function(a) { return a.AccessPointName == activeApn.AccessPointName; });

    var otherApn = {};
    if (!activeApnInList && activeApn)
      Object.assign(otherApn, activeApn);
    else
      Object.assign(otherApn, this.otherApn);

    // Always use 'Other' for the name of custom APN entries (the name does
    // not get saved).
    otherApn.Name = 'Other';
    otherApn.AccessPointName =
        otherApn.AccessPointName || this.DefaultAccessPointName;
    this.set('otherApn', otherApn);
    result.push(otherApn);

    this.set('apnSelectList', result);
    this.set('selectedApn',
             activeApn.AccessPointName || this.otherApn.AccessPointName);
  },

  /**
   * We need to update the select value after the dom-repeat template updates:
   * 1. Rebuilding the template options resets the select value property.
   * 2. The template update occurs after any property changed events.
   * TODO(stevenjb): Remove once we use cr-dropdown-menu which (hopefully)
   * won't require this.
   * @private
   */
  onSelectApnUpdated_: function() {
    this.$.selectApn.value = this.selectedApn;
  },

  /**
   * Event triggered when the selectApn seleciton changes.
   * @param {Event} event The select node change event.
   * @private
   */
  onSelectApnChange_: function(event) {
    var apn = event.target.value;
    // Don't send a change event for 'Other' until the 'Save' button is clicked,
    // unless it has been changed from the default.
    if (!this.isOtherSelected_(this.networkState, apn) ||
        this.otherApn.AccessPointName != this.DefaultAccessPointName) {
      this.sendApnChange_(apn);
    }
  },

  /**
   * Event triggered when any 'Other' APN network property changes.
   * @param {!{detail: { field: string, value: string}}} event
   * @private
   */
  onOtherApnChange_: function(event) {
    this.set('otherApn.' + event.detail.field, event.detail.value);
    // Don't send a change event for 'Other' until the 'Save' button is clicked.
  },

  /**
   * Event triggered when the Other APN 'Save' button is clicked.
   * @param {Event} event
   * @private
   */
  onSaveOther_: function(event) {
    this.sendApnChange_(this.selectedApn);
  },

  /**
   * Send the apn-change event.
   * @param {!CrOnc.APNProperties} selectedApn
   * @private
   */
  sendApnChange_: function(selectedApn) {
    var apnList = this.networkState.Cellular.APNList || [];
    var apn = this.findApnInList(apnList, selectedApn);
    if (apn == undefined) {
      apn = {
        'AccessPointName': this.otherApn.AccessPointName,
        'Username': this.otherApn.Username,
        'Password': this.otherApn.Password
      };
    }
    this.fire('apn-change', {field: 'APN', value: apn});
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} networkState
   * @param {string} selectedApn
   * @return {boolean} True if the 'other' APN is currently selected.
   * @private
   */
  isOtherSelected_: function(networkState, selectedApn) {
    if (!networkState || !networkState.Cellular)
      return false;
    var apnList = networkState.Cellular.APNList || [];
    var apn = this.findApnInList(apnList, selectedApn);
    return apn == undefined;
  },

  /**
   * @param {!CrOnc.APNProperties} apn
   * @return {string} The most descriptive name for the access point.
   * @private
   */
  apnDesc_: function(apn) {
    return apn.LocalizedName || apn.Name || apn.AccessPointName;
  },

  /**
   * @param {!Array<!CrOnc.APNProperties>} apnList
   * @param {string} accessPointName
   * @return {CrOnc.APNProperties|undefined} The entry in |apnList| matching
   *     |accessPointName| if it exists, or undefined.
   * @private
   */
  findApnInList: function(apnList, accessPointName) {
    for (var a of apnList) {
      if (a.AccessPointName == accessPointName)
        return a;
    }
    return undefined;
  }
});
