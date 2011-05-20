// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

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
      options.internet.NetworkElement.decorate($('remembered-list'));
      $('remembered-list').load(templateData.rememberedList);

      options.internet.CellularPlanElement.decorate($('planList'));

      $('wired-section').hidden = (templateData.wiredList.length == 0);
      $('wireless-section').hidden = (templateData.wirelessList.length == 0);
      $('remembered-section').hidden =
          (templateData.rememberedList.length == 0);
      InternetOptions.setupAttributes(templateData);
      $('detailsInternetDismiss').addEventListener('click', function(event) {
        InternetOptions.setDetails();
      });
      $('detailsInternetLogin').addEventListener('click', function(event) {
        InternetOptions.loginFromDetails();
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
        var data = $('inetAddress').data;
        chrome.send('setApn', [String(data.servicePath),
                               String($('cellularApn').value),
                               String($('cellularApnUsername').value),
                               String($('cellularApnPassword').value)]);
      });
      $('cellularApnSet').addEventListener('click', function(event) {
        var data = $('inetAddress').data;
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
      $('remembered-section').hidden = accesslocked;
      $('detailsInternetPage').hidden = accesslocked;
    }
  };

  /**
   * Whether access to this page is locked.
   * @type {boolean}
   */
  cr.defineProperty(InternetOptions, 'accesslocked', cr.PropertyKind.JS,
      InternetOptions.prototype.updateControls_);

  // A boolean flag from InternerOptionsHandler to indicate whether to use
  // inline WebUI for ethernet/wifi login/options.
  InternetOptions.useSettingsUI = false;

  // Network status update will be blocked while typing in WEP password etc.
  InternetOptions.updateLocked = false;
  InternetOptions.updatePending = false;
  InternetOptions.updataData = null;

  InternetOptions.loginFromDetails = function () {
    var data = $('inetAddress').data;
    var servicePath = data.servicePath;
    if (data.type == options.internet.Constants.TYPE_WIFI) {
      if (data.certInPkcs) {
        chrome.send('loginToCertNetwork',[String(servicePath),
                                          String(data.certPath),
                                          String(data.ident)]);
      } else {
        chrome.send('loginToCertNetwork',[String(servicePath),
                                          String($('inetCert').value),
                                          String($('inetIdent').value),
                                          String($('inetCertPass').value)]);
      }
    } else if (data.type == options.internet.Constants.TYPE_CELLULAR) {
        chrome.send('buttonClickCallback', [String(data.type),
                                            servicePath,
                                            'connect']);
    }
    OptionsPage.closeOverlay();
  };

  InternetOptions.activateFromDetails = function () {
    var data = $('inetAddress').data;
    var servicePath = data.servicePath;
    if (data.type == options.internet.Constants.TYPE_CELLULAR) {
      chrome.send('buttonClickCallback', [String(data.type),
                                          String(servicePath),
                                          'activate']);
    }
    OptionsPage.closeOverlay();
  };

  InternetOptions.setDetails = function () {
    var data = $('inetAddress').data;
    var servicePath = data.servicePath;
    if (data.type == options.internet.Constants.TYPE_WIFI) {
      chrome.send('setDetails',[String(servicePath),
                                $('autoConnectNetwork').checked ?
                                  "true" : "false"]);
    }
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

    InternetOptions.useSettingsUI = data.networkUseSettingsUI;
  };

  // Prevent clobbering of password input field.
  InternetOptions.lockUpdates = function () {
    InternetOptions.updateLocked = true;
  };

  InternetOptions.unlockUpdates = function () {
    InternetOptions.updateLocked = false;
    if (InternetOptions.updatePending) {
      InternetOptions.refreshNetworkData(InternetOptions.updateData);
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
    if (InternetOptions.updateLocked) {
      InternetOptions.updateData = data;
      InternetOptions.updatePending = true;
    } else {
      $('wired-list').load(data.wiredList);
      $('wireless-list').load(data.wirelessList);
      $('remembered-list').load(data.rememberedList);

      $('wired-section').hidden = (data.wiredList.length == 0);
      $('wireless-section').hidden = (data.wirelessList.length == 0);
      InternetOptions.setupAttributes(data);
      $('remembered-section').hidden = (data.rememberedList.length == 0);
      InternetOptions.updateData = null;
      InternetOptions.updatePending = false;
    }
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

  InternetOptions.showPasswordEntry = function (data) {
    var element = $(data.servicePath);
    element.showPassword();
  };

  InternetOptions.showDetailedInfo = function (data) {
    var detailsPage = DetailsInternetPage.getInstance();
    $('buyplanDetails').hidden = true;
    $('activateDetails').hidden = true;
    $('detailsInternetLogin').hidden = data.connected;

    detailsPage.connecting = data.connecting;

    detailsPage.connected = data.connected;
    if (data.connected) {
      $('inetTitle').textContent = localStrings.getString('inetStatus');
    } else {
      $('inetTitle').textContent = localStrings.getString('inetConnect');
    }
    $('connectionState').textContent = data.connectionState;
    var address = $('inetAddress');
    address.data = data;
    if (data.ipconfigs && data.ipconfigs.length) {
      // We will be displaying only the first ipconfig info for now until we
      // start supporting multiple IP addresses per connection.
      address.textContent = data.ipconfigs[0].address;
      $('inetSubnetAddress').textContent = data.ipconfigs[0].subnetAddress;
      $('inetGateway').textContent = data.ipconfigs[0].gateway;
      $('inetDns').textContent = data.ipconfigs[0].dns;
    } else {
      // This is most likely a transient state due to device still connecting.
      address.textContent = '?';
      $('inetSubnetAddress').textContent = '?';
      $('inetGateway').textContent = '?';
      $('inetDns').textContent = '?';
    }
    if (data.hardwareAddress) {
      $('hardwareAddress').textContent = data.hardwareAddress;
      $('hardwareAddressRow').style.display = 'table-row';
    } else {
      // This is most likely a device without a hardware address.
      $('hardwareAddressRow').style.display = 'none';
    }
    if (data.type == 2) {
      OptionsPage.showTab($('wifiNetworkNavTab'));
      detailsPage.wireless = true;
      detailsPage.ethernet = false;
      detailsPage.cellular = false;
      detailsPage.gsm = false;
      $('inetSsid').textContent = data.ssid;
      $('autoConnectNetwork').checked = data.autoConnect;
      if (!AccountsOptions.currentUserIsOwner()) {
        // Disable this for guest non-Owners.
        $('autoConnectNetwork').disabled = true;
      }
      detailsPage.password = data.encrypted;
    } else if(data.type == 5) {
      if (!data.gsm)
        OptionsPage.showTab($('cellularPlanNavTab'));
      else
        OptionsPage.showTab($('cellularConnNavTab'));
      detailsPage.ethernet = false;
      detailsPage.wireless = false;
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
    } else {
      OptionsPage.showTab($('internetNavTab'));
      detailsPage.ethernet = true;
      detailsPage.wireless = false;
      detailsPage.cellular = false;
      detailsPage.gsm = false;
    }
    OptionsPage.navigateToPage('detailsInternetPage');
  };

  // Export
  return {
    InternetOptions: InternetOptions
  };
});
