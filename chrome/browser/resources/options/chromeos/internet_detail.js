// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE(stevenjb): This code is in the process of being converted to be
// compatible with the networkingPrivate extension API:
// * The network property dictionaries are being converted to use ONC values.
// * chrome.send calls will be replaced with an API object that simulates the
//   networkingPrivate API. See network_config.js.
// See crbug.com/279351 for more info.

cr.define('options.internet', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var IPAddressField = options.internet.IPAddressField;

  /**
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

  /*
   * Helper function to update the properties of the data object from the
   * properties in the update object.
   * @param {object} data object to update.
   * @param {object} object containing the updated properties.
   */
  function updateDataObject(data, update) {
    for (var prop in update) {
      if (prop in data)
        data[prop] = update[prop];
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
    DetailsInternetPage.getInstance().updateControls();
  }

  /**
   * Simple helper method for converting a field to a string. It is used to
   * easily assign an empty string from fields that may be unknown or undefined.
   * @param {object} value that should be converted to a string.
   * @return {string} the result.
   */
  function stringFromValue(value) {
    return value ? String(value) : '';
  }

  /**
   * Sends the 'checked' state of a control to chrome for a network.
   * @param {string} path The service path of the network.
   * @param {string} message The message to send to chrome.
   * @param {HTMLInputElement} checkbox The checkbox storing the value to send.
   */
  function sendCheckedIfEnabled(path, message, checkbox) {
    if (!checkbox.hidden && !checkbox.disabled)
      chrome.send(message, [path, checkbox.checked ? 'true' : 'false']);
  }

  /**
   * Looks up the string to display for 'state' in loadTimeData.
   * @param {string} state The ONC State property of a network.
   */
  function networkOncStateString(state) {
    if (state == 'NotConnected')
      return loadTimeData.getString('OncStateNotConnected');
    else if (state == 'Connecting')
      return loadTimeData.getString('OncStateConnecting');
    else if (state == 'Connected')
      return loadTimeData.getString('OncStateConnected');
    return loadTimeData.getString('OncStateUnknown');
  }

  /**
   * Returns the display name for the network represented by 'data'.
   * @param {Object} data The network ONC dictionary.
   */
  function getNetworkName(data) {
    if (data.Type == 'Ethernet')
      return loadTimeData.getString('ethernetName');
    return data.Name;
  }

  /**
   * Returns True if the network represented by 'data' is a secure WiFi network.
   * @param {Object} data The network ONC dictionary.
   */
  function isSecureWiFiNetwork(data) {
    return data.WiFi && data.WiFi.Security && data.WiFi.Security != 'None';
  }

  /////////////////////////////////////////////////////////////////////////////
  // DetailsInternetPage class:

  /**
   * Encapsulated handling of ChromeOS internet details overlay page.
   * @constructor
   */
  function DetailsInternetPage() {
    Page.call(this, 'detailsInternetPage', null, 'details-internet-page');
  }

  cr.addSingletonGetter(DetailsInternetPage);

  DetailsInternetPage.prototype = {
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);
      var params = parseQueryParams(window.location);
      this.initializePageContents_(params);
      this.showNetworkDetails_(params);
    },

    /**
     * Auto-activates the network details dialog if network information
     * is included in the URL.
     */
    showNetworkDetails_: function(params) {
      var servicePath = params.servicePath;
      if (!servicePath || !servicePath.length)
        return;
      var networkType = '';  // ignored for 'options'
      chrome.send('networkCommand', [networkType, servicePath, 'options']);
    },


    /**
     * Initializes the contents of the page.
     */
    initializePageContents_: function(params) {
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

      $('details-internet-configure').addEventListener('click',
                                                       function(event) {
        DetailsInternetPage.setDetails();
        DetailsInternetPage.configureNetwork();
      });

      $('activate-details').addEventListener('click', function(event) {
        DetailsInternetPage.activateFromDetails();
      });

      $('buyplan-details').addEventListener('click', function(event) {
        var data = $('connection-state').data;
        chrome.send('buyDataPlan', [data.servicePath]);
        PageManager.closeOverlay();
      });

      $('view-account-details').addEventListener('click', function(event) {
        var data = $('connection-state').data;
        chrome.send('showMorePlanInfo', [data.servicePath]);
        PageManager.closeOverlay();
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
          var defaultApn = data.providerApnList.value[iApn];
          data.apn.apn = stringFromValue(defaultApn.apn);
          data.apn.username = stringFromValue(defaultApn.username);
          data.apn.password = stringFromValue(defaultApn.password);
          chrome.send('setApn', [data.servicePath,
                                 data.apn.apn,
                                 data.apn.username,
                                 data.apn.password]);
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

        data.apn.apn = stringFromValue($('cellular-apn').value);
        data.apn.username = stringFromValue($('cellular-apn-username').value);
        data.apn.password = stringFromValue($('cellular-apn-password').value);
        data.userApn = {
          'apn': data.apn.apn,
          'username': data.apn.username,
          'password': data.apn.password
        };
        chrome.send('setApn', [data.servicePath,
                               data.apn.apn,
                               data.apn.username,
                               data.apn.password]);

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
          chrome.send('setApn', [data.servicePath,
              stringFromValue(apnList[apnSelector.selectedIndex].apn),
              stringFromValue(apnList[apnSelector.selectedIndex].username),
              stringFromValue(apnList[apnSelector.selectedIndex].password)]
          );
          data.selectedApn = apnSelector.selectedIndex;
        } else if (apnSelector.selectedIndex == data.userApnIndex) {
          chrome.send('setApn', [data.servicePath,
                                 stringFromValue(data.userApn.apn),
                                 stringFromValue(data.userApn.username),
                                 stringFromValue(data.userApn.password)]);
          data.selectedApn = apnSelector.selectedIndex;
        } else {
          $('cellular-apn').value = stringFromValue(data.apn.apn);
          $('cellular-apn-username').value = stringFromValue(data.apn.username);
          $('cellular-apn-password').value = stringFromValue(data.apn.password);

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
      ['proxy-host-single-port',
       'secure-proxy-port',
       'socks-port',
       'ftp-proxy-port',
       'proxy-host-port'
      ].forEach(function(id) {
        options.PrefPortNumber.decorate($(id));
      });

      options.proxyexceptions.ProxyExceptions.decorate($('ignored-host-list'));
      $('remove-host').addEventListener('click',
                                        this.handleRemoveProxyExceptions_);
      $('add-host').addEventListener('click', this.handleAddProxyException_);
      $('direct-proxy').addEventListener('click', this.disableManualProxy_);
      $('manual-proxy').addEventListener('click', this.enableManualProxy_);
      $('auto-proxy').addEventListener('click', this.disableManualProxy_);
      $('proxy-all-protocols').addEventListener('click',
                                                this.toggleSingleProxy_);
      $('proxy-use-pac-url').addEventListener('change',
                                              this.handleAutoConfigProxy_);

      observePrefsUI($('direct-proxy'));
      observePrefsUI($('manual-proxy'));
      observePrefsUI($('auto-proxy'));
      observePrefsUI($('proxy-all-protocols'));
      observePrefsUI($('proxy-use-pac-url'));

      $('ip-automatic-configuration-checkbox').addEventListener('click',
        this.handleIpAutoConfig_);
      $('automatic-dns-radio').addEventListener('click',
        this.handleNameServerTypeChange_);
      $('google-dns-radio').addEventListener('click',
        this.handleNameServerTypeChange_);
      $('user-dns-radio').addEventListener('click',
        this.handleNameServerTypeChange_);

      // We only load this string if we have the string data available
      // because the proxy settings page on the login screen re-uses the
      // proxy sub-page from the internet options, and it doesn't ever
      // show the DNS settings, so we don't need this string there.
      // The string isn't available because
      // chrome://settings-frame/strings.js (where the string is
      // stored) is not accessible from the login screen.
      // TODO(pneubeck): Remove this once i18n of the proxy dialog on the login
      // page is fixed. http://crbug.com/242865
      if (loadTimeData.data_) {
        $('google-dns-label').innerHTML =
          loadTimeData.getString('googleNameServers');
      }
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
     * Handler for when the IP automatic configuration checkbox is clicked.
     * @param {Event} e The click event.
     * @private
     */
    handleIpAutoConfig_: function(e) {
      var checked = $('ip-automatic-configuration-checkbox').checked;
      var fields = [$('ip-address'), $('ip-netmask'), $('ip-gateway')];
      for (var i = 0; i < fields.length; ++i) {
        fields[i].editable = !checked;
        if (checked) {
          var model = fields[i].model;
          model.value = model.automatic;
          fields[i].model = model;
        }
      }
      if (!checked)
        $('ip-address').focus();
    },

    /**
     * Handler for when the name server selection changes.
     * @param {Event} e The click event.
     * @private
     */
    handleNameServerTypeChange_: function(event) {
      var type = event.target.value;
      DetailsInternetPage.updateNameServerDisplay(type);
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
      $('ipconfig-section').hidden = !this.connected && this.deviceConnected;
      $('ipconfig-dns-section').hidden =
        !this.connected && this.deviceConnected;

      // Network type related.
      updateHidden('#details-internet-page .cellular-details', !this.cellular);
      updateHidden('#details-internet-page .wifi-details', !this.wireless);
      updateHidden('#details-internet-page .wimax-details', !this.wimax);
      updateHidden('#details-internet-page .vpn-details', !this.vpn);
      updateHidden('#details-internet-page .proxy-details', !this.showProxy);

      // Cellular

      // Conditionally call updateHidden on .gsm-only, so that we don't unhide
      // a previously hidden element.
      if (this.gsm)
        updateHidden('#details-internet-page .cdma-only', true);
      else
        updateHidden('#details-internet-page .gsm-only', true);

      // Wifi

      // Network information merged into the Wifi tab for wireless networks
      // unless the option is set for enabling a static IP configuration.
      updateHidden('#details-internet-page .network-details',
                   (this.wireless && !this.showStaticIPConfig) || this.vpn);
      updateHidden('#details-internet-page .wifi-network-setting',
                   this.showStaticIPConfig);

      // Password and shared.
      updateHidden('#details-internet-page #password-details',
                   !this.wireless || !this.hasSecurity);
      updateHidden('#details-internet-page #wifi-shared-network',
          !this.shared);
      updateHidden('#details-internet-page #prefer-network',
                   !this.showPreferred);

      // WiMAX.
      updateHidden('#details-internet-page #wimax-shared-network',
        !this.shared);

      // Proxy
      this.updateProxyBannerVisibility_();
      this.toggleSingleProxy_();
      if ($('manual-proxy').checked)
        this.enableManualProxy_();
      else
        this.disableManualProxy_();
    },

    /**
     * Updates info banner visibility state. This function shows the banner
     * if proxy is managed or shared-proxies is off for shared network.
     * @private
     */
    updateProxyBannerVisibility_: function() {
      var bannerDiv = $('network-proxy-info-banner');
      if (!loadTimeData.data_) {
        // TODO(pneubeck): This temporarily prevents an exception below until
        // i18n of the proxy dialog on the login page is
        // fixed. http://crbug.com/242865
        bannerDiv.hidden = true;
        return;
      }

      // Show banner and determine its message if necessary.
      var controlledBy = $('direct-proxy').controlledBy;
      if (!controlledBy || controlledBy == '') {
        bannerDiv.hidden = true;
      } else {
        bannerDiv.hidden = false;
        // The possible banner texts are loaded in proxy_handler.cc.
        var bannerText = 'proxyBanner' + controlledBy.charAt(0).toUpperCase() +
                         controlledBy.slice(1);
        $('banner-text').textContent = loadTimeData.getString(bannerText);
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
     * Handler for when the user clicks on the checkbox to enter
     * auto configuration URL.
     * @private
     * @param {Event} e Click Event.
     */
    handleAutoConfigProxy_: function(e) {
      $('proxy-pac-url').disabled = !$('proxy-use-pac-url').checked;
    },

    /**
     * Handler for selecting a radio button that will disable the manual
     * controls.
     * @private
     * @param {Event} e Click event.
     */
    disableManualProxy_: function(e) {
      $('ignored-host-list').disabled = true;
      $('new-host').disabled = true;
      $('remove-host').disabled = true;
      $('add-host').disabled = true;
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
      $('proxy-use-pac-url').disabled = $('auto-proxy').disabled ||
                                        !$('auto-proxy').checked;
      $('proxy-pac-url').disabled = $('proxy-use-pac-url').disabled ||
                                    !$('proxy-use-pac-url').checked;
      $('auto-proxy-parms').hidden = !$('auto-proxy').checked;
      $('manual-proxy-parms').hidden = !$('manual-proxy').checked;
      chrome.send('coreOptionsUserMetricsAction',
                  ['Options_NetworkManualProxy_Disable']);
    },

    /**
     * Handler for selecting a radio button that will enable the manual
     * controls.
     * @private
     * @param {Event} e Click event.
     */
    enableManualProxy_: function(e) {
      $('ignored-host-list').redraw();
      var allDisabled = $('manual-proxy').disabled;
      $('ignored-host-list').disabled = allDisabled;
      $('new-host').disabled = allDisabled;
      $('remove-host').disabled = allDisabled;
      $('add-host').disabled = allDisabled;
      $('proxy-all-protocols').disabled = allDisabled;
      $('proxy-host-name').disabled = allDisabled;
      $('proxy-host-port').disabled = allDisabled;
      $('proxy-host-single-name').disabled = allDisabled;
      $('proxy-host-single-port').disabled = allDisabled;
      $('secure-proxy-host-name').disabled = allDisabled;
      $('secure-proxy-port').disabled = allDisabled;
      $('ftp-proxy').disabled = allDisabled;
      $('ftp-proxy-port').disabled = allDisabled;
      $('socks-host').disabled = allDisabled;
      $('socks-port').disabled = allDisabled;
      $('proxy-use-pac-url').disabled = true;
      $('proxy-pac-url').disabled = true;
      $('auto-proxy-parms').hidden = !$('auto-proxy').checked;
      $('manual-proxy-parms').hidden = !$('manual-proxy').checked;
      chrome.send('coreOptionsUserMetricsAction',
                  ['Options_NetworkManualProxy_Enable']);
    },
  };

  /**
   * Enables or Disables all buttons that provide operations on the cellular
   * network.
   */
  DetailsInternetPage.changeCellularButtonsState = function(disable) {
    var buttonsToDisableList =
        new Array('details-internet-login',
                  'details-internet-disconnect',
                  'details-internet-configure',
                  'activate-details',
                  'buyplan-details',
                  'view-account-details');

    for (var i = 0; i < buttonsToDisableList.length; ++i) {
      var button = $(buttonsToDisableList[i]);
      button.disabled = disable;
    }
  };

  /**
   * Shows a spinner while the carrier is changed.
   */
  DetailsInternetPage.showCarrierChangeSpinner = function(visible) {
    $('switch-carrier-spinner').hidden = !visible;
    // Disable any buttons that allow us to operate on cellular networks.
    DetailsInternetPage.changeCellularButtonsState(visible);
  };

  /**
   * Changes the network carrier.
   */
  DetailsInternetPage.handleCarrierChanged = function() {
    var carrierSelector = $('select-carrier');
    var carrier = carrierSelector[carrierSelector.selectedIndex].textContent;
    DetailsInternetPage.showCarrierChangeSpinner(true);
    var data = $('connection-state').data;
    chrome.send('setCarrier', [data.servicePath, carrier]);
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
    $('web-proxy-auto-discovery').hidden = true;
    detailsPage.cellular = false;
    detailsPage.wireless = false;
    detailsPage.vpn = false;
    detailsPage.showProxy = true;
    updateHidden('#internet-tab', true);
    updateHidden('#details-tab-strip', true);
    updateHidden('#details-internet-page .action-area', true);
    detailsPage.updateControls();
    detailsPage.visible = true;
    chrome.send('coreOptionsUserMetricsAction',
                ['Options_NetworkShowProxyTab']);
  };

  /**
   * Initializes even handling for keyboard driven flow.
   */
  DetailsInternetPage.initializeKeyboardFlow = function() {
    keyboard.initializeKeyboardFlow();
  };

  DetailsInternetPage.updateProxySettings = function(type) {
      var proxyHost = null,
          proxyPort = null;

      if (type == 'cros.session.proxy.singlehttp') {
        proxyHost = 'proxy-host-single-name';
        proxyPort = 'proxy-host-single-port';
      } else if (type == 'cros.session.proxy.httpurl') {
        proxyHost = 'proxy-host-name';
        proxyPort = 'proxy-host-port';
      } else if (type == 'cros.session.proxy.httpsurl') {
        proxyHost = 'secure-proxy-host-name';
        proxyPort = 'secure-proxy-port';
      } else if (type == 'cros.session.proxy.ftpurl') {
        proxyHost = 'ftp-proxy';
        proxyPort = 'ftp-proxy-port';
      } else if (type == 'cros.session.proxy.socks') {
        proxyHost = 'socks-host';
        proxyPort = 'socks-port';
      } else {
        return;
      }

      var hostValue = $(proxyHost).value;
      if (hostValue.indexOf(':') !== -1) {
        if (hostValue.match(/:/g).length == 1) {
          hostValue = hostValue.split(':');
          $(proxyHost).value = hostValue[0];
          $(proxyPort).value = hostValue[1];
        }
      }
  };

  DetailsInternetPage.updateCarrier = function() {
    DetailsInternetPage.showCarrierChangeSpinner(false);
  };

  DetailsInternetPage.loginFromDetails = function() {
    var data = $('connection-state').data;
    var servicePath = data.servicePath;
    chrome.send('networkCommand', [data.Type, servicePath, 'connect']);
    PageManager.closeOverlay();
  };

  DetailsInternetPage.disconnectNetwork = function() {
    var data = $('connection-state').data;
    var servicePath = data.servicePath;
    chrome.send('networkCommand', [data.Type, servicePath, 'disconnect']);
    PageManager.closeOverlay();
  };

  DetailsInternetPage.configureNetwork = function() {
    var data = $('connection-state').data;
    var servicePath = data.servicePath;
    chrome.send('networkCommand', [data.Type, servicePath, 'configure']);
    PageManager.closeOverlay();
  };

  DetailsInternetPage.activateFromDetails = function() {
    var data = $('connection-state').data;
    var servicePath = data.servicePath;
    if (data.Type == 'Cellular')
      chrome.send('networkCommand', [data.Type, servicePath, 'activate']);
    PageManager.closeOverlay();
  };

  DetailsInternetPage.setDetails = function() {
    var data = $('connection-state').data;
    var servicePath = data.servicePath;
    if (data.Type == 'WiFi') {
      sendCheckedIfEnabled(servicePath, 'setPreferNetwork',
                           $('prefer-network-wifi'));
      sendCheckedIfEnabled(servicePath, 'setAutoConnect',
                           $('auto-connect-network-wifi'));
    } else if (data.Type == 'Wimax') {
      sendCheckedIfEnabled(servicePath, 'setAutoConnect',
                           $('auto-connect-network-wimax'));
    } else if (data.Type == 'Cellular') {
      sendCheckedIfEnabled(servicePath, 'setAutoConnect',
                           $('auto-connect-network-cellular'));
    } else if (data.Type == 'VPN') {
      chrome.send('setServerHostname',
                  [servicePath,
                   $('inet-server-hostname').value]);
      sendCheckedIfEnabled(servicePath, 'setAutoConnect',
                           $('auto-connect-network-vpn'));
    }

    var nameServerTypes = ['automatic', 'google', 'user'];
    var nameServerType = 'automatic';
    for (var i = 0; i < nameServerTypes.length; ++i) {
      if ($(nameServerTypes[i] + '-dns-radio').checked) {
        nameServerType = nameServerTypes[i];
        break;
      }
    }

    // Skip any empty values.
    var userNameServers = [];
    for (var i = 1; i <= 4; ++i) {
      var nameServerField = $('ipconfig-dns' + i);
      if (nameServerField && nameServerField.model &&
          nameServerField.model.value) {
        userNameServers.push(nameServerField.model.value);
      }
    }

    userNameServers = userNameServers.join(',');

    chrome.send('setIPConfig',
                [servicePath,
                 Boolean($('ip-automatic-configuration-checkbox').checked),
                 $('ip-address').model.value || '',
                 $('ip-netmask').model.value || '',
                 $('ip-gateway').model.value || '',
                 nameServerType,
                 userNameServers]);
    PageManager.closeOverlay();
  };

  DetailsInternetPage.updateNameServerDisplay = function(type) {
    var editable = type == 'user';
    var fields = [$('ipconfig-dns1'), $('ipconfig-dns2'),
                  $('ipconfig-dns3'), $('ipconfig-dns4')];
    for (var i = 0; i < fields.length; ++i) {
      fields[i].editable = editable;
    }
    if (editable)
      $('ipconfig-dns1').focus();

    var automaticDns = $('automatic-dns-display');
    var googleDns = $('google-dns-display');
    var userDns = $('user-dns-settings');
    switch (type) {
      case 'automatic':
        automaticDns.setAttribute('selected', '');
        googleDns.removeAttribute('selected');
        userDns.removeAttribute('selected');
        break;
      case 'google':
        automaticDns.removeAttribute('selected');
        googleDns.setAttribute('selected', '');
        userDns.removeAttribute('selected');
        break;
      case 'user':
        automaticDns.removeAttribute('selected');
        googleDns.removeAttribute('selected');
        userDns.setAttribute('selected', '');
        break;
    }
  };

  DetailsInternetPage.updateConnectionButtonVisibilty = function(data) {
    if (data.type == 'Ethernet') {
      // Ethernet can never be connected or disconnected and can always be
      // configured (e.g. to set security).
      $('details-internet-login').hidden = true;
      $('details-internet-disconnect').hidden = true;
      $('details-internet-configure').hidden = false;
      return;
    }

    var connectState = data.ConnectionState;
    if (connectState == 'NotConnected') {
      $('details-internet-login').hidden = false;
      // Connecting to an unconfigured network might trigger certificate
      // installation UI. Until that gets handled here, always enable the
      // Connect button.
      $('details-internet-login').disabled = false;
      $('details-internet-disconnect').hidden = true;
    } else {
      $('details-internet-login').hidden = true;
      $('details-internet-disconnect').hidden = false;
    }

    var connectable = data.Connectable;
    if (connectState != 'Connected' &&
        (!connectable || isSecureWiFiNetwork(data) ||
        (data.Type == 'Wimax' || data.Type == 'VPN'))) {
      $('details-internet-configure').hidden = false;
    } else {
      $('details-internet-configure').hidden = true;
    }
  };

  DetailsInternetPage.updateConnectionData = function(update) {
    var detailsPage = DetailsInternetPage.getInstance();
    if (!detailsPage.visible)
      return;

    var data = $('connection-state').data;
    if (!data)
      return;

    if (update.servicePath != data.servicePath)
      return;

    // Update our cached data object.
    updateDataObject(data, update);

    detailsPage.deviceConnected = data.deviceConnected;
    detailsPage.connecting = data.ConnectionState == 'Connecting';
    detailsPage.connected = data.ConnectionState == 'Connected';
    var connectionStateString = networkOncStateString(data.ConnectionState);
    $('connection-state').textContent = connectionStateString;

    this.updateConnectionButtonVisibilty(data);

    if (data.Type == 'WiFi') {
      $('wifi-connection-state').textContent = connectionStateString;
    } else if (data.Type == 'Wimax') {
      $('wimax-connection-state').textContent = connectionStateString;
    } else if (data.Type == 'Cellular') {
      $('activation-state').textContent = data.activationState;

      $('buyplan-details').hidden = !data.showBuyButton;
      $('view-account-details').hidden = !data.showViewAccountButton;

      $('activate-details').hidden = !data.showActivateButton;
      if (data.showActivateButton)
        $('details-internet-login').hidden = true;

      if (detailsPage.gsm) {
        // TODO(stevenjb): Use managed properties for policy controlled values.
        var lockEnabled = data.simCardLockEnabled.value;
        $('sim-card-lock-enabled').checked = lockEnabled;
        $('change-pin').hidden = !lockEnabled;
      }
    }

    $('connection-state').data = data;
  };

  DetailsInternetPage.showDetailedInfo = function(data) {
    var detailsPage = DetailsInternetPage.getInstance();

    // Populate header
    $('network-details-title').textContent = getNetworkName(data);
    var connectionStateString = networkOncStateString(data.ConnectionState);
    $('network-details-subtitle-status').textContent = connectionStateString;
    var typeKey = null;
    switch (data.Type) {
    case 'Ethernet':
      typeKey = 'ethernetTitle';
      break;
    case 'WiFi':
      typeKey = 'wifiTitle';
      break;
    case 'Wimax':
      typeKey = 'wimaxTitle';
      break;
    case 'Cellular':
      typeKey = 'cellularTitle';
      break;
    case 'VPN':
      typeKey = 'vpnTitle';
      break;
    }
    var typeLabel = $('network-details-subtitle-type');
    var typeSeparator = $('network-details-subtitle-separator');
    if (typeKey) {
      typeLabel.textContent = loadTimeData.getString(typeKey);
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

    this.updateConnectionButtonVisibilty(data);

    $('web-proxy-auto-discovery').hidden = true;

    detailsPage.deviceConnected = data.deviceConnected;
    detailsPage.connecting = data.ConnectionState == 'Connecting';
    detailsPage.connected = data.ConnectionState == 'Connected';

    // Only show proxy for remembered networks.
    if (data.remembered) {
      detailsPage.showProxy = true;
      chrome.send('selectNetwork', [data.servicePath]);
    } else {
      detailsPage.showProxy = false;
    }
    detailsPage.showStaticIPConfig = data.showStaticIPConfig;
    $('connection-state').textContent = connectionStateString;

    var ipAutoConfig = data.ipAutoConfig ? 'automatic' : 'user';
    $('ip-automatic-configuration-checkbox').checked = data.ipAutoConfig;
    var inetAddress = {autoConfig: ipAutoConfig};
    var inetNetmask = {autoConfig: ipAutoConfig};
    var inetGateway = {autoConfig: ipAutoConfig};

    if (data.ipconfig.value) {
      inetAddress.automatic = data.ipconfig.value.address;
      inetAddress.value = data.ipconfig.value.address;
      inetNetmask.automatic = data.ipconfig.value.netmask;
      inetNetmask.value = data.ipconfig.value.netmask;
      inetGateway.automatic = data.ipconfig.value.gateway;
      inetGateway.value = data.ipconfig.value.gateway;
      if (data.ipconfig.value.webProxyAutoDiscoveryUrl) {
        $('web-proxy-auto-discovery').hidden = false;
        $('web-proxy-auto-discovery-url').value =
            data.ipconfig.value.webProxyAutoDiscoveryUrl;
      }
    }

    // Override the "automatic" values with the real saved DHCP values,
    // if they are set.
    if (data.savedIP.address) {
      inetAddress.automatic = data.savedIP.address;
      inetAddress.value = data.savedIP.address;
    }
    if (data.savedIP.netmask) {
      inetNetmask.automatic = data.savedIP.netmask;
      inetNetmask.value = data.savedIP.netmask;
    }
    if (data.savedIP.gateway) {
      inetGateway.automatic = data.savedIP.gateway;
      inetGateway.value = data.savedIP.gateway;
    }

    if (ipAutoConfig == 'user') {
      if (data.staticIP.value.address) {
        inetAddress.value = data.staticIP.value.address;
        inetAddress.user = data.staticIP.value.address;
      }
      if (data.staticIP.value.netmask) {
        inetNetmask.value = data.staticIP.value.netmask;
        inetNetmask.user = data.staticIP.value.netmask;
      }
      if (data.staticIP.value.gateway) {
        inetGateway.value = data.staticIP.value.gateway;
        inetGateway.user = data.staticIP.value.gateway;
      }
    }

    var configureAddressField = function(field, model) {
      IPAddressField.decorate(field);
      field.model = model;
      field.editable = model.autoConfig == 'user';
    };

    configureAddressField($('ip-address'), inetAddress);
    configureAddressField($('ip-netmask'), inetNetmask);
    configureAddressField($('ip-gateway'), inetGateway);

    var inetNameServers = '';
    if (data.ipconfig.value && data.ipconfig.value.nameServers) {
      inetNameServers = data.ipconfig.value.nameServers;
      $('automatic-dns-display').textContent = inetNameServers;
    }

    if (data.savedIP && data.savedIP.nameServers)
      $('automatic-dns-display').textContent = data.savedIP.nameServers;

    if (data.nameServersGoogle)
      $('google-dns-display').textContent = data.nameServersGoogle;

    var nameServersUser = [];
    if (data.staticIP.value.nameServers)
      nameServersUser = data.staticIP.value.nameServers.split(',');

    var nameServerModels = [];
    for (var i = 0; i < 4; ++i)
      nameServerModels.push({value: nameServersUser[i] || ''});

    $(data.nameServerType + '-dns-radio').checked = true;
    configureAddressField($('ipconfig-dns1'), nameServerModels[0]);
    configureAddressField($('ipconfig-dns2'), nameServerModels[1]);
    configureAddressField($('ipconfig-dns3'), nameServerModels[2]);
    configureAddressField($('ipconfig-dns4'), nameServerModels[3]);

    DetailsInternetPage.updateNameServerDisplay(data.nameServerType);

    if (data.MacAddress) {
      $('hardware-address').textContent = data.MacAddress;
      $('hardware-address-row').style.display = 'table-row';
    } else {
      // This is most likely a device without a hardware address.
      $('hardware-address-row').style.display = 'none';
    }

    // Signal strength as percentage (for WiFi and Wimax).
    var signalStrength =
        (data.WiFi && data.WiFi.SignalStrength) ? data.WiFi.SignalStrength : 0;
    var strengthFormat = loadTimeData.getString('inetSignalStrengthFormat');
    strengthFormat = strengthFormat.replace('$1', signalStrength);

    if (data.Type == 'WiFi') {
      OptionsPage.showTab($('wifi-network-nav-tab'));
      detailsPage.wireless = true;
      detailsPage.vpn = false;
      detailsPage.ethernet = false;
      detailsPage.cellular = false;
      detailsPage.gsm = false;
      detailsPage.wimax = false;
      detailsPage.shared = data.shared;
      $('wifi-connection-state').textContent = connectionStateString;
      $('wifi-ssid').textContent = data.WiFi ? data.WiFi.SSID : data.Name;
      if (data.WiFi && data.WiFi.BSSID) {
        $('wifi-bssid').textContent = data.WiFi.BSSID;
        $('wifi-bssid-entry').hidden = false;
      } else {
        $('wifi-bssid-entry').hidden = true;
      }
      $('wifi-ip-address').textContent = inetAddress.value;
      $('wifi-netmask').textContent = inetNetmask.value;
      $('wifi-gateway').textContent = inetGateway.value;
      $('wifi-name-servers').textContent = inetNameServers;
      var hasSecurity = isSecureWiFiNetwork(data);
      if (hasSecurity) {
        $('wifi-security').textContent = data.WiFi.Security;
        $('wifi-security-entry').hidden = false;
      } else {
        $('wifi-security-entry').hidden = true;
      }
      // Frequency is in MHz.
      var frequency =
          data.WiFi && data.WiFi.Frequency ? data.WiFi.Frequency : 0;
      var frequencyFormat = loadTimeData.getString('inetFrequencyFormat');
      frequencyFormat = frequencyFormat.replace('$1', frequency);
      $('wifi-frequency').textContent = frequencyFormat;
      $('wifi-signal-strength').textContent = strengthFormat;
      if (data.MacAddress) {
        $('wifi-hardware-address').textContent = data.MacAddress;
        $('wifi-hardware-address-entry').hidden = false;
      } else {
        $('wifi-hardware-address-entry').hidden = true;
      }
      detailsPage.showPreferred = data.remembered;
      $('prefer-network-wifi').checked = data.preferred.value;
      $('prefer-network-wifi').disabled = !data.remembered;
      $('auto-connect-network-wifi').checked = data.autoConnect.value;
      $('auto-connect-network-wifi').disabled = !data.remembered;
      detailsPage.hasSecurity = hasSecurity;
    } else if (data.Type == 'Wimax') {
      OptionsPage.showTab($('wimax-network-nav-tab'));
      detailsPage.wimax = true;
      detailsPage.wireless = false;
      detailsPage.vpn = false;
      detailsPage.ethernet = false;
      detailsPage.cellular = false;
      detailsPage.gsm = false;
      detailsPage.shared = data.shared;
      detailsPage.showPreferred = data.remembered;
      $('wimax-connection-state').textContent = connectionStateString;
      $('auto-connect-network-wimax').checked = data.autoConnect.value;
      $('auto-connect-network-wimax').disabled = !data.remembered;
      if (data.identity) {
        $('wimax-eap-identity').textContent = data.identity;
        $('wimax-eap-identity-entry').hidden = false;
      } else {
        $('wimax-eap-identity-entry').hidden = true;
      }
      $('wimax-signal-strength').textContent = strengthFormat;
    } else if (data.Type == 'Cellular') {
      OptionsPage.showTab($('cellular-conn-nav-tab'));
      detailsPage.ethernet = false;
      detailsPage.wireless = false;
      detailsPage.wimax = false;
      detailsPage.vpn = false;
      detailsPage.cellular = true;
      if (data.showCarrierSelect && data.currentCarrierIndex != -1) {
        var carrierSelector = $('select-carrier');
        carrierSelector.onchange = DetailsInternetPage.handleCarrierChanged;
        carrierSelector.options.length = 0;
        for (var i = 0; i < data.carriers.length; ++i) {
          var option = document.createElement('option');
          option.textContent = data.carriers[i];
          carrierSelector.add(option);
        }
        carrierSelector.selectedIndex = data.currentCarrierIndex;
      } else {
        $('service-name').textContent = getNetworkName(data);
      }

      $('network-technology').textContent = data.Cellular.NetworkTechnology;
      $('activation-state').textContent = data.activationState;
      $('roaming-state').textContent = data.roamingState;
      $('restricted-pool').textContent = data.restrictedPool;
      $('error-state').textContent = data.errorState;
      $('manufacturer').textContent = data.Cellular.Manufacturer;
      $('model-id').textContent = data.Cellular.ModelID;
      $('firmware-revision').textContent = data.Cellular.FirmwareRevision;
      $('hardware-revision').textContent = data.Cellular.HardwareRevision;
      $('mdn').textContent = data.Cellular.MDN;

      // Show ServingOperator properties only if available.
      if (data.Cellular.ServingOperator) {
        $('operator-name').textContent = data.Cellular.ServingOperator.Name;
        $('operator-code').textContent = data.Cellular.ServingOperator.Code;
      } else {
        $('operator-name').parentElement.hidden = true;
        $('operator-code').parentElement.hidden = true;
      }
      // Make sure that GSM/CDMA specific properties that shouldn't be hidden
      // are visible.
      updateHidden('#details-internet-page .gsm-only', false);
      updateHidden('#details-internet-page .cdma-only', false);

      // Show IMEI/ESN/MEID/MIN/PRL only if they are available.
      (function() {
        var setContentOrHide = function(field, value) {
          if (value)
            $(field).textContent = value;
          else
            $(field).parentElement.hidden = true;
        };
        setContentOrHide('esn', data.Cellular.ESN);
        setContentOrHide('imei', data.Cellular.IMEI);
        setContentOrHide('meid', data.Cellular.MEID);
        setContentOrHide('min', data.Cellular.MIN);
        setContentOrHide('prl-version', data.Cellular.PRLVersion);
      })();
      detailsPage.gsm = data.Cellular.Family == 'GSM';
      if (detailsPage.gsm) {
        $('iccid').textContent = data.Cellular.ICCID;
        $('imsi').textContent = data.Cellular.IMSI;

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
          var localizedName = apnList[i].localizedName;
          var name = localizedName ? localizedName : apnList[i].name;
          var apn = apnList[i].apn;
          option.textContent = name ? (name + ' (' + apn + ')') : apn;
          option.value = i;
          // data.apn and data.lastGoodApn will always be defined, however
          // data.apn.apn and data.lastGoodApn.apn may not be. This is not a
          // problem, as apnList[i].apn will always be defined and the
          // comparisons below will work as expected.
          if ((data.apn.apn == apn &&
               data.apn.username == apnList[i].username &&
               data.apn.password == apnList[i].password) ||
              (!data.apn.apn &&
               data.lastGoodApn.apn == apn &&
               data.lastGoodApn.username == apnList[i].username &&
               data.lastGoodApn.password == apnList[i].password)) {
            data.selectedApn = i;
          }
          // Insert new option before "other" option.
          apnSelector.add(option, otherOption);
        }
        if (data.selectedApn == -1 && data.apn.apn) {
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
        // TODO(stevenjb): Used managed properties for policy controlled value.
        var lockEnabled = data.simCardLockEnabled.value;
        $('sim-card-lock-enabled').checked = lockEnabled;
        $('change-pin').hidden = !lockEnabled;
      }
      $('auto-connect-network-cellular').checked = data.autoConnect.value;
      $('auto-connect-network-cellular').disabled = false;

      $('buyplan-details').hidden = !data.showBuyButton;
      $('view-account-details').hidden = !data.showViewAccountButton;
      $('activate-details').hidden = !data.showActivateButton;
      if (data.showActivateButton) {
        $('details-internet-login').hidden = true;
      }
    } else if (data.Type == 'VPN') {
      OptionsPage.showTab($('vpn-nav-tab'));
      detailsPage.wireless = false;
      detailsPage.wimax = false;
      detailsPage.vpn = true;
      detailsPage.ethernet = false;
      detailsPage.cellular = false;
      detailsPage.gsm = false;
      $('inet-service-name').textContent = getNetworkName(data);
      $('inet-provider-type').textContent = data.providerType;
      $('inet-username').textContent = data.username;
      var inetServerHostname = $('inet-server-hostname');
      inetServerHostname.value = data.serverHostname.value;
      inetServerHostname.resetHandler = function() {
        PageManager.hideBubble();
        inetServerHostname.value = data.serverHostname.recommendedValue;
      };
      $('auto-connect-network-vpn').checked = data.autoConnect.value;
      $('auto-connect-network-vpn').disabled = false;
    } else {
      OptionsPage.showTab($('internet-nav-tab'));
      detailsPage.ethernet = true;
      detailsPage.wireless = false;
      detailsPage.wimax = false;
      detailsPage.vpn = false;
      detailsPage.cellular = false;
      detailsPage.gsm = false;
    }

    // Update controlled option indicators.
    indicators = cr.doc.querySelectorAll(
        '#details-internet-page .controlled-setting-indicator');
    for (var i = 0; i < indicators.length; i++) {
      var propName = indicators[i].getAttribute('data');
      if (!propName || !data[propName])
        continue;
      var propData = data[propName];
      // Create a synthetic pref change event decorated as
      // CoreOptionsHandler::CreateValueForPref() does.
      var event = new Event(name);
      event.value = {
        value: propData.value,
        controlledBy: propData.controlledBy,
        recommendedValue: propData.recommendedValue
      };
      indicators[i].handlePrefChange(event);
      var forElement = $(indicators[i].getAttribute('for'));
      if (forElement) {
        if (propData.controlledBy == 'policy')
          forElement.disabled = true;
        if (forElement.resetHandler)
          indicators[i].resetHandler = forElement.resetHandler;
      }
    }

    detailsPage.updateControls();

    // Don't show page name in address bar and in history to prevent people
    // navigate here by hand and solve issue with page session restore.
    PageManager.showPageByName('detailsInternetPage', false);
  };

  return {
    DetailsInternetPage: DetailsInternetPage
  };
});
