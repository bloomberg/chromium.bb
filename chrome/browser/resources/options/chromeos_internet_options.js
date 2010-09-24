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

      // Setting up the details page
      $('detailsInternetDismiss').onclick = function(event) {
          OptionsPage.clearOverlays();
      };

      $('detailsInternetLogin').onclick = function(event) {
          InternetOptions.loginFromDetails();
      };
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

  //
  //Chrome callbacks
  //
  InternetOptions.refreshNetworkData = function (data) {
    $('wiredList').load(data.wiredList);
    $('wirelessList').load(data.wirelessList);
    $('rememberedList').load(data.rememberedList);

    $('wiredSection').hidden = (data.wiredList.length == 0);
    $('wirelessSection').hidden = (data.wirelessList.length == 0);
    $('rememberedSection').hidden = (data.rememberedList.length == 0);
  };

  InternetOptions.showPasswordEntry = function (data) {
    var element = $(data.servicePath);
    element.showPassword();
  };

  InternetOptions.showDetailedInfo = function (data) {
    var page = $('detailsInternetPage');
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
    var address = $('inetAddress');
    address.textContent = data.address;
    address.data = data;
    $('inetSubnetAddress').textContent = data.subnetAddress;
    $('inetGateway').textContent = data.gateway;
    $('inetDns').textContent = data.dns;
    if (data.type == 2) {
      page.removeAttribute('ethernet');
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
    } else {
      page.setAttribute('ethernet', true);
      page.removeAttribute('cert');
      page.removeAttribute('certPkcs');
    }
    OptionsPage.showOverlay('detailsInternetPage');
  };

  // Export
  return {
    InternetOptions: InternetOptions
  };

});
