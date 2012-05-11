// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.internet', function() {
  var OptionsPage = options.OptionsPage;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Network settings constants. These enums must match their C++
   * counterparts.
   */
  function Constants() {}

  // Network types:
  Constants.TYPE_UNKNOWN = 0;
  Constants.TYPE_ETHERNET = 1;
  Constants.TYPE_WIFI = 2;
  Constants.TYPE_WIMAX = 3;
  Constants.TYPE_BLUETOOTH = 4;
  Constants.TYPE_CELLULAR = 5;
  Constants.TYPE_VPN = 6;

  /*
   * Minimum delay in milliseconds before updating controls.  Used to
   * consolidate update requests resulting from preference update
   * notifications.
   * @type {Number}
   * @const
   */
  var minimumUpdateContentsDelay_ = 50;

  /**
   * Time of the last request to update controls in milliseconds.
   * @type {Number}
   */
  var lastContentsUpdateRequest_ = 0;

  /**
   * Time of the last update to the controls in milliseconds.
   * @type {Number}
   */
  var lastContentsUpdate_ = 0;

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
    var now = new Date();
    requestUpdateControls(now.getTime());
  }

  /**
   * Throttles the frequency of updating controls to accelerate loading of the
   * page.
   * @param {Number} when Timestamp for the update request.
   */
  function requestUpdateControls(when) {
    if (when < lastContentsUpdate_)
      return;
    var now = new Date();
    var time = now.getTime();
    if (!lastContentsUpdateRequest_)
      lastContentsUpdateRequest_ = time;
    var elapsed = time - lastContentsUpdateRequest_;
    if (elapsed > minimumUpdateContentsDelay_) {
      DetailsInternetPage.getInstance().updateControls();
    } else {
      setTimeout(function() {
        requestUpdateControls(when);
      }, minimumUpdateContentsDelay_);
    }
    lastContentsUpdateRequest_ = time;
  }

  /////////////////////////////////////////////////////////////////////////////
  // DetailsInternetPage class:

  /**
   * Encapsulated handling of ChromeOS internet details overlay page.
   * @constructor
   */
  function DetailsInternetPage() {
    OptionsPage.call(this,
                     'detailsInternetPage',
                     null,
                     'details-internet-page');
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
      options.internet.CellularPlanElement.decorate($('plan-list'));
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
      chrome.send('networkCommand',
          [networkType, servicePath, 'options']);
    },

    /**
     * Initializes the contents of the page.
     */
    initializePageContents_: function() {
      $('details-internet-dismiss').addEventListener('click', function(event) {
        DetailsInternetPage.setDetails();
      });

      $('details-internet-login').addEventListener('click', function(event) {
        DetailsInternetPage.setDetails();
        DetailsInternetPage.loginFromDetails();
      });

      $('details-internet-disconnect').addEventListener('click',
                                                        function(event) {
        DetailsInternetPage.setDetails();
        DetailsInternetPage.disconnectNetwork();
      });

      $('activate-details').addEventListener('click', function(event) {
        DetailsInternetPage.activateFromDetails();
      });

      $('buyplan-details').addEventListener('click', function(event) {
        chrome.send('buyDataPlan');
        OptionsPage.closeOverlay();
      });

      $('view-account-details').addEventListener('click', function(event) {
        chrome.send('showMorePlanInfo');
        OptionsPage.closeOverlay();
      });

      $('cellular-apn-use-default').addEventListener('click', function(event) {
        var data = $('connection-state').data;
        var apnSelector = $('select-apn');

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

      $('cellular-apn-set').addEventListener('click', function(event) {
        if ($('cellular-apn').value == '')
          return;

        var data = $('connection-state').data;
        var apnSelector = $('select-apn');

        data.apn.apn = String($('cellular-apn').value);
        data.apn.username = String($('cellular-apn-username').value);
        data.apn.password = String($('cellular-apn-password').value);
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

      $('cellular-apn-cancel').addEventListener('click', function(event) {
        $('select-apn').selectedIndex = $('connection-state').data.selectedApn;
        updateHidden('.apn-list-view', false);
        updateHidden('.apn-details-view', true);
      });

      $('select-apn').addEventListener('change', function(event) {
        var data = $('connection-state').data;
        var apnSelector = $('select-apn');
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
          $('cellular-apn').value = data.apn.apn;
          $('cellular-apn-username').value = data.apn.username;
          $('cellular-apn-password').value = data.apn.password;

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
      options.proxyexceptions.ProxyExceptions.decorate($('ignored-host-list'));
      $('remove-host').addEventListener('click',
                                        this.handleRemoveProxyExceptions_);
      $('add-host').addEventListener('click', this.handleAddProxyException_);
      $('direct-proxy').addEventListener('click', this.disableManualProxy_);
      $('manual-proxy').addEventListener('click', this.enableManualProxy_);
      $('auto-proxy').addEventListener('click', this.disableManualProxy_);
      $('proxy-all-protocols').addEventListener('click',
                                                this.toggleSingleProxy_);
      observePrefsUI($('direct-proxy'));
      observePrefsUI($('manual-proxy'));
      observePrefsUI($('auto-proxy'));
      observePrefsUI($('proxy-all-protocols'));
    },

    /**
     * Handler for "add" event fired from userNameEdit.
     * @param {Event} e Add event fired from userNameEdit.
     * @private
     */
    handleAddProxyException_: function(e) {
      var exception = $('new-host').value;
      $('new-host').value = '';

      exception = exception.trim();
      if (exception)
        $('ignored-host-list').addException(exception);
    },

    /**
     * Handler for when the remove button is clicked
     * @param {Event} e The click event.
     * @private
     */
    handleRemoveProxyExceptions_: function(e) {
      var selectedItems = $('ignored-host-list').selectedItems;
      for (var x = 0; x < selectedItems.length; x++) {
        $('ignored-host-list').removeException(selectedItems[x]);
      }
    },

    /**
     * Update details page controls.
     * @private
     */
    updateControls: function() {
      // Record time of the update to throttle future updates.
      var now = new Date();
      lastContentsUpdate_ = now.getTime();

      // Only show ipconfig section if network is connected OR if nothing on
      // this device is connected. This is so that you can fix the ip configs
      // if you can't connect to any network.
      // TODO(chocobo): Once ipconfig is moved to flimflam service objects,
      //   we need to redo this logic to allow configuration of all networks.
      $('ipconfig-section').hidden = !this.connected && this.deviceConnected;

      // Network type related.
      updateHidden('#details-internet-page .cellular-details', !this.cellular);
      updateHidden('#details-internet-page .wifi-details', !this.wireless);
      updateHidden('#details-internet-page .vpn-details', !this.vpn);
      updateHidden('#details-internet-page .proxy-details', !this.showProxy);
      /* Network information merged into the Wifi tab for wireless networks
         unless the option is set for enabling a static IP configuration. */
      updateHidden('#details-internet-page .network-details',
                   this.wireless && !this.showStaticIPConfig);
      updateHidden('#details-internet-page .wifi-network-setting',
                   this.showStaticIPConfig);

      // Cell plan related.
      $('plan-list').hidden = this.cellplanloading;
      updateHidden('#details-internet-page .no-plan-info',
                   !this.cellular || this.cellplanloading || this.hascellplan);
      updateHidden('#details-internet-page .plan-loading-info',
                   !this.cellular || this.nocellplan || this.hascellplan);
      updateHidden('#details-internet-page .plan-details-info',
                   !this.cellular || this.nocellplan || this.cellplanloading);
      updateHidden('#details-internet-page .gsm-only',
                   !this.cellular || !this.gsm);
      updateHidden('#details-internet-page .cdma-only',
                   !this.cellular || this.gsm);
      updateHidden('#details-internet-page .apn-list-view',
                   !this.cellular || !this.gsm);
      updateHidden('#details-internet-page .apn-details-view', true);

      // Password and shared.
      updateHidden('#details-internet-page #password-details',
                   !this.wireless || !this.password);
      updateHidden('#details-internet-page #shared-network', !this.shared);
      updateHidden('#details-internet-page #prefer-network',
                   !this.showPreferred);

      // Proxy
      this.updateProxyBannerVisibility_();
      this.toggleSingleProxy_();
      if ($('manual-proxy').checked)
        this.enableManualProxy_();
      else
        this.disableManualProxy_();
      if (!this.proxyListInitialized_ && this.visible) {
        this.proxyListInitialized_ = true;
        $('ignored-host-list').redraw();
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
      var controlledBy = $('direct-proxy').controlledBy;
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
      if ($('proxy-all-protocols').checked) {
        $('multi-proxy').hidden = true;
        $('single-proxy').hidden = false;
      } else {
        $('multi-proxy').hidden = false;
        $('single-proxy').hidden = true;
      }
    },

    /**
     * Handler for selecting a radio button that will disable the manual
     * controls.
     * @private
     * @param {Event} e Click event.
     */
    disableManualProxy_: function(e) {
      $('advanced-config').hidden = true;
      $('proxy-all-protocols').disabled = true;
      $('proxy-host-name').disabled = true;
      $('proxy-host-port').disabled = true;
      $('proxy-host-single-name').disabled = true;
      $('proxy-host-single-port').disabled = true;
      $('secure-proxy-host-name').disabled = true;
      $('secure-proxy-port').disabled = true;
      $('ftp-proxy').disabled = true;
      $('ftp-proxy-port').disabled = true;
      $('socks-host').disabled = true;
      $('socks-port').disabled = true;
      $('proxy-config').disabled = $('auto-proxy').disabled ||
                                   !$('auto-proxy').checked;
    },

    /**
     * Handler for selecting a radio button that will enable the manual
     * controls.
     * @private
     * @param {Event} e Click event.
     */
    enableManualProxy_: function(e) {
      $('advanced-config').hidden = false;
      $('ignored-host-list').redraw();
      var all_disabled = $('manual-proxy').disabled;
      $('new-host').disabled = all_disabled;
      $('remove-host').disabled = all_disabled;
      $('add-host').disabled = all_disabled;
      $('proxy-all-protocols').disabled = all_disabled;
      $('proxy-host-name').disabled = all_disabled;
      $('proxy-host-port').disabled = all_disabled;
      $('proxy-host-single-name').disabled = all_disabled;
      $('proxy-host-single-port').disabled = all_disabled;
      $('secure-proxy-host-name').disabled = all_disabled;
      $('secure-proxy-port').disabled = all_disabled;
      $('ftp-proxy').disabled = all_disabled;
      $('ftp-proxy-port').disabled = all_disabled;
      $('socks-host').disabled = all_disabled;
      $('socks-port').disabled = all_disabled;
      $('proxy-config').disabled = true;
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
    $('buyplan-details').hidden = true;
    $('activate-details').hidden = true;
    $('view-account-details').hidden = true;
    detailsPage.cellular = false;
    detailsPage.wireless = false;
    detailsPage.vpn = false;
    detailsPage.showProxy = true;
    updateHidden('#internet-tab', true);
    updateHidden('#details-tab-strip', true);
    updateHidden('#details-internet-page .action-area', true);
    detailsPage.updateControls();
    detailsPage.visible = true;
  };

  DetailsInternetPage.updateCellularPlans = function(data) {
    var detailsPage = DetailsInternetPage.getInstance();
    detailsPage.cellplanloading = false;
    if (data.plans && data.plans.length) {
      detailsPage.nocellplan = false;
      detailsPage.hascellplan = true;
      $('plan-list').load(data.plans);
    } else {
      detailsPage.nocellplan = true;
      detailsPage.hascellplan = false;
    }

    detailsPage.hasactiveplan = !data.needsPlan;
    detailsPage.activated = data.activated;
    if (!data.activated)
      $('details-internet-login').hidden = true;

    $('buyplan-details').hidden = !data.showBuyButton;
    $('activate-details').hidden = !data.showActivateButton;
    $('view-account-details').hidden = !data.showViewAccountButton;
  };

  DetailsInternetPage.updateSecurityTab = function(requirePin) {
    $('sim-card-lock-enabled').checked = requirePin;
    $('change-pin').hidden = !requirePin;
  };


  DetailsInternetPage.loginFromDetails = function() {
    var data = $('connection-state').data;
    var servicePath = data.servicePath;
    chrome.send('networkCommand', [String(data.type),
                                          servicePath,
                                          'connect']);
    OptionsPage.closeOverlay();
  };

  DetailsInternetPage.disconnectNetwork = function() {
    var data = $('connection-state').data;
    var servicePath = data.servicePath;
    chrome.send('networkCommand', [String(data.type),
                                        servicePath,
                                        'disconnect']);
    OptionsPage.closeOverlay();
  };

  DetailsInternetPage.activateFromDetails = function() {
    var data = $('connection-state').data;
    var servicePath = data.servicePath;
    if (data.type == Constants.TYPE_CELLULAR) {
      chrome.send('networkCommand', [String(data.type),
                                          String(servicePath),
                                          'activate']);
    }
    OptionsPage.closeOverlay();
  };

  DetailsInternetPage.setDetails = function() {
    var data = $('connection-state').data;
    var servicePath = data.servicePath;
    if (data.type == Constants.TYPE_WIFI) {
      chrome.send('setPreferNetwork',
                   [String(servicePath),
                    $('prefer-network-wifi').checked ? 'true' : 'false']);
      chrome.send('setAutoConnect',
                  [String(servicePath),
                   $('auto-connect-network-wifi').checked ? 'true' : 'false']);
    } else if (data.type == Constants.TYPE_CELLULAR) {
      chrome.send('setAutoConnect',
                  [String(servicePath),
                   $('auto-connect-network-cellular').checked ? 'true' :
                       'false']);
    }
    var ipConfigList = $('ip-config-list');
    chrome.send('setIPConfig', [String(servicePath),
                                $('ip-type-dhcp').checked ? 'true' : 'false',
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
    // TODO(kevers): Find more appropriate place to cache data.
    $('connection-state').data = data;

    $('buyplan-details').hidden = true;
    $('activate-details').hidden = true;
    $('view-account-details').hidden = true;
    $('details-internet-login').hidden = data.connected;
    if (data.type == Constants.TYPE_ETHERNET)
      $('details-internet-disconnect').hidden = true;
    else
      $('details-internet-disconnect').hidden = !data.connected;

    detailsPage.deviceConnected = data.deviceConnected;
    detailsPage.connecting = data.connecting;
    detailsPage.connected = data.connected;
    detailsPage.showProxy = data.showProxy;
    detailsPage.showStaticIPConfig = data.showStaticIPConfig;
    $('connection-state').textContent = data.connectionState;

    var inetAddress = '';
    var inetSubnetAddress = '';
    var inetGateway = '';
    var inetDns = '';
    $('ip-type-dhcp').checked = true;
    if (data.ipconfigStatic.value) {
      inetAddress = data.ipconfigStatic.value.address;
      inetSubnetAddress = data.ipconfigStatic.value.subnetAddress;
      inetGateway = data.ipconfigStatic.value.gateway;
      inetDns = data.ipconfigStatic.value.dns;
      $('ip-type-static').checked = true;
    } else if (data.ipconfigDHCP.value) {
      inetAddress = data.ipconfigDHCP.value.address;
      inetSubnetAddress = data.ipconfigDHCP.value.subnetAddress;
      inetGateway = data.ipconfigDHCP.value.gateway;
      inetDns = data.ipconfigDHCP.value.dns;
    }

    // Hide the dhcp/static radio if needed.
    $('ip-type-dhcp-div').hidden = !data.showStaticIPConfig;
    $('ip-type-static-div').hidden = !data.showStaticIPConfig;

    var ipConfigList = $('ip-config-list');
    options.internet.IPConfigList.decorate(ipConfigList);
    ipConfigList.disabled =
        $('ip-type-dhcp').checked || data.ipconfigStatic.controlledBy ||
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

    $('ip-type-dhcp').addEventListener('click', function(event) {
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

    $('ip-type-static').addEventListener('click', function(event) {
      // Enable ipConfigList.
      ipConfigList.disabled = false;
      ipConfigList.focus();
      ipConfigList.selectionModel.selectedIndex = 0;
    });

    if (data.hardwareAddress) {
      $('hardware-address').textContent = data.hardwareAddress;
      $('hardware-address-row').style.display = 'table-row';
    } else {
      // This is most likely a device without a hardware address.
      $('hardware-address-row').style.display = 'none';
    }
    if (data.type == Constants.TYPE_WIFI) {
      OptionsPage.showTab($('wifi-network-nav-tab'));
      detailsPage.wireless = true;
      detailsPage.vpn = false;
      detailsPage.ethernet = false;
      detailsPage.cellular = false;
      detailsPage.gsm = false;
      detailsPage.shared = data.shared;
      $('wifi-connection-state').textContent = data.connectionState;
      $('wifi-ssid').textContent = data.ssid;
      if (data.bssid && data.bssid.length > 0) {
        $('wifi-bssid').textContent = data.bssid;
        $('wifi-bssid-entry').hidden = false;
      } else {
        $('wifi-bssid-entry').hidden = true;
      }
      $('wifi-ip-address').textContent = inetAddress;
      $('wifi-subnet-address').textContent = inetSubnetAddress;
      $('wifi-gateway').textContent = inetGateway;
      $('wifi-dns').textContent = inetDns;
      if (data.encryption && data.encryption.length > 0) {
        $('wifi-security').textContent = data.encryption;
        $('wifi-security-entry').hidden = false;
      } else {
        $('wifi-security-entry').hidden = true;
      }
      // Frequency is in MHz.
      var frequency = localStrings.getString('inetFrequencyFormat');
      frequency = frequency.replace('$1', data.frequency);
      $('wifi-frequency').textContent = frequency;
      // Signal strength as percentage.
      var signalStrength = localStrings.getString('inetSignalStrengthFormat');
      signalStrength = signalStrength.replace('$1', data.strength);
      $('wifi-signal-strength').textContent = signalStrength;
      if (data.hardwareAddress) {
        $('wifi-hardware-address').textContent = data.hardwareAddress;
        $('wifi-hardware-address-entry').hidden = false;
      } else {
        $('wifi-hardware-address-entry').hidden = true;
      }
      detailsPage.showPreferred = data.showPreferred;
      $('prefer-network-wifi').checked = data.preferred.value;
      $('prefer-network-wifi').disabled = !data.remembered;
      $('auto-connect-network-wifi').checked = data.autoConnect.value;
      $('auto-connect-network-wifi').disabled = !data.remembered;
      detailsPage.password = data.encrypted;
    } else if (data.type == Constants.TYPE_CELLULAR) {
      if (!data.gsm)
        OptionsPage.showTab($('cellular-plan-nav-tab'));
      else
        OptionsPage.showTab($('cellular-conn-nav-tab'));
      detailsPage.ethernet = false;
      detailsPage.wireless = false;
      detailsPage.vpn = false;
      detailsPage.cellular = true;
      $('service-name').textContent = data.serviceName;
      $('network-technology').textContent = data.networkTechnology;
      $('activation-state').textContent = data.activationState;
      $('roaming-state').textContent = data.roamingState;
      $('restricted-pool').textContent = data.restrictedPool;
      $('error-state').textContent = data.errorState;
      $('manufacturer').textContent = data.manufacturer;
      $('model-id').textContent = data.modelId;
      $('firmware-revision').textContent = data.firmwareRevision;
      $('hardware-revision').textContent = data.hardwareRevision;
      $('prl-version').textContent = data.prlVersion;
      $('meid').textContent = data.meid;
      $('imei').textContent = data.imei;
      $('mdn').textContent = data.mdn;
      $('esn').textContent = data.esn;
      $('min').textContent = data.min;
      detailsPage.gsm = data.gsm;
      if (data.gsm) {
        $('operator-name').textContent = data.operatorName;
        $('operator-code').textContent = data.operatorCode;
        $('imsi').textContent = data.imsi;

        var apnSelector = $('select-apn');
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
      $('auto-connect-network-cellular').checked = data.autoConnect.value;
      $('auto-connect-network-cellular').disabled = false;

      $('buyplan-details').hidden = !data.showBuyButton;
      $('view-account-details').hidden = !data.showViewAccountButton;
      $('activate-details').hidden = !data.showActivateButton;
      if (data.showActivateButton) {
        $('details-internet-login').hidden = true;
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
    } else if (data.type == Constants.TYPE_VPN) {
      OptionsPage.showTab($('vpn-nav-tab'));
      detailsPage.wireless = false;
      detailsPage.vpn = true;
      detailsPage.ethernet = false;
      detailsPage.cellular = false;
      detailsPage.gsm = false;
      $('inet-service-name').textContent = data.service_name;
      $('inet-server-hostname').textContent = data.server_hostname;
      $('inet-provider-type').textContent = data.provider_type;
      $('inet-username').textContent = data.username;
    } else {
      OptionsPage.showTab($('internet-nav-tab'));
      detailsPage.ethernet = true;
      detailsPage.wireless = false;
      detailsPage.vpn = false;
      detailsPage.cellular = false;
      detailsPage.gsm = false;
    }

    // Update controlled option indicators.
    indicators = cr.doc.querySelectorAll(
        '#details-internet-page .controlled-setting-indicator');
    for (var i = 0; i < indicators.length; i++) {
      var dataProperty = indicators[i].getAttribute('data');
      if (dataProperty && data[dataProperty]) {
        var controlledBy = data[dataProperty].controlledBy;
        if (controlledBy) {
          indicators[i].controlledBy = controlledBy;
          var forElement = $(indicators[i].getAttribute('for'));
          if (forElement)
            forElement.disabled = controlledBy != 'recommended';
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
