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
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      options.internet.NetworkElement.decorate($('wiredList'));
      $('wiredList').load(templateData.wiredList);
      options.internet.NetworkElement.decorate($('wirelessList'));
      $('wirelessList').load(templateData.wirelessList);
      options.internet.NetworkElement.decorate($('rememberedList'));
      $('rememberedList').load(templateData.rememberedList);

      $('wiredSection').hidden = (templateData.wiredList.length == 0);
      $('wirelessSection').hidden = (templateData.wirelessList.length == 0);
      $('rememberedSection').hidden = (templateData.rememberedList.length == 0);
      InternetOptions.setupAttributes(templateData);
      // Setting up the details page
      $('detailsInternetDismiss').onclick = function(event) {
          OptionsPage.clearOverlays();
      };

      $('detailsInternetLogin').onclick = function(event) {
          InternetOptions.loginFromDetails();
      };
      $('enableWifi').onclick = function(event) {
        chrome.send('enableWifi', []);
      };
      $('disableWifi').onclick = function(event) {
         chrome.send('disableWifi', []);
      };
      $('enableCellular').onclick = function(event) {
         chrome.send('enableCellular', []);
      };
      $('disableCellular').onclick = function(event) {
         chrome.send('disableCellular', []);
      };
      $('purchaseMore').onclick = function(event) {
        chrome.send('buyDataPlan', []);
      };
      $('moreInfo').onclick = function(event) {
        chrome.send('showMorePlanInfo', []);
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

  InternetOptions.loginFromDetails = function () {
    var data = $('inetAddress').data;
    var servicePath = data.servicePath;
    if (data.certinpkcs) {
      chrome.send('loginToCertNetwork',[String(servicePath),
                                        String(data.certPath),
                                        String(data.ident),
                                        String(data.certPass)]);
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
      newinfo.push($('rememberNetwork').checked);
      if (data.encrypted) {
        if (data.certneeded) {
          newinfo.push($('inetIdent').value);
          newinfo.push($('inetCert').value);
          newinfo.push($('inetCertPass').value);
        } else {
          newinfo.push($('inetPass').value);
        }
      }
      chrome.send('setDetails', newinfo);
    }
  };

  InternetOptions.setupAttributes = function(data) {
    var buttons = $('wirelessButtons');
    if (data.wifiEnabled) {
      buttons.setAttribute('wifiEnabled', true);
    } else {
      buttons.removeAttribute('wifiEnabled');
    }
    if (data.cellularAvailable) {
      buttons.setAttribute('cellularAvail', true);
      if (data.cellularEnabled) {
        buttons.setAttribute('cellularEnabled', true);
      } else {
        buttons.removeAttribute('cellularEnabled');
      }
    } else {
      buttons.removeAttribute('cellularAvail');
    }
  };

  //
  //Chrome callbacks
  //
  InternetOptions.refreshNetworkData = function (data) {
    $('wiredList').load(data.wiredList);
    $('wirelessList').load(data.wirelessList);
    $('rememberedList').load(data.rememberedList);

    $('wiredSection').hidden = (data.wiredList.length == 0);
    $('wirelessSection').hidden = (data.wirelessList.length == 0);
    InternetOptions.setupAttributes(data);
    $('rememberedSection').hidden = (data.rememberedList.length == 0);
  };

  InternetOptions.updateCellularPlans = function (data) {
    var page = $('detailsInternetPage');
    if (!data.plans || !data.plans.length || !data.plans[0].plan_type) {
      // No cellular data plan.
      page.setAttribute('nocellplan', true);
      page.removeAttribute('hascellplan');
    } else {
      page.removeAttribute('nocellplan');
      page.setAttribute('hascellplan', true);
      var plan = data.plans[0];
      $('planSummary').textContent = plan.planSummary;
      $('dataRemaining').textContent = plan.dataRemaining;
      $('planExpires').textContent = plan.planExpires;
    }
    page.removeAttribute('cellplanloading');
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
      address.data = null;
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
      $('rememberNetwork').checked = data.autoConnect;
      if (data.encrypted) {
        if (data.certNeeded) {
          page.setAttribute('cert', true);
          if (data.certInPkcs) {
            page.setAttribute('certPkcs', true);
            $('inetIdentPkcs').value = data.ident;
          } else {
            page.removeAttribute('certPkcs');
            $('inetIdent').value = data.ident;
            $('inetCert').value = data.certPath;
            $('inetCertPass').value = data.certPass;
          }
        } else {
          page.removeAttribute('cert');
          $('inetPass').value = data.pass;
        }
      } else {
        page.removeAttribute('cert');
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
