// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.internet', function() {
  var OptionsPage = options.OptionsPage;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

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
      this.initializePageContents_();
      this.showNetworkDetails_();
    },

    /**
     * Auto-activates the network details dialog if network information
     * is included in the URL.
     */
    showNetworkDetails_: function() {
      var params = parseQueryParams(window.location);
      var servicePath = params.servicePath;
      var networkType = params.networkType;
      if (!servicePath || !servicePath.length ||
          !networkType || !networkType.length)
        return;
      chrome.send('buttonClickCallback',
          [networkType, servicePath, 'options']);
    },

    /**
     * Initializes the contents of the page.
     */
    initializePageContents_: function() {
      $('detailsInternetDismiss').addEventListener('click', function(event) {
        DetailsInternetPage.setDetails();
      });

      $('detailsInternetLogin').addEventListener('click', function(event) {
        DetailsInternetPage.setDetails();
        DetailsInternetPage.loginFromDetails();
      });

      $('detailsInternetDisconnect').addEventListener('click', function(event) {
        DetailsInternetPage.setDetails();
        DetailsInternetPage.disconnectNetwork();
      });

      $('activateDetails').addEventListener('click', function(event) {
        DetailsInternetPage.activateFromDetails();
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
      if (!controlledBy || controlledBy == '') {
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

  /**
   * Performs minimal initialization of the InternetDetails dialog in
   * preparation for showing proxy-setttings.
   */
  DetailsInternetPage.initializeProxySettings = function() {
    var detailsPage = DetailsInternetPage.getInstance();
    detailsPage.initializePageContents_();
  };

  /**
   * Displays the InternetDetails dialog with only the proxy settings visible.
   */
  DetailsInternetPage.showProxySettings = function() {
    var detailsPage = DetailsInternetPage.getInstance();
    $('network-details-header').hidden = true;
    $('buyplanDetails').hidden = true;
    $('activateDetails').hidden = true;
    $('viewAccountDetails').hidden = true;
    detailsPage.cellular = false;
    detailsPage.wireless = false;
    detailsPage.vpn = false;
    detailsPage.showProxy = true;
    updateHidden('#internetTab', true);
    updateHidden('#details-tab-strip', true);
    updateHidden('#detailsInternetPage .action-area', true);
    detailsPage.updateControls();
    detailsPage.visible = true;
  };

  DetailsInternetPage.updateCellularPlans = function(data) {
    var detailsPage = DetailsInternetPage.getInstance();
    detailsPage.cellplanloading = false;
    if (data.plans && data.plans.length) {
      detailsPage.nocellplan = false;
      detailsPage.hascellplan = true;
      $('planList').load(data.plans);
    } else {
      detailsPage.nocellplan = true;
      detailsPage.hascellplan = false;
    }

    detailsPage.hasactiveplan = !data.needsPlan;
    detailsPage.activated = data.activated;
    if (!data.activated)
      $('detailsInternetLogin').hidden = true;

    $('buyplanDetails').hidden = !data.showBuyButton;
    $('activateDetails').hidden = !data.showActivateButton;
    $('viewAccountDetails').hidden = !data.showViewAccountButton;
  };

  DetailsInternetPage.updateSecurityTab = function(requirePin) {
    $('sim-card-lock-enabled').checked = requirePin;
    $('change-pin').hidden = !requirePin;
  };


  DetailsInternetPage.loginFromDetails = function() {
    var data = $('connectionState').data;
    var servicePath = data.servicePath;
    chrome.send('buttonClickCallback', [String(data.type),
                                        servicePath,
                                        'connect']);
    OptionsPage.closeOverlay();
  };

  DetailsInternetPage.disconnectNetwork = function() {
    var data = $('connectionState').data;
    var servicePath = data.servicePath;
    chrome.send('buttonClickCallback', [String(data.type),
                                        servicePath,
                                        'disconnect']);
    OptionsPage.closeOverlay();
  };

  DetailsInternetPage.activateFromDetails = function() {
    var data = $('connectionState').data;
    var servicePath = data.servicePath;
    if (data.type == options.internet.Constants.TYPE_CELLULAR) {
      chrome.send('buttonClickCallback', [String(data.type),
                                          String(servicePath),
                                          'activate']);
    }
    OptionsPage.closeOverlay();
  };

  DetailsInternetPage.setDetails = function() {
    var data = $('connectionState').data;
    var servicePath = data.servicePath;
    if (data.type == options.internet.Constants.TYPE_WIFI) {
      chrome.send('setPreferNetwork',
                   [String(servicePath),
                    $('preferNetworkWifi').checked ? 'true' : 'false']);
      chrome.send('setAutoConnect',
                  [String(servicePath),
                   $('autoConnectNetworkWifi').checked ? 'true' : 'false']);
    } else if (data.type == options.internet.Constants.TYPE_CELLULAR) {
      chrome.send('setAutoConnect',
                  [String(servicePath),
                   $('autoConnectNetworkCellular').checked ? 'true' : 'false']);
    }
    var ipConfigList = $('ipConfigList');
    chrome.send('setIPConfig', [String(servicePath),
                                $('ipTypeDHCP').checked ? 'true' : 'false',
                                ipConfigList.dataModel.item(0).value,
                                ipConfigList.dataModel.item(1).value,
                                ipConfigList.dataModel.item(2).value,
                                ipConfigList.dataModel.item(3).value]);
    OptionsPage.closeOverlay();
  };

  DetailsInternetPage.showDetailedInfo = function(data) {
    var detailsPage = DetailsInternetPage.getInstance();

    // Populate header
    $('network-details-title').textContent = data.networkName;
    var statusKey = data.connected ? 'networkConnected' :
                                     'networkNotConnected';
    $('network-details-subtitle-status').textContent =
        localStrings.getString(statusKey);
    var typeKey = null;
    var Constants = options.internet.Constants;
    switch (data.type) {
    case Constants.TYPE_ETHERNET:
      typeKey = 'ethernetTitle';
      break;
    case Constants.TYPE_WIFI:
      typeKey = 'wifiTitle';
      break;
    case Constants.TYPE_CELLULAR:
      typeKey = 'cellularTitle';
      break;
    case Constants.TYPE_VPN:
      typeKey = 'vpnTitle';
      break;
    }
    var typeLabel = $('network-details-subtitle-type');
    var typeSeparator = $('network-details-subtitle-separator');
    if (typeKey) {
      typeLabel.textContent = localStrings.getString(typeKey);
      typeLabel.hidden = false;
      typeSeparator.hidden = false;
    } else {
      typeLabel.hidden = true;
      typeSeparator.hidden = true;
    }

    // TODO(chocobo): Is this hack to cache the data here reasonable?
    $('connectionState').data = data;

    $('buyplanDetails').hidden = true;
    $('activateDetails').hidden = true;
    $('viewAccountDetails').hidden = true;
    $('detailsInternetLogin').hidden = data.connected;
    if (data.type == options.internet.Constants.TYPE_ETHERNET)
      $('detailsInternetDisconnect').hidden = true;
    else
      $('detailsInternetDisconnect').hidden = !data.connected;

    detailsPage.deviceConnected = data.deviceConnected;
    detailsPage.connecting = data.connecting;
    detailsPage.connected = data.connected;
    detailsPage.showProxy = data.showProxy;
    $('connectionState').textContent = data.connectionState;

    var inetAddress = '';
    var inetSubnetAddress = '';
    var inetGateway = '';
    var inetDns = '';
    $('ipTypeDHCP').checked = true;
    if (data.ipconfigStatic.value) {
      inetAddress = data.ipconfigStatic.value.address;
      inetSubnetAddress = data.ipconfigStatic.value.subnetAddress;
      inetGateway = data.ipconfigStatic.value.gateway;
      inetDns = data.ipconfigStatic.value.dns;
      $('ipTypeStatic').checked = true;
    } else if (data.ipconfigDHCP.value) {
      inetAddress = data.ipconfigDHCP.value.address;
      inetSubnetAddress = data.ipconfigDHCP.value.subnetAddress;
      inetGateway = data.ipconfigDHCP.value.gateway;
      inetDns = data.ipconfigDHCP.value.dns;
    }

    // Hide the dhcp/static radio if needed.
    $('ipTypeDHCPDiv').hidden = !data.showStaticIPConfig;
    $('ipTypeStaticDiv').hidden = !data.showStaticIPConfig;

    var ipConfigList = $('ipConfigList');
    options.internet.IPConfigList.decorate(ipConfigList);
    ipConfigList.disabled =
        $('ipTypeDHCP').checked || data.ipconfigStatic.controlledBy ||
        !data.showStaticIPConfig;
    ipConfigList.autoExpands = true;
    var model = new ArrayDataModel([]);
    model.push({
      'property': 'inetAddress',
      'name': localStrings.getString('inetAddress'),
      'value': inetAddress,
    });
    model.push({
      'property': 'inetSubnetAddress',
      'name': localStrings.getString('inetSubnetAddress'),
      'value': inetSubnetAddress,
    });
    model.push({
      'property': 'inetGateway',
      'name': localStrings.getString('inetGateway'),
      'value': inetGateway,
    });
    model.push({
      'property': 'inetDns',
      'name': localStrings.getString('inetDns'),
      'value': inetDns,
    });
    ipConfigList.dataModel = model;

    $('ipTypeDHCP').addEventListener('click', function(event) {
      // Disable ipConfigList and switch back to dhcp values (if any).
      if (data.ipconfigDHCP.value) {
        var config = data.ipconfigDHCP.value;
        ipConfigList.dataModel.item(0).value = config.address;
        ipConfigList.dataModel.item(1).value = config.subnetAddress;
        ipConfigList.dataModel.item(2).value = config.gateway;
        ipConfigList.dataModel.item(3).value = config.dns;
      }
      ipConfigList.dataModel.updateIndex(0);
      ipConfigList.dataModel.updateIndex(1);
      ipConfigList.dataModel.updateIndex(2);
      ipConfigList.dataModel.updateIndex(3);
      // Unselect all so we don't keep the currently selected field editable.
      ipConfigList.selectionModel.unselectAll();
      ipConfigList.disabled = true;
    });

    $('ipTypeStatic').addEventListener('click', function(event) {
      // enable ipConfigList
      ipConfigList.disabled = false;
      ipConfigList.focus();
      ipConfigList.selectionModel.selectedIndex = 0;
    });

    if (data.hardwareAddress) {
      $('hardwareAddress').textContent = data.hardwareAddress;
      $('hardwareAddressRow').style.display = 'table-row';
    } else {
      // This is most likely a device without a hardware address.
      $('hardwareAddressRow').style.display = 'none';
    }
    if (data.type == options.internet.Constants.TYPE_WIFI) {
      OptionsPage.showTab($('wifiNetworkNavTab'));
      detailsPage.wireless = true;
      detailsPage.vpn = false;
      detailsPage.ethernet = false;
      detailsPage.cellular = false;
      detailsPage.gsm = false;
      detailsPage.shared = data.shared;
      $('inetSsid').textContent = data.ssid;
      detailsPage.showPreferred = data.showPreferred;
      $('preferNetworkWifi').checked = data.preferred.value;
      $('preferNetworkWifi').disabled = !data.remembered;
      $('autoConnectNetworkWifi').checked = data.autoConnect.value;
      $('autoConnectNetworkWifi').disabled = !data.remembered;
      detailsPage.password = data.encrypted;
    } else if (data.type == options.internet.Constants.TYPE_CELLULAR) {
      if (!data.gsm)
        OptionsPage.showTab($('cellularPlanNavTab'));
      else
        OptionsPage.showTab($('cellularConnNavTab'));
      detailsPage.ethernet = false;
      detailsPage.wireless = false;
      detailsPage.vpn = false;
      detailsPage.cellular = true;
      if (data.carrierUrl) {
        var a = $('carrierUrl');
        if (!a) {
          a = document.createElement('a');
          $('serviceName').appendChild(a);
          a.id = 'carrierUrl';
          a.target = '_blank';
        }
        a.href = data.carrierUrl;
        a.textContent = data.serviceName;
      } else {
        $('serviceName').textContent = data.serviceName;
      }
      $('networkTechnology').textContent = data.networkTechnology;
      $('activationState').textContent = data.activationState;
      $('roamingState').textContent = data.roamingState;
      $('restrictedPool').textContent = data.restrictedPool;
      $('errorState').textContent = data.errorState;
      $('manufacturer').textContent = data.manufacturer;
      $('modelId').textContent = data.modelId;
      $('firmwareRevision').textContent = data.firmwareRevision;
      $('hardwareRevision').textContent = data.hardwareRevision;
      $('prlVersion').textContent = data.prlVersion;
      $('meid').textContent = data.meid;
      $('imei').textContent = data.imei;
      $('mdn').textContent = data.mdn;
      $('esn').textContent = data.esn;
      $('min').textContent = data.min;
      detailsPage.gsm = data.gsm;
      if (data.gsm) {
        $('operatorName').textContent = data.operatorName;
        $('operatorCode').textContent = data.operatorCode;
        $('imsi').textContent = data.imsi;

        var apnSelector = $('selectApn');
        // Clear APN lists, keep only last element that "other".
        while (apnSelector.length != 1)
          apnSelector.remove(0);
        var otherOption = apnSelector[0];
        data.selectedApn = -1;
        data.userApnIndex = -1;
        var apnList = data.providerApnList.value;
        for (var i = 0; i < apnList.length; i++) {
          var option = document.createElement('option');
          var name = apnList[i].localizedName;
          if (name == '' && apnList[i].name != '')
            name = apnList[i].name;
          if (name == '')
            name = apnList[i].apn;
          else
            name = name + ' (' + apnList[i].apn + ')';
          option.textContent = name;
          option.value = i;
          if ((data.apn.apn == apnList[i].apn &&
               data.apn.username == apnList[i].username &&
               data.apn.password == apnList[i].password) ||
              (data.apn.apn == '' &&
               data.lastGoodApn.apn == apnList[i].apn &&
               data.lastGoodApn.username == apnList[i].username &&
               data.lastGoodApn.password == apnList[i].password)) {
            data.selectedApn = i;
          }
          // Insert new option before "other" option.
          apnSelector.add(option, otherOption);
        }
        if (data.selectedApn == -1 && data.apn.apn != '') {
          var option = document.createElement('option');
          option.textContent = data.apn.apn;
          option.value = -1;
          apnSelector.add(option, otherOption);
          data.selectedApn = apnSelector.length - 2;
          data.userApnIndex = data.selectedApn;
        }
        apnSelector.selectedIndex = data.selectedApn;
        updateHidden('.apn-list-view', false);
        updateHidden('.apn-details-view', true);
        DetailsInternetPage.updateSecurityTab(data.simCardLockEnabled.value);
      }
      $('autoConnectNetworkCellular').checked = data.autoConnect.value;
      $('autoConnectNetworkCellular').disabled = false;

      $('buyplanDetails').hidden = !data.showBuyButton;
      $('viewAccountDetails').hidden = !data.showViewAccountButton;
      $('activateDetails').hidden = !data.showActivateButton;
      if (data.showActivateButton) {
        $('detailsInternetLogin').hidden = true;
      }

      detailsPage.hascellplan = false;
      if (data.connected) {
        detailsPage.nocellplan = false;
        detailsPage.cellplanloading = true;
        chrome.send('refreshCellularPlan', [data.servicePath]);
      } else {
        detailsPage.nocellplan = true;
        detailsPage.cellplanloading = false;
      }
    } else if (data.type == options.internet.Constants.TYPE_VPN) {
      OptionsPage.showTab($('vpnNavTab'));
      detailsPage.wireless = false;
      detailsPage.vpn = true;
      detailsPage.ethernet = false;
      detailsPage.cellular = false;
      detailsPage.gsm = false;
      $('inetServiceName').textContent = data.service_name;
      $('inetServerHostname').textContent = data.server_hostname;
      $('inetProviderType').textContent = data.provider_type;
      $('inetUsername').textContent = data.username;
    } else {
      OptionsPage.showTab($('internetNavTab'));
      detailsPage.ethernet = true;
      detailsPage.wireless = false;
      detailsPage.vpn = false;
      detailsPage.cellular = false;
      detailsPage.gsm = false;
    }

    // Update controlled option indicators.
    indicators = cr.doc.querySelectorAll(
        '#detailsInternetPage .controlled-setting-indicator');
    for (var i = 0; i < indicators.length; i++) {
      var dataProperty = indicators[i].getAttribute('data');
      if (dataProperty && data[dataProperty]) {
        var controlledBy = data[dataProperty].controlledBy;
        if (controlledBy) {
          indicators[i].controlledBy = controlledBy;
          var forElement = $(indicators[i].getAttribute('for'));
          if (forElement)
            forElement.disabled = true;
          if (forElement.type == 'radio' && !forElement.checked)
            indicators[i].hidden = true;
        } else {
          indicators[i].controlledBy = null;
        }
      }
    }

    detailsPage.updateControls();

    // Don't show page name in address bar and in history to prevent people
    // navigate here by hand and solve issue with page session restore.
    OptionsPage.showPageByName('detailsInternetPage', false);
  };

  return {
    DetailsInternetPage: DetailsInternetPage
  };
});
