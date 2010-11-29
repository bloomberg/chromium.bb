// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
    OptionsPage.call(this, 'internet', localStrings.getString('internetPage'),
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

      options.internet.NetworkElement.decorate($('wiredList'));
      $('wiredList').load(templateData.wiredList);
      options.internet.NetworkElement.decorate($('wirelessList'));
      $('wirelessList').load(templateData.wirelessList);
      options.internet.NetworkElement.decorate($('rememberedList'));
      $('rememberedList').load(templateData.rememberedList);

      options.internet.CellularPlanElement.decorate($('planList'));

      $('wiredSection').hidden = (templateData.wiredList.length == 0);
      $('wirelessSection').hidden = (templateData.wirelessList.length == 0);
      $('rememberedSection').hidden = (templateData.rememberedList.length == 0);
      InternetOptions.setupAttributes(templateData);
      // Setting up the details page
      $('detailsInternetOk').onclick = function(event) {
          InternetOptions.setDetails();
      };
      $('detailsInternetDismiss').onclick = function(event) {
          OptionsPage.clearOverlays();
      };
      $('detailsInternetLogin').onclick = function(event) {
          InternetOptions.loginFromDetails();
      };
      $('enableWifi').onclick = function(event) {
        event.target.disabled = true;
        chrome.send('enableWifi', []);
      };
      $('disableWifi').onclick = function(event) {
        event.target.disabled = true;
        chrome.send('disableWifi', []);
      };
      $('enableCellular').onclick = function(event) {
        event.target.disabled = true;
        chrome.send('enableCellular', []);
      };
      $('disableCellular').onclick = function(event) {
        event.target.disabled = true;
        chrome.send('disableCellular', []);
      };
      $('purchaseMore').onclick = function(event) {
        chrome.send('buyDataPlan', []);
      };
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
    }
  };

  // A boolean flag from InternerOptionsHandler to indicate whether to use
  // inline DOMUI for ethernet/wifi login/options.
  InternetOptions.useSettingsUI = false;

  // Network status update will be blocked while typing in WEP password etc.
  InternetOptions.updateLocked = false;
  InternetOptions.updatePending = false;
  InternetOptions.updataData = null;

  InternetOptions.loginFromDetails = function () {
    var data = $('inetAddress').data;
    var servicePath = data.servicePath;
    if (data.certInPkcs) {
      chrome.send('loginToCertNetwork',[String(servicePath),
                                        String(data.certPath),
                                        String($('inetIdentPkcs').value)]);
    } else {
      chrome.send('loginToCertNetwork',[String(servicePath),
                                        String($('inetCert').value),
                                        String($('inetIdent').value),
                                        String($('inetCertPass').value)]);
    }
    OptionsPage.clearOverlays();
  };

  InternetOptions.setDetails = function() {
    var data = $('inetAddress').data;
    if (data.type == 2) {
      var newinfo = [];
      newinfo.push(data.servicePath);
      newinfo.push($('autoConnectNetwork').checked ? "true" : "false");
      if (data.encrypted && data.certNeeded) {
        if (data.certInPkcs) {
          newinfo.push($('inetIdentPkcs').value);
        } else {
          newinfo.push($('inetIdent').value);
          newinfo.push($('inetCert').value);
          newinfo.push($('inetCertPass').value);
        }
      }
      chrome.send('setDetails', newinfo);
    }
    OptionsPage.clearOverlays();
  };

  InternetOptions.setupAttributes = function(data) {
    var buttons = $('wirelessButtons');
    if (data.wifiEnabled) {
      $('disableWifi').disabled = false;
      $('disableWifi').classList.remove('hidden');
      $('enableWifi').classList.add('hidden');
    } else {
      $('enableWifi').disabled = false;
      $('enableWifi').classList.remove('hidden');
      $('disableWifi').classList.add('hidden');
    }
    if (data.cellularAvailable) {
      if (data.cellularEnabled) {
        $('disableCellular').disabled = false;
        $('disableCellular').classList.remove('hidden');
        $('enableCellular').classList.add('hidden');
      } else {
        $('enableCellular').disabled = false;
        $('enableCellular').classList.remove('hidden');
        $('disableCellular').classList.add('hidden');
      }
    } else {
      $('enableCellular').classList.add('hidden');
      $('disableCellular').classList.add('hidden');
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
    if (InternetOptions.updateLocked) {
      InternetOptions.updateData = data;
      InternetOptions.updatePending = true;
    } else {
      $('wiredList').load(data.wiredList);
      $('wirelessList').load(data.wirelessList);
      $('rememberedList').load(data.rememberedList);

      $('wiredSection').hidden = (data.wiredList.length == 0);
      $('wirelessSection').hidden = (data.wirelessList.length == 0);
      InternetOptions.setupAttributes(data);
      $('rememberedSection').hidden = (data.rememberedList.length == 0);
      InternetOptions.updateData = null;
      InternetOptions.updatePending = false;
    }
  };

  InternetOptions.updateCellularPlans = function (data) {
    var page = $('detailsInternetPage');
    page.removeAttribute('cellplanloading');
    if (data.plans && data.plans.length) {
      page.removeAttribute('nocellplan');
      page.setAttribute('hascellplan', true);
      $('planList').load(data.plans);
    } else {
      page.setAttribute('nocellplan', true);
      page.removeAttribute('hascellplan');
    }
    if (!data.needsPlan) {
      page.setAttribute('hasactiveplan', true);
    } else {
      page.removeAttribute('hasactiveplan');
    }

    // Nudge webkit so that it redraws the details overlay page.
    // See http://crosbug.com/9616 for details.
    // Webkit bug: https://bugs.webkit.org/show_bug.cgi?id=50176
    var dummy = page.ownerDocument.createTextNode(' ');
    page.appendChild(dummy);
    page.removeChild(dummy);
  };

  InternetOptions.showPasswordEntry = function (data) {
    var element = $(data.servicePath);
    element.showPassword();
  };

  InternetOptions.showDetailedInfo = function (data) {
    var page = $('detailsInternetPage');
    if (data.connected) {
      $('inetTitle').textContent = localStrings.getString('inetStatus');
    } else {
      $('inetTitle').textContent = localStrings.getString('inetConnect');
    }
    if (data.connecting) {
      page.setAttribute('connecting', data.connecting);
    } else {
      page.removeAttribute('connecting');
    }
    if (data.connected) {
      page.setAttribute('connected', data.connected);
    } else {
      page.removeAttribute('connected');
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
      page.setAttribute('wireless', true);
      page.removeAttribute('ethernet');
      page.removeAttribute('cellular');
      page.removeAttribute('gsm');
      $('inetSsid').textContent = data.ssid;
      $('autoConnectNetwork').checked = data.autoConnect;
      if (!AccountsOptions.currentUserIsOwner()) {
        // Disable this for guest non-Owners.
        $('autoConnectNetwork').disabled = true;
      }
      page.removeAttribute('password');
      page.removeAttribute('cert');
      page.removeAttribute('certPkcs');
      if (data.encrypted) {
        if (data.certNeeded) {
          if (data.certInPkcs) {
            page.setAttribute('certPkcs', true);
            $('inetIdentPkcs').value = data.ident;
          } else {
            page.setAttribute('cert', true);
            $('inetIdent').value = data.ident;
            $('inetCert').value = data.certPath;
          }
        } else {
          page.setAttribute('password', true);
        }
      }
    } else if(data.type == 5) {
      OptionsPage.showTab($('cellularPlanNavTab'));
      page.removeAttribute('ethernet');
      page.removeAttribute('wireless');
      page.removeAttribute('cert');
      page.removeAttribute('certPkcs');
      page.setAttribute('cellular', true);
      $('serviceName').textContent = data.serviceName;
      $('networkTechnology').textContent = data.networkTechnology;
      $('activationState').textContent = data.activationState;
      $('roamingState').textContent = data.roamingState;
      $('restrictedPool').textContent = data.restrictedPool;
      $('errorState').textContent = data.errorState;
      $('manufacturer').textContent = data.manufacturer;
      $('modelId').textContent = data.modelId;
      $('firmwareRevision').textContent = data.firmwareRevision;
      $('hardwareRevision').textContent = data.hardwareRevision;
      $('lastUpdate').textContent = data.lastUpdate;
      $('prlVersion').textContent = data.prlVersion;
      $('meid').textContent = data.meid;
      $('imei').textContent = data.imei;
      $('mdn').textContent = data.mdn;
      $('esn').textContent = data.esn;
      $('min').textContent = data.min;
      if (!data.gsm) {
        page.removeAttribute('gsm');
      } else {
        $('operatorName').textContent = data.operatorName;
        $('operatorCode').textContent = data.operatorCode;
        $('imsi').textContent = data.imsi;
        page.setAttribute('gsm', true);
      }
      page.removeAttribute('hascellplan');
      page.removeAttribute('nocellplan');
      page.setAttribute('cellplanloading', true);
      chrome.send('refreshCellularPlan', [data.servicePath])
    } else {
      OptionsPage.showTab($('internetNavTab'));
      page.setAttribute('ethernet', true);
      page.removeAttribute('wireless');
      page.removeAttribute('cert');
      page.removeAttribute('certPkcs');
      page.removeAttribute('cellular');
      page.removeAttribute('gsm');
    }
    OptionsPage.showOverlay('detailsInternetPage');
  };

  // Export
  return {
    InternetOptions: InternetOptions
  };

});
