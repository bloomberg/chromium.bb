// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.internet', function() {
  var OptionsPage = options.OptionsPage;

  /*
   * Helper function to set hidden attribute for elements matching a selector.
   * @param {string} selector CSS selector for extracting a list of elements.
   * @param {bool} hidden New hidden value.
   */
  function updateHidden(selector, hidden) {
    var elements = cr.doc.querySelectorAll(selector);
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

      options.internet.CellularPlanElement.decorate($('planList'));

      $('detailsInternetDismiss').addEventListener('click', function(event) {
        InternetOptions.setDetails();
      });

      $('detailsInternetLogin').addEventListener('click', function(event) {
        InternetOptions.setDetails();
        InternetOptions.loginFromDetails();
      });

      $('detailsInternetDisconnect').addEventListener('click', function(event) {
        InternetOptions.setDetails();
        InternetOptions.disconnectNetwork();
      });

      $('activateDetails').addEventListener('click', function(event) {
        InternetOptions.activateFromDetails();
      });

      $('buyplanDetails').addEventListener('click', function(event) {
        chrome.send('buyDataPlan');
        OptionsPage.closeOverlay();
      });

      $('viewAccountDetails').addEventListener('click', function(event) {
        chrome.send('showMorePlanInfo');
        OptionsPage.closeOverlay();
      });

      $('cellularApnUseDefault').addEventListener('click', function(event) {
        var data = $('connectionState').data;
        var apnSelector = $('selectApn');

        if (data.userApnIndex != -1) {
          apnSelector.remove(data.userApnIndex);
          data.userApnIndex = -1;
        }

        if (data.providerApnList.value.length > 0) {
          var iApn = 0;
          data.apn.apn = data.providerApnList.value[iApn].apn;
          data.apn.username = data.providerApnList.value[iApn].username;
          data.apn.password = data.providerApnList.value[iApn].password;
          chrome.send('setApn', [String(data.servicePath),
                                 String(data.apn.apn),
                                 String(data.apn.username),
                                 String(data.apn.password)]);
          apnSelector.selectedIndex = iApn;
          data.selectedApn = iApn;
        } else {
          data.apn.apn = '';
          data.apn.username = '';
          data.apn.password = '';
          apnSelector.selectedIndex = -1;
          data.selectedApn = -1;
        }
        updateHidden('.apn-list-view', false);
        updateHidden('.apn-details-view', true);
      });

      $('cellularApnSet').addEventListener('click', function(event) {
        if ($('cellularApn').value == '')
          return;

        var data = $('connectionState').data;
        var apnSelector = $('selectApn');

        data.apn.apn = String($('cellularApn').value);
        data.apn.username = String($('cellularApnUsername').value);
        data.apn.password = String($('cellularApnPassword').value);
        chrome.send('setApn', [String(data.servicePath),
                                String(data.apn.apn),
                                String(data.apn.username),
                                String(data.apn.password)]);

        if (data.userApnIndex != -1) {
          apnSelector.remove(data.userApnIndex);
          data.userApnIndex = -1;
        }

        var option = document.createElement('option');
        option.textContent = data.apn.apn;
        option.value = -1;
        option.selected = true;
        apnSelector.add(option, apnSelector[apnSelector.length - 1]);
        data.userApnIndex = apnSelector.length - 2;
        data.selectedApn = data.userApnIndex;

        updateHidden('.apn-list-view', false);
        updateHidden('.apn-details-view', true);
      });

      $('cellularApnCancel').addEventListener('click', function(event) {
        $('selectApn').selectedIndex = $('connectionState').data.selectedApn;
        updateHidden('.apn-list-view', false);
        updateHidden('.apn-details-view', true);
      });

      $('selectApn').addEventListener('change', function(event) {
        var data = $('connectionState').data;
        var apnSelector = $('selectApn');
        if (apnSelector[apnSelector.selectedIndex].value != -1) {
          var apnList = data.providerApnList.value;
          chrome.send('setApn', [String(data.servicePath),
              String(apnList[apnSelector.selectedIndex].apn),
              String(apnList[apnSelector.selectedIndex].username),
              String(apnList[apnSelector.selectedIndex].password)
          ]);
          data.selectedApn = apnSelector.selectedIndex;
        } else if (apnSelector.selectedIndex == data.userApnIndex) {
          chrome.send('setApn', [String(data.servicePath),
                                 String(data.apn.apn),
                                 String(data.apn.username),
                                 String(data.apn.password)]);
          data.selectedApn = apnSelector.selectedIndex;
        } else {
          $('cellularApn').value = data.apn.apn;
          $('cellularApnUsername').value = data.apn.username;
          $('cellularApnPassword').value = data.apn.password;

          updateHidden('.apn-list-view', true);
          updateHidden('.apn-details-view', false);
        }
      });

      $('sim-card-lock-enabled').addEventListener('click', function(event) {
        var newValue = $('sim-card-lock-enabled').checked;
        // Leave value as is because user needs to enter PIN code first.
        // When PIN will be entered and value changed,
        // we'll update UI to reflect that change.
        $('sim-card-lock-enabled').checked = !newValue;
        chrome.send('setSimCardLock', [newValue]);
      });
      $('change-pin').addEventListener('click', function(event) {
        chrome.send('changePin');
      });
    },

    /**
     * Update details page controls.
     * @private
     */
    updateControls_: function() {
      // Only show ipconfig section if network is connected OR if nothing on
      // this device is connected. This is so that you can fix the ip configs
      // if you can't connect to any network.
      // TODO(chocobo): Once ipconfig is moved to flimflam service objects,
      //   we need to redo this logic to allow configuration of all networks.
      $('ipconfigSection').hidden = !this.connected && this.deviceConnected;

      // Network type related.
      updateHidden('#detailsInternetPage .cellular-details', !this.cellular);
      updateHidden('#detailsInternetPage .wifi-details', !this.wireless);
      updateHidden('#detailsInternetPage .vpn-details', !this.vpn);
      updateHidden('#detailsInternetPage .proxy-details', !this.showProxy);

      // Cell plan related.
      $('planList').hidden = this.cellplanloading;
      updateHidden('#detailsInternetPage .no-plan-info',
                   !this.cellular || this.cellplanloading || this.hascellplan);
      updateHidden('#detailsInternetPage .plan-loading-info',
                   !this.cellular || this.nocellplan || this.hascellplan);
      updateHidden('#detailsInternetPage .plan-details-info',
                   !this.cellular || this.nocellplan || this.cellplanloading);
      updateHidden('#detailsInternetPage .gsm-only',
                   !this.cellular || !this.gsm);
      updateHidden('#detailsInternetPage .cdma-only',
                   !this.cellular || this.gsm);
      updateHidden('#detailsInternetPage .apn-list-view',
                   !this.cellular || !this.gsm);
      updateHidden('#detailsInternetPage .apn-details-view', true);

      // Password and shared.
      updateHidden('#detailsInternetPage .password-details',
                   !this.wireless || !this.password);
      updateHidden('#detailsInternetPage .shared-network', !this.shared);
      updateHidden('#detailsInternetPage .prefer-network',
                   !this.showPreferred);
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
   * Whether the underlying network shared wifi. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(DetailsInternetPage, 'shared',
      cr.PropertyKind.JS,
      DetailsInternetPage.prototype.updateControls_);

  /**
   * Whether the underlying network is a vpn. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(DetailsInternetPage, 'vpn',
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
