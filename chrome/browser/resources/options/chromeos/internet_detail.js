// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.internet', function() {
  var OptionsPage = options.OptionsPage;

  /*
   * Helper function to set hidden attribute on given element list.
   * @param {Array} elements List of elements to be updated.
   * @param {bool} hidden New hidden value.
   */
  function updateHidden(elements, hidden) {
    for (var i = 0, el; el = elements[i]; i++) {
      el.hidden = hidden;
    }
  }

  /////////////////////////////////////////////////////////////////////////////
  // DetailsInternetPage class:

  /**
   * Encapsulated handling of ChromeOS internet details overlay page.
   * @constructor
   */
  function DetailsInternetPage() {
    OptionsPage.call(this, 'detailsInternetPage', null, 'detailsInternetPage');
  }

  cr.addSingletonGetter(DetailsInternetPage);

  DetailsInternetPage.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initializes DetailsInternetPage page.
     * Calls base class implementation to starts preference initialization.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);
    },

    /**
     * Update details page controls.
     * @private
     */
    updateControls_: function() {
      // Connection state related.
      $('advancedSection').hidden = !this.connected;

      // Network type related.
      updateHidden(
          cr.doc.querySelectorAll('#detailsInternetPage .cellular-details'),
          !this.cellular);
      updateHidden(
          cr.doc.querySelectorAll('#detailsInternetPage .wifi-details'),
          !this.wireless);

      // Cell plan related
      $('planList').hidden = this.cellplanloading;
      updateHidden(
          cr.doc.querySelectorAll('#detailsInternetPage .no-plan-info'),
          !this.cellular || this.cellplanloading || this.hascellplan);
      updateHidden(
          cr.doc.querySelectorAll('#detailsInternetPage .plan-loading-info'),
          !this.cellular || this.nocellplan || this.hascellplan);
      updateHidden(
          cr.doc.querySelectorAll('#detailsInternetPage .plan-details-info'),
          !this.cellular || this.nocellplan  || this.cellplanloading);
      updateHidden(
          cr.doc.querySelectorAll('#detailsInternetPage .gsm-only'),
          !this.cellular || !this.gsm);
      updateHidden(
          cr.doc.querySelectorAll('#detailsInternetPage .cdma-only'),
          !this.cellular || this.gsm);

      updateHidden(
          cr.doc.querySelectorAll('#detailsInternetPage .password-details'),
          !this.wireless || !this.password);
    }
  };

  /**
   * Whether the underlying network is connected. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(DetailsInternetPage, 'connected',
      cr.PropertyKind.JS,
      DetailsInternetPage.prototype.updateControls_);

  /**
   * Whether the underlying network is wifi. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(DetailsInternetPage, 'wireless',
      cr.PropertyKind.JS,
      DetailsInternetPage.prototype.updateControls_);

  /**
   * Whether the underlying network is ethernet. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(DetailsInternetPage, 'ethernet',
      cr.PropertyKind.JS,
      DetailsInternetPage.prototype.updateControls_);

  /**
   * Whether the underlying network is cellular. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(DetailsInternetPage, 'cellular',
      cr.PropertyKind.JS,
      DetailsInternetPage.prototype.updateControls_);

  /**
   * Whether the network is loading cell plan. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(DetailsInternetPage, 'cellplanloading',
      cr.PropertyKind.JS,
      DetailsInternetPage.prototype.updateControls_);

  /**
   * Whether the network has cell plan(s). Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(DetailsInternetPage, 'hascellplan',
      cr.PropertyKind.JS,
      DetailsInternetPage.prototype.updateControls_);

  /**
   * Whether the network has no cell plan. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(DetailsInternetPage, 'nocellplan',
      cr.PropertyKind.JS,
      DetailsInternetPage.prototype.updateControls_);

  /**
   * Whether the network is gsm. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(DetailsInternetPage, 'gsm',
      cr.PropertyKind.JS,
      DetailsInternetPage.prototype.updateControls_);

  /**
   * Whether show password details for network. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(DetailsInternetPage, 'password',
      cr.PropertyKind.JS,
      DetailsInternetPage.prototype.updateControls_);

  // TODO(xiyuan): Check to see if it is safe to remove these attributes.
  cr.defineProperty(DetailsInternetPage, 'hasactiveplan',
      cr.PropertyKind.JS);
  cr.defineProperty(DetailsInternetPage, 'activated',
      cr.PropertyKind.JS);
  cr.defineProperty(DetailsInternetPage, 'connecting',
      cr.PropertyKind.JS);
  cr.defineProperty(DetailsInternetPage, 'connected',
      cr.PropertyKind.JS);

  return {
    DetailsInternetPage: DetailsInternetPage
  };
});
