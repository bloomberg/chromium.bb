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

  /**
   * Monitor pref change of given element.
   * @param {Element} el Target element.
   */
  function observePrefsUI(el) {
    Preferences.getInstance().addEventListener(el.pref, handlePrefUpdate);
  }

  /**
   * UI pref change handler.
   * @param {Event} e The update event.
   */
  function handlePrefUpdate(e) {
    DetailsInternetPage.prototype.updateControls;
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
     * Indicates if the list of proxy exceptions has been initialized.
     * @type {boolean}
     */
    proxyListInitialized_: false,

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

      // Proxy
      options.proxyexceptions.ProxyExceptions.decorate($('ignoredHostList'));
      $('removeHost').addEventListener('click',
                                       this.handleRemoveProxyExceptions_);
      $('addHost').addEventListener('click', this.handleAddProxyException_);
      $('directProxy').addEventListener('click', this.disableManualProxy_);
      $('manualProxy').addEventListener('click', this.enableManualProxy_);
      $('autoProxy').addEventListener('click', this.disableManualProxy_);
      $('proxyAllProtocols').addEventListener('click',
                                              this.toggleSingleProxy_);
      observePrefsUI($('directProxy'));
      observePrefsUI($('manualProxy'));
      observePrefsUI($('autoProxy'));
      observePrefsUI($('proxyAllProtocols'));
    },

    /**
     * Handler for "add" event fired from userNameEdit.
     * @param {Event} e Add event fired from userNameEdit.
     * @private
     */
    handleAddProxyException_: function(e) {
      var exception = $('newHost').value;
      $('newHost').value = '';

      exception = exception.trim();
      if (exception)
        $('ignoredHostList').addException(exception);
    },

    /**
     * Handler for when the remove button is clicked
     * @param {Event} e The click event.
     * @private
     */
    handleRemoveProxyExceptions_: function(e) {
      var selectedItems = $('ignoredHostList').selectedItems;
      for (var x = 0; x < selectedItems.length; x++) {
        $('ignoredHostList').removeException(selectedItems[x]);
      }
    },

    /**
     * Update details page controls.
     * @private
     */
    updateControls: function() {
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

      // Proxy
      this.updateProxyBannerVisibility_();
      this.toggleSingleProxy_();
      if ($('manualProxy').checked)
        this.enableManualProxy_();
      else
        this.disableManualProxy_();
      if (!this.proxyListInitialized_ && this.visible) {
        this.proxyListInitialized_ = true;
        $('ignoredHostList').redraw();
      }
    },

    /**
     * Updates info banner visibility state. This function shows the banner
     * if proxy is managed or shared-proxies is off for shared network.
     * @private
     */
    updateProxyBannerVisibility_: function() {
      var bannerDiv = $('info-banner');
      // Show banner and determine its message if necessary.
      var controlledBy = $('directProxy').controlledBy;
      if (controlledBy == '') {
        bannerDiv.hidden = true;
      } else {
        bannerDiv.hidden = false;
        // controlledBy must match strings loaded in proxy_handler.cc and
        // set in proxy_cros_settings_provider.cc.
        $('banner-text').textContent = localStrings.getString(controlledBy);
      }
    },

    /**
     * Handler for when the user clicks on the checkbox to allow a
     * single proxy usage.
     * @private
     * @param {Event} e Click Event.
     */
    toggleSingleProxy_: function(e) {
      if ($('proxyAllProtocols').checked) {
        $('multiProxy').hidden = true;
        $('singleProxy').hidden = false;
      } else {
        $('multiProxy').hidden = false;
        $('singleProxy').hidden = true;
      }
    },

    /**
     * Handler for selecting a radio button that will disable the manual
     * controls.
     * @private
     * @param {Event} e Click event.
     */
    disableManualProxy_: function(e) {
      $('advancedConfig').hidden = true;
      $('proxyAllProtocols').disabled = true;
      $('proxyHostName').disabled = true;
      $('proxyHostPort').disabled = true;
      $('proxyHostSingleName').disabled = true;
      $('proxyHostSinglePort').disabled = true;
      $('secureProxyHostName').disabled = true;
      $('secureProxyPort').disabled = true;
      $('ftpProxy').disabled = true;
      $('ftpProxyPort').disabled = true;
      $('socksHost').disabled = true;
      $('socksPort').disabled = true;
      $('proxyConfig').disabled = $('autoProxy').disabled ||
                                  !$('autoProxy').checked;
    },

    /**
     * Handler for selecting a radio button that will enable the manual
     * controls.
     * @private
     * @param {Event} e Click event.
     */
    enableManualProxy_: function(e) {
      $('advancedConfig').hidden = false;
      $('ignoredHostList').redraw();
      var all_disabled = $('manualProxy').disabled;
      $('newHost').disabled = all_disabled;
      $('removeHost').disabled = all_disabled;
      $('addHost').disabled = all_disabled;
      $('proxyAllProtocols').disabled = all_disabled;
      $('proxyHostName').disabled = all_disabled;
      $('proxyHostPort').disabled = all_disabled;
      $('proxyHostSingleName').disabled = all_disabled;
      $('proxyHostSinglePort').disabled = all_disabled;
      $('secureProxyHostName').disabled = all_disabled;
      $('secureProxyPort').disabled = all_disabled;
      $('ftpProxy').disabled = all_disabled;
      $('ftpProxyPort').disabled = all_disabled;
      $('socksHost').disabled = all_disabled;
      $('socksPort').disabled = all_disabled;
      $('proxyConfig').disabled = true;
    },
  };

  return {
    DetailsInternetPage: DetailsInternetPage
  };
});
