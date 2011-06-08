// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;
  const ArrayDataModel = cr.ui.ArrayDataModel;

  /////////////////////////////////////////////////////////////////////////////
  // InternetOptions class:

  /**
   * Encapsulated handling of ChromeOS internet options page.
   * @constructor
   */
  function InternetOptions() {
    OptionsPage.call(this, 'internet', templateData.internetPageTabTitle,
                     'internetPage');
  }

  cr.addSingletonGetter(InternetOptions);

  // Inherit InternetOptions from OptionsPage.
  InternetOptions.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initializes InternetOptions page.
     * Calls base class implementation to starts preference initialization.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      if (templateData.accessLocked) {
        this.accesslocked = true;
      }

      options.internet.NetworkElement.decorate($('wired-list'));
      $('wired-list').load(templateData.wiredList);
      options.internet.NetworkElement.decorate($('wireless-list'));
      $('wireless-list').load(templateData.wirelessList);
      options.internet.NetworkElement.decorate($('vpn-list'));
      $('vpn-list').load(templateData.vpnList);
      options.internet.NetworkElement.decorate($('remembered-list'));
      $('remembered-list').load(templateData.rememberedList);

      options.internet.CellularPlanElement.decorate($('planList'));

      $('wired-section').hidden = (templateData.wiredList.length == 0);
      $('wireless-section').hidden = (templateData.wirelessList.length == 0);
      $('vpn-section').hidden = (templateData.vpnList.length == 0);
      $('remembered-section').hidden =
          (templateData.rememberedList.length == 0);
      InternetOptions.setupAttributes(templateData);
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
      $('enable-wifi').addEventListener('click', function(event) {
        event.target.disabled = true;
        chrome.send('enableWifi', []);
      });
      $('disable-wifi').addEventListener('click', function(event) {
        event.target.disabled = true;
        chrome.send('disableWifi', []);
      });
      $('enable-cellular').addEventListener('click', function(event) {
        event.target.disabled = true;
        chrome.send('enableCellular', []);
      });
      $('disable-cellular').addEventListener('click', function(event) {
        event.target.disabled = true;
        chrome.send('disableCellular', []);
      });
      $('buyplanDetails').addEventListener('click', function(event) {
        chrome.send('buyDataPlan', []);
        OptionsPage.closeOverlay();
      });
      $('cellularApnClear').addEventListener('click', function(event) {
        $('cellularApn').value = "";
        $('cellularApnUsername').value = "";
        $('cellularApnPassword').value = "";
        var data = $('connectionState').data;
        chrome.send('setApn', [String(data.servicePath),
                               String($('cellularApn').value),
                               String($('cellularApnUsername').value),
                               String($('cellularApnPassword').value)]);
      });
      $('cellularApnSet').addEventListener('click', function(event) {
        var data = $('connectionState').data;
        chrome.send('setApn', [String(data.servicePath),
                               String($('cellularApn').value),
                               String($('cellularApnUsername').value),
                               String($('cellularApnPassword').value)]);
      });
      $('sim-card-lock-enabled').addEventListener('click', function(event) {
        var newValue = $('sim-card-lock-enabled').checked;
        // Leave value as is because user needs to enter PIN code first.
        // When PIN will be entered and value changed,
        // we'll update UI to reflect that change.
        $('sim-card-lock-enabled').checked = !newValue;
        InternetOptions.enableSecurityTab(false);
        chrome.send('setSimCardLock', [newValue]);
      });
      $('change-pin').addEventListener('click', function(event) {
        chrome.send('changePin');
      });
      this.showNetworkDetails_();
    },

    showNetworkDetails_: function() {
      var params = parseQueryParams(window.location);
      var servicePath = params.servicePath;
      var networkType = params.networkType;
      if (!servicePath || !servicePath.length ||
          !networkType || !networkType.length)
        return;
      chrome.send('buttonClickCallback',
          [networkType, servicePath, "options"]);
    },

    /**
     * Update internet page controls.
     * @private
     */
    updateControls_: function() {
      accesslocked = this.accesslocked;

      $('locked-network-banner').hidden = !accesslocked;
      $('wireless-buttons').hidden = accesslocked;
      $('wired-section').hidden = accesslocked;
      $('wireless-section').hidden = accesslocked;
      $('vpn-section').hidden = accesslocked;
      $('remembered-section').hidden = accesslocked;

      // Don't change hidden attribute on OptionsPage divs directly beucase it
      // is used in supporting infrasture now.
      if (accesslocked && DetailsInternetPage.getInstance().visible)
        this.closeOverlay();
    }
  };

  /**
   * Whether access to this page is locked.
   * @type {boolean}
   */
  cr.defineProperty(InternetOptions, 'accesslocked', cr.PropertyKind.JS,
      InternetOptions.prototype.updateControls_);

  InternetOptions.loginFromDetails = function () {
    var data = $('connectionState').data;
    var servicePath = data.servicePath;
    chrome.send('buttonClickCallback', [String(data.type),
                                        servicePath,
                                        'connect']);
    OptionsPage.closeOverlay();
  };

  InternetOptions.disconnectNetwork = function () {
    var data = $('connectionState').data;
    var servicePath = data.servicePath;
    chrome.send('buttonClickCallback', [String(data.type),
                                        servicePath,
                                        'disconnect']);
    OptionsPage.closeOverlay();
  };

  InternetOptions.activateFromDetails = function () {
    var data = $('connectionState').data;
    var servicePath = data.servicePath;
    if (data.type == options.internet.Constants.TYPE_CELLULAR) {
      chrome.send('buttonClickCallback', [String(data.type),
                                          String(servicePath),
                                          'activate']);
    }
    OptionsPage.closeOverlay();
  };

  InternetOptions.setDetails = function () {
    var data = $('connectionState').data;
    var servicePath = data.servicePath;
    if (data.type == options.internet.Constants.TYPE_WIFI) {
      chrome.send('setAutoConnect',
                  [String(servicePath),
                   $('autoConnectNetworkWifi').checked ? "true" : "false"]);
    } else if (data.type == options.internet.Constants.TYPE_CELLULAR) {
      chrome.send('setAutoConnect',
                  [String(servicePath),
                   $('autoConnectNetworkCellular').checked ? "true" : "false"]);
    }
    chrome.send('setShared',[String(servicePath),
         $('sharedNetwork').checked ? "true" : "false"]);

    var ipConfigList = $('ipConfigList');
    chrome.send('setIPConfig',[String(servicePath),
                               $('ipTypeDHCP').checked ? "true" : "false",
                               ipConfigList.dataModel.item(0).value,
                               ipConfigList.dataModel.item(1).value,
                               ipConfigList.dataModel.item(2).value,
                               ipConfigList.dataModel.item(3).value]);
    OptionsPage.closeOverlay();
  };

  InternetOptions.enableSecurityTab = function(enabled) {
    $('sim-card-lock-enabled').disabled = !enabled;
    $('change-pin').disabled = !enabled;
  };

  InternetOptions.setupAttributes = function(data) {
    var buttons = $('wireless-buttons');
    if (data.wifiEnabled) {
      $('disable-wifi').disabled = false;
      $('disable-wifi').hidden = false;
      $('enable-wifi').hidden = true;
    } else {
      $('enable-wifi').disabled = false;
      $('enable-wifi').hidden = false;
      $('disable-wifi').hidden = true;
    }
    if (data.cellularAvailable) {
      if (data.cellularEnabled) {
        $('disable-cellular').disabled = false;
        $('disable-cellular').hidden = false;
        $('enable-cellular').hidden = true;
      } else {
        $('enable-cellular').disabled = false;
        $('enable-cellular').hidden = false;
        $('disable-cellular').hidden = true;
      }
      if (!AccountsOptions.currentUserIsOwner())
        $('internet-owner-only-warning').hidden = false;
      $('data-roaming').hidden = false;
    } else {
      $('enable-cellular').hidden = true;
      $('disable-cellular').hidden = true;
      $('data-roaming').hidden = true;
    }
  };

  //
  //Chrome callbacks
  //
  InternetOptions.refreshNetworkData = function (data) {
    var self = InternetOptions.getInstance();
    if (data.accessLocked) {
      self.accesslocked = true;
      return;
    }
    self.accesslocked = false;
    $('wired-list').load(data.wiredList);
    $('wireless-list').load(data.wirelessList);
    $('vpn-list').load(data.vpnList);
    $('remembered-list').load(data.rememberedList);

    $('wired-section').hidden = (data.wiredList.length == 0);
    $('wireless-section').hidden = (data.wirelessList.length == 0);
    $('vpn-section').hidden = (data.vpnList.length == 0);
    InternetOptions.setupAttributes(data);
    $('remembered-section').hidden = (data.rememberedList.length == 0);
  };

  // TODO(xiyuan): This function seems belonging to DetailsInternetPage.
  InternetOptions.updateCellularPlans = function (data) {
    var detailsPage = DetailsInternetPage.getInstance();
    detailsPage.cellplanloading = false;
    if (data.plans && data.plans.length) {
      detailsPage.nocellplan = false
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
  };

  InternetOptions.updateSecurityTab = function(data) {
    InternetOptions.enableSecurityTab(true);
    $('sim-card-lock-enabled').checked = data.requirePin;
  };

  InternetOptions.showDetailedInfo = function (data) {
    var detailsPage = DetailsInternetPage.getInstance();
    // TODO(chocobo): Is this hack to cache the data here reasonable?
    $('connectionState').data = data;
    $('buyplanDetails').hidden = true;
    $('activateDetails').hidden = true;
    $('detailsInternetLogin').hidden = data.connected;
    if (data.type == options.internet.Constants.TYPE_ETHERNET)
      $('detailsInternetDisconnect').hidden = true;
    else
      $('detailsInternetDisconnect').hidden = !data.connected;

    detailsPage.deviceConnected = data.deviceConnected;
    detailsPage.connecting = data.connecting;
    detailsPage.connected = data.connected;
    if (data.connected) {
      $('inetTitle').textContent = localStrings.getString('inetStatus');
    } else {
      $('inetTitle').textContent = localStrings.getString('inetConnect');
    }
    $('connectionState').textContent = data.connectionState;

    var inetAddress = '';
    var inetSubnetAddress = '';
    var inetGateway = '';
    var inetDns = '';
    $('ipTypeDHCP').checked = true;
    if (data.ipconfigStatic) {
      inetAddress = data.ipconfigStatic.address;
      inetSubnetAddress = data.ipconfigStatic.subnetAddress;
      inetGateway = data.ipconfigStatic.gateway;
      inetDns = data.ipconfigStatic.dns;
      $('ipTypeStatic').checked = true;
    } else if (data.ipconfigDHCP) {
      inetAddress = data.ipconfigDHCP.address;
      inetSubnetAddress = data.ipconfigDHCP.subnetAddress;
      inetGateway = data.ipconfigDHCP.gateway;
      inetDns = data.ipconfigDHCP.dns;
    }

    // Hide the dhcp/static radio if not ethernet or wifi
    var ethernet_wifi = data.type == options.internet.Constants.TYPE_WIFI ||
                        data.type == options.internet.Constants.TYPE_ETHERNET;
    $('ipTypeDHCPDiv').hidden = !ethernet_wifi;
    $('ipTypeStaticDiv').hidden = !ethernet_wifi;

    var ipConfigList = $('ipConfigList');
    ipConfigList.disabled = $('ipTypeDHCP').checked || !ethernet_wifi;
    options.internet.IPConfigList.decorate(ipConfigList);
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
      // disable ipConfigList and switch back to dhcp values (if any)
      if (data.ipconfigDHCP) {
        ipConfigList.dataModel.item(0).value = data.ipconfigDHCP.address;
        ipConfigList.dataModel.item(1).value = data.ipconfigDHCP.subnetAddress;
        ipConfigList.dataModel.item(2).value = data.ipconfigDHCP.gateway;
        ipConfigList.dataModel.item(3).value = data.ipconfigDHCP.dns;
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
      $('inetSsid').textContent = data.ssid;
      $('autoConnectNetworkWifi').checked = data.autoConnect;
      $('autoConnectNetworkWifi').disabled = !data.remembered;
      $('sharedNetwork').checked = data.shared;
      $('sharedNetwork').disabled = !data.remembered || !data.shareable;
      detailsPage.password = data.encrypted;
    } else if(data.type == options.internet.Constants.TYPE_CELLULAR) {
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
          a.target = "_blank";
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
        $('cellularApn').value = data.apn;
        $('cellularApnUsername').value = data.apn_username;
        $('cellularApnPassword').value = data.apn_password;
        $('sim-card-lock-enabled').checked = data.simCardLockEnabled;
        InternetOptions.enableSecurityTab(true);
      }
      $('autoConnectNetworkCellular').checked = data.autoConnect;
      $('autoConnectNetworkCellular').disabled = false;

      $('buyplanDetails').hidden = !data.showBuyButton;
      $('activateDetails').hidden = !data.showActivateButton;
      if (data.showActivateButton) {
        $('detailsInternetLogin').hidden = true;
      }

      detailsPage.hascellplan = false;
      if (data.connected) {
        detailsPage.nocellplan = false;
        detailsPage.cellplanloading = true;
        chrome.send('refreshCellularPlan', [data.servicePath])
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
    // Don't show page name in address bar and in history to prevent people
    // navigate here by hand and solve issue with page session restore.
    OptionsPage.showPageByName('detailsInternetPage', false);
  };

  // Export
  return {
    InternetOptions: InternetOptions
  };
});
