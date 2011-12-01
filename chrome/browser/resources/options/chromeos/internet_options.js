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

      this.updatePolicyIndicatorVisibility_();

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
      $('change-proxy-button').addEventListener('click', function(event) {
        OptionsPage.closeOverlay();
        OptionsPage.showPageByName('proxy', false);
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowProxySettings']);
      });
      $('upload-network-settings').addEventListener('change', function(event) {
        var file = event.target.files[0];
        var reader = new FileReader();
        reader.onloadend = function(e) {
          chrome.send('importNetworkSettings', [this.result]);
        };
        reader.readAsText(file);
      }, false);
      $('buyplanDetails').addEventListener('click', function(event) {
        chrome.send('buyDataPlan', []);
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

        InternetOptions.prototype.updateHidden_(
          cr.doc.querySelectorAll('.apn-list-view'),
          false);
        InternetOptions.prototype.updateHidden_(
          cr.doc.querySelectorAll('.apn-details-view'),
          true);
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
        data.userApnIndex = apnSelector.length - 2
        data.selectedApn = data.userApnIndex;

        InternetOptions.prototype.updateHidden_(
          cr.doc.querySelectorAll('.apn-list-view'),
          false);
        InternetOptions.prototype.updateHidden_(
          cr.doc.querySelectorAll('.apn-details-view'),
          true);
      });
      $('cellularApnCancel').addEventListener('click', function(event) {
        $('selectApn').selectedIndex = $('connectionState').data.selectedApn;

        InternetOptions.prototype.updateHidden_(
          cr.doc.querySelectorAll('.apn-list-view'),
          false);
        InternetOptions.prototype.updateHidden_(
          cr.doc.querySelectorAll('.apn-details-view'),
          true);
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

          InternetOptions.prototype.updateHidden_(
            cr.doc.querySelectorAll('.apn-list-view'),
            true);
          InternetOptions.prototype.updateHidden_(
            cr.doc.querySelectorAll('.apn-details-view'),
            false);
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
      this.showNetworkDetails_();
    },

    showNetworkDetails_: function() {
      var params = parseQueryParams(window.location);
      var servicePath = params.servicePath;
      var networkType = params.networkType;
      if (!servicePath || !servicePath.length ||
          !networkType || !networkType.length)
        return;
      var networkName = params.networkName;
      if (networkName)
        options.ProxyOptions.getInstance().setNetworkName(networkName);
      chrome.send('buttonClickCallback',
          [networkType, servicePath, "options"]);
    },

    updateHidden_: function(elements, hidden) {
      for (var i = 0, el; el = elements[i]; i++) {
        el.hidden = hidden;
      }
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

      // Don't change hidden attribute on OptionsPage divs directly because it
      // is used in supporting infrastructure now.
      if (accesslocked && DetailsInternetPage.getInstance().visible)
        this.closeOverlay();
    },

    /**
     * Updates the policy indicator visibility. Space is only allocated for the
     * policy indicators if there is at least one visible.
     * @private
     */
    updatePolicyIndicatorVisibility_: function() {
      var page = $('internetPage');
      if (page.querySelectorAll(
              '.network-item > .controlled-setting-indicator[controlled-by]')
              .length) {
        page.classList.remove('hide-indicators');
      } else {
        page.classList.add('hide-indicators');
      }
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
      chrome.send('setPreferNetwork',
                   [String(servicePath),
                    $('preferNetworkWifi').checked ? "true" : "false"]);
      chrome.send('setAutoConnect',
                  [String(servicePath),
                   $('autoConnectNetworkWifi').checked ? "true" : "false"]);
    } else if (data.type == options.internet.Constants.TYPE_CELLULAR) {
      chrome.send('setAutoConnect',
                  [String(servicePath),
                   $('autoConnectNetworkCellular').checked ? "true" : "false"]);
    }

    var ipConfigList = $('ipConfigList');
    chrome.send('setIPConfig',[String(servicePath),
                               $('ipTypeDHCP').checked ? "true" : "false",
                               ipConfigList.dataModel.item(0).value,
                               ipConfigList.dataModel.item(1).value,
                               ipConfigList.dataModel.item(2).value,
                               ipConfigList.dataModel.item(3).value]);
    OptionsPage.closeOverlay();
  };

  InternetOptions.setupAttributes = function(data) {
    var buttons = $('wireless-buttons');
    if (data.wifiEnabled) {
      $('disable-wifi').disabled = data.wifiBusy;
      $('disable-wifi').hidden = false;
      $('enable-wifi').hidden = true;
    } else {
      $('enable-wifi').disabled = data.wifiBusy;
      $('enable-wifi').hidden = false;
      $('disable-wifi').hidden = true;
    }
    if (data.cellularAvailable) {
      if (data.cellularEnabled) {
        $('disable-cellular').disabled = data.cellularBusy;
        $('disable-cellular').hidden = false;
        $('enable-cellular').hidden = true;
      } else {
        $('enable-cellular').disabled = data.cellularBusy;
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

    self.updatePolicyIndicatorVisibility_();

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

  InternetOptions.updateSecurityTab = function(requirePin) {
    $('sim-card-lock-enabled').checked = requirePin;
    $('change-pin').hidden = !requirePin;
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

    // Hide change-proxy-button and change-proxy-section if not showing proxy.
    $('change-proxy-button').hidden = !data.showProxy;
    $('change-proxy-section').hidden = !data.showProxy;

    var ipConfigList = $('ipConfigList');
    ipConfigList.disabled =
        $('ipTypeDHCP').checked || data.ipconfigStatic.controlledBy ||
        !data.showStaticIPConfig;
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
        InternetOptions.prototype.updateHidden_(
          cr.doc.querySelectorAll('.apn-list-view'),
          false);
        InternetOptions.prototype.updateHidden_(
          cr.doc.querySelectorAll('.apn-details-view'),
          true);

        InternetOptions.updateSecurityTab(data.simCardLockEnabled.value);
      }
      $('autoConnectNetworkCellular').checked = data.autoConnect.value;
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

    // Don't show page name in address bar and in history to prevent people
    // navigate here by hand and solve issue with page session restore.
    OptionsPage.showPageByName('detailsInternetPage', false);
  };

  InternetOptions.invalidNetworkSettings = function () {
    alert(localStrings.getString('invalidNetworkSettings'));
  };

  // Export
  return {
    InternetOptions: InternetOptions
  };
});
