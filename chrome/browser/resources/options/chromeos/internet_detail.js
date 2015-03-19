// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: onc_data.js

// NOTE(stevenjb): This code is in the process of being converted to be
// compatible with the networkingPrivate extension API:
// * The network property dictionaries are being converted to use ONC values.
// * chrome.send calls will be replaced with chrome.networkingPrivate calls.
// See crbug.com/279351 for more info.

cr.define('options.internet', function() {
  var OncData = cr.onc.OncData;
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;
  /** @const */ var IPAddressField = options.internet.IPAddressField;

  /** @const */ var GoogleNameServers = ['8.8.4.4', '8.8.8.8'];
  /** @const */ var CarrierGenericUMTS = 'Generic UMTS';

  /**
   * Helper function to set hidden attribute for elements matching a selector.
   * @param {string} selector CSS selector for extracting a list of elements.
   * @param {boolean} hidden New hidden value.
   */
  function updateHidden(selector, hidden) {
    var elements = cr.doc.querySelectorAll(selector);
    for (var i = 0, el; el = elements[i]; i++) {
      el.hidden = hidden;
    }
  }

  /**
   * Helper function to update the properties of the data object from the
   * properties in the update object.
   * @param {Object} data Object to update.
   * @param {Object} update Object containing the updated properties.
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
   * @param {Object} value that should be converted to a string.
   * @return {string} the result.
   */
  function stringFromValue(value) {
    return value ? String(value) : '';
  }

  /**
   * @param {string} action An action to send to coreOptionsUserMetricsAction.
   */
  function sendChromeMetricsAction(action) {
    chrome.send('coreOptionsUserMetricsAction', [action]);
  }

  /**
   * Send metrics to Chrome when the detailed page is opened.
   * @param {string} type The ONC type of the network being shown.
   * @param {string} state The ONC network state.
   */
  function sendShowDetailsMetrics(type, state) {
    if (type == 'WiFi') {
      sendChromeMetricsAction('Options_NetworkShowDetailsWifi');
      if (state != 'NotConnected')
        sendChromeMetricsAction('Options_NetworkShowDetailsWifiConnected');
    } else if (type == 'Cellular') {
      sendChromeMetricsAction('Options_NetworkShowDetailsCellular');
      if (state != 'NotConnected')
        sendChromeMetricsAction('Options_NetworkShowDetailsCellularConnected');
    } else if (type == 'VPN') {
      sendChromeMetricsAction('Options_NetworkShowDetailsVPN');
      if (state != 'NotConnected')
        sendChromeMetricsAction('Options_NetworkShowDetailsVPNConnected');
    }
  }

  /**
   * Returns the netmask as a string for a given prefix length.
   * @param {number} prefixLength The ONC routing prefix length.
   * @return {string} The corresponding netmask.
   */
  function prefixLengthToNetmask(prefixLength) {
    // Return the empty string for invalid inputs.
    if (prefixLength < 0 || prefixLength > 32)
      return '';
    var netmask = '';
    for (var i = 0; i < 4; ++i) {
      var remainder = 8;
      if (prefixLength >= 8) {
        prefixLength -= 8;
      } else {
        remainder = prefixLength;
        prefixLength = 0;
      }
      if (i > 0)
        netmask += '.';
      var value = 0;
      if (remainder != 0)
        value = ((2 << (remainder - 1)) - 1) << (8 - remainder);
      netmask += value.toString();
    }
    return netmask;
  }

  /**
   * Returns the prefix length from the netmask string.
   * @param {string} netmask The netmask string, e.g. 255.255.255.0.
   * @return {number} The corresponding netmask or -1 if invalid.
   */
  function netmaskToPrefixLength(netmask) {
    var prefixLength = 0;
    var tokens = netmask.split('.');
    if (tokens.length != 4)
      return -1;
    for (var i = 0; i < tokens.length; ++i) {
      var token = tokens[i];
      // If we already found the last mask and the current one is not
      // '0' then the netmask is invalid. For example, 255.224.255.0
      if (prefixLength / 8 != i) {
        if (token != '0')
          return -1;
      } else if (token == '255') {
        prefixLength += 8;
      } else if (token == '254') {
        prefixLength += 7;
      } else if (token == '252') {
        prefixLength += 6;
      } else if (token == '248') {
        prefixLength += 5;
      } else if (token == '240') {
        prefixLength += 4;
      } else if (token == '224') {
        prefixLength += 3;
      } else if (token == '192') {
        prefixLength += 2;
      } else if (token == '128') {
        prefixLength += 1;
      } else if (token == '0') {
        prefixLength += 0;
      } else {
        // mask is not a valid number.
        return -1;
      }
    }
    return prefixLength;
  }

  /////////////////////////////////////////////////////////////////////////////
  // DetailsInternetPage class:

  /**
   * Encapsulated handling of ChromeOS internet details overlay page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function DetailsInternetPage() {
    // If non-negative, indicates a custom entry in select-apn.
    this.userApnIndex_ = -1;

    // The custom APN properties associated with entry |userApnIndex_|.
    this.userApn_ = {};

    // The currently selected APN entry in $('select-apn') (which may or may not
    // == userApnIndex_).
    this.selectedApnIndex_ = -1;

    // We show the Proxy configuration tab for remembered networks and when
    // configuring a proxy from the login screen.
    this.showProxy_ = false;

    Page.call(this, 'detailsInternetPage', '', 'details-internet-page');
  }

  cr.addSingletonGetter(DetailsInternetPage);

  DetailsInternetPage.prototype = {
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);
      this.initializePageContents_();
      this.showNetworkDetails_();
    },

    /**
     * Auto-activates the network details dialog if network information
     * is included in the URL.
     */
    showNetworkDetails_: function() {
      var guid = parseQueryParams(window.location).guid;
      if (!guid || !guid.length)
        return;
      chrome.send('loadVPNProviders');
      chrome.networkingPrivate.getManagedProperties(
          guid, DetailsInternetPage.initializeDetailsPage);
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

      $('details-internet-configure').addEventListener('click',
                                                       function(event) {
        DetailsInternetPage.setDetails();
        DetailsInternetPage.configureNetwork();
      });

      $('activate-details').addEventListener('click', function(event) {
        DetailsInternetPage.activateFromDetails();
      });

      $('view-account-details').addEventListener('click', function(event) {
        chrome.send('showMorePlanInfo',
                    [DetailsInternetPage.getInstance().onc_.guid()]);
        PageManager.closeOverlay();
      });

      $('cellular-apn-use-default').addEventListener('click', function(event) {
        DetailsInternetPage.getInstance().setDefaultApn_();
      });

      $('cellular-apn-set').addEventListener('click', function(event) {
        DetailsInternetPage.getInstance().setApn_($('cellular-apn').value);
      });

      $('cellular-apn-cancel').addEventListener('click', function(event) {
        DetailsInternetPage.getInstance().cancelApn_();
      });

      $('select-apn').addEventListener('change', function(event) {
        DetailsInternetPage.getInstance().selectApn_();
      });

      $('sim-card-lock-enabled').addEventListener('click', function(event) {
        var newValue = $('sim-card-lock-enabled').checked;
        // Leave value as is because user needs to enter PIN code first.
        // When PIN will be entered and value changed,
        // we'll update UI to reflect that change.
        $('sim-card-lock-enabled').checked = !newValue;
        var operation = newValue ? 'setLocked' : 'setUnlocked';
        chrome.send('simOperation', [operation]);
      });
      $('change-pin').addEventListener('click', function(event) {
        chrome.send('simOperation', ['changePin']);
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
     * @param {Event} event The click event.
     * @private
     */
    handleNameServerTypeChange_: function(event) {
      var type = event.target.value;
      DetailsInternetPage.updateNameServerDisplay(type);
    },

    /**
     * Gets the IPConfig ONC Object.
     * @param {string} nameServerType The selected name server type:
     *   'automatic', 'google', or 'user'.
     * @return {Object} The IPConfig ONC object.
     * @private
     */
    getIpConfig_: function(nameServerType) {
      var ipConfig = {};
      // If 'ip-address' is empty, automatic configuration will be used.
      if (!$('ip-automatic-configuration-checkbox').checked &&
          $('ip-address').model.value) {
        ipConfig['IPAddress'] = $('ip-address').model.value;
        var netmask = $('ip-netmask').model.value;
        var routingPrefix = 0;
        if (netmask) {
          routingPrefix = netmaskToPrefixLength(netmask);
          if (routingPrefix == -1) {
            console.error('Invalid netmask: ' + netmask);
            routingPrefix = 0;
          }
        }
        ipConfig['RoutingPrefix'] = routingPrefix;
        ipConfig['Gateway'] = $('ip-gateway').model.value || '';
      }

      // Note: If no nameserver fields are set, automatic configuration will be
      // used. TODO(stevenjb): Validate input fields.
      if (nameServerType != 'automatic') {
        var userNameServers = [];
        if (nameServerType == 'google') {
          userNameServers = GoogleNameServers.slice();
        } else if (nameServerType == 'user') {
          for (var i = 1; i <= 4; ++i) {
            var nameServerField = $('ipconfig-dns' + i);
            // Skip empty values.
            if (nameServerField && nameServerField.model &&
                nameServerField.model.value) {
              userNameServers.push(nameServerField.model.value);
            }
          }
        }
        if (userNameServers.length)
          ipConfig['NameServers'] = userNameServers.sort();
      }
      return ipConfig;
    },

    /**
     * Creates an indicator event for controlled properties using
     * the same dictionary format as CoreOptionsHandler::CreateValueForPref.
     * @param {string} name The name for the Event.
     * @param {{value: *, controlledBy: *, recommendedValue: *}} propData
     *     Property dictionary.
     * @private
     */
    createControlledEvent_: function(name, propData) {
      assert('value' in propData && 'controlledBy' in propData &&
             'recommendedValue' in propData);
      var event = new Event(name);
      event.value = {
        value: propData.value,
        controlledBy: propData.controlledBy,
        recommendedValue: propData.recommendedValue
      };
      return event;
    },

    /**
     * Creates an indicator event for controlled properties using
     * the ONC getManagedProperties dictionary format.
     * @param {string} name The name for the Event.
     * @param {Object} propData ONC managed network property dictionary.
     * @private
     */
    createManagedEvent_: function(name, propData) {
      var event = new Event(name);
      event.value = {};

      // Set the current value and recommended value.
      var activeValue = propData['Active'];
      var effective = propData['Effective'];
      if (activeValue == undefined)
        activeValue = propData[effective];
      event.value.value = activeValue;

      // If a property is editable then it is not enforced, and 'controlledBy'
      // is set to 'recommended' unless effective == {User|Shared}Setting, in
      // which case the value was modified from the recommended value.
      // Otherwise if 'Effective' is set to 'UserPolicy' or 'DevicePolicy' then
      // the set value is mandated by the policy.
      if (propData['UserEditable']) {
        if (effective == 'UserPolicy')
          event.value.controlledBy = 'recommended';
        event.value.recommendedValue = propData['UserPolicy'];
      } else if (propData['DeviceEditable']) {
        if (effective == 'DevicePolicy')
          event.value.controlledBy = 'recommended';
        event.value.recommendedValue = propData['DevicePolicy'];
      } else if (effective == 'UserPolicy' || effective == 'DevicePolicy') {
        event.value.controlledBy = 'policy';
      }

      return event;
    },

    /**
     * Update details page controls.
     */
    updateControls: function() {
      // Note: onc may be undefined when called from a pref update before
      // initialized in initializeDetailsPage.
      var onc = this.onc_;

      // Always show the ipconfig section. TODO(stevenjb): Improve the display
      // for unconnected networks. Currently the IP address fields may be
      // blank if the network is not connected.
      $('ipconfig-section').hidden = false;
      $('ipconfig-dns-section').hidden = false;

      // Network type related.
      updateHidden('#details-internet-page .cellular-details',
                   this.type_ != 'Cellular');
      updateHidden('#details-internet-page .wifi-details',
                   this.type_ != 'WiFi');
      updateHidden('#details-internet-page .wimax-details',
                   this.type_ != 'WiMAX');
      updateHidden('#details-internet-page .vpn-details', this.type_ != 'VPN');
      updateHidden('#details-internet-page .proxy-details', !this.showProxy_);

      // Cellular
      if (onc && this.type_ == 'Cellular') {
        // Hide gsm/cdma specific elements.
        if (onc.getActiveValue('Cellular.Family') == 'GSM')
          updateHidden('#details-internet-page .cdma-only', true);
        else
          updateHidden('#details-internet-page .gsm-only', true);
      }

      // Wifi

      // Hide network tab for VPN.
      updateHidden('#details-internet-page .network-details',
                   this.type_ == 'VPN');

      // Password and shared.
      var source = onc ? onc.getSource() : 'None';
      var shared = (source == 'Device' || source == 'DevicePolicy');
      var security = onc ? onc.getWiFiSecurity() : 'None';
      updateHidden('#details-internet-page #password-details',
                   this.type_ != 'WiFi' || security == 'None');
      updateHidden('#details-internet-page #wifi-shared-network', !shared);
      updateHidden('#details-internet-page #prefer-network', source == 'None');

      // WiMAX.
      updateHidden('#details-internet-page #wimax-shared-network', !shared);

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
     */
    toggleSingleProxy_: function() {
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
     */
    handleAutoConfigProxy_: function() {
      $('proxy-pac-url').disabled = !$('proxy-use-pac-url').checked;
    },

    /**
     * Handler for selecting a radio button that will disable the manual
     * controls.
     * @private
     */
    disableManualProxy_: function() {
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
      sendChromeMetricsAction('Options_NetworkManualProxy_Disable');
    },

    /**
     * Handler for selecting a radio button that will enable the manual
     * controls.
     * @private
     */
    enableManualProxy_: function() {
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
      sendChromeMetricsAction('Options_NetworkManualProxy_Enable');
    },

    /**
     * Helper method called from initializeDetailsPage and updateConnectionData.
     * Updates visibility/enabled of the login/disconnect/configure buttons.
     * @private
     */
    updateConnectionButtonVisibilty_: function() {
      var onc = this.onc_;
      if (this.type_ == 'Ethernet') {
        // Ethernet can never be connected or disconnected and can always be
        // configured (e.g. to set security).
        $('details-internet-login').hidden = true;
        $('details-internet-disconnect').hidden = true;
        $('details-internet-configure').hidden = false;
        return;
      }

      var connectState = onc.getActiveValue('ConnectionState');
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

      var connectable = onc.getActiveValue('Connectable');
      if (connectState != 'Connected' &&
          (!connectable || onc.getWiFiSecurity() != 'None' ||
          (this.type_ == 'WiMAX' || this.type_ == 'VPN'))) {
        $('details-internet-configure').hidden = false;
      } else {
        $('details-internet-configure').hidden = true;
      }
    },

    /**
     * Helper method called from initializeDetailsPage and updateConnectionData.
     * Updates the connection state property and account / sim card links.
     * @private
     */
    updateDetails_: function() {
      var onc = this.onc_;

      var connectionStateString = onc.getTranslatedValue('ConnectionState');
      $('connection-state').textContent = connectionStateString;

      var type = this.type_;
      var showViewAccount = false;
      var showActivate = false;
      if (type == 'WiFi') {
        $('wifi-connection-state').textContent = connectionStateString;
      } else if (type == 'WiMAX') {
        $('wimax-connection-state').textContent = connectionStateString;
      } else if (type == 'Cellular') {
        $('activation-state').textContent =
            onc.getTranslatedValue('Cellular.ActivationState');
        if (onc.getActiveValue('Cellular.Family') == 'GSM') {
          var lockEnabled =
              onc.getActiveValue('Cellular.SIMLockStatus.LockEnabled');
          $('sim-card-lock-enabled').checked = lockEnabled;
          $('change-pin').hidden = !lockEnabled;
        }
        showViewAccount = onc.getActiveValue('showViewAccountButton');
        var activationState = onc.getActiveValue('Cellular.ActivationState');
        showActivate = activationState == 'NotActivated' ||
            activationState == 'PartiallyActivated';
      }

      $('view-account-details').hidden = !showViewAccount;
      $('activate-details').hidden = !showActivate;
      // If activation is not complete, hide the login button.
      if (showActivate)
        $('details-internet-login').hidden = true;
    },

    /**
     * Helper method called from initializeDetailsPage and updateConnectionData.
     * Updates the fields in the header section of the details frame.
     * @private
     */
    populateHeader_: function() {
      var onc = this.onc_;

      $('network-details-title').textContent =
          this.networkTitle_ || onc.getTranslatedValue('Name');

      var connectionState = onc.getActiveValue('ConnectionState');
      var connectionStateString = onc.getTranslatedValue('ConnectionState');
      $('network-details-subtitle-status').textContent = connectionStateString;

      var typeKey;
      var type = this.type_;
      if (type == 'Ethernet')
        typeKey = 'ethernetTitle';
      else if (type == 'WiFi')
        typeKey = 'wifiTitle';
      else if (type == 'WiMAX')
        typeKey = 'wimaxTitle';
      else if (type == 'Cellular')
        typeKey = 'cellularTitle';
      else if (type == 'VPN')
        typeKey = 'vpnTitle';
      else
        typeKey = null;
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
    },

    /**
     * Helper method to insert a 'user' option into the Apn list.
     * @param {Object} userOption The 'user' apn dictionary
     * @private
     */
    insertApnUserOption_: function(userOption) {
      // Add the 'user' option before the last option ('other')
      var apnSelector = $('select-apn');
      assert(apnSelector.length > 0);
      var otherOption = apnSelector[apnSelector.length - 1];
      apnSelector.add(userOption, otherOption);
      this.userApnIndex_ = apnSelector.length - 2;
      this.selectedApnIndex_ = this.userApnIndex_;
    },

    /**
     * Helper method called from initializeApnList to populate the Apn list.
     * @param {Array} apnList List of available APNs.
     * @private
     */
    populateApnList_: function(apnList) {
      var onc = this.onc_;
      var apnSelector = $('select-apn');
      assert(apnSelector.length == 1);
      var otherOption = apnSelector[0];
      var activeApn = onc.getActiveValue('Cellular.APN.AccessPointName');
      var activeUsername = onc.getActiveValue('Cellular.APN.Username');
      var activePassword = onc.getActiveValue('Cellular.APN.Password');
      var lastGoodApn =
          onc.getActiveValue('Cellular.LastGoodAPN.AccessPointName');
      var lastGoodUsername =
          onc.getActiveValue('Cellular.LastGoodAPN.Username');
      var lastGoodPassword =
          onc.getActiveValue('Cellular.LastGoodAPN.Password');
      for (var i = 0; i < apnList.length; i++) {
        var apnDict = apnList[i];
        var option = document.createElement('option');
        var localizedName = apnDict['LocalizedName'];
        var name = localizedName ? localizedName : apnDict['Name'];
        var accessPointName = apnDict['AccessPointName'];
        option.textContent =
            name ? (name + ' (' + accessPointName + ')') : accessPointName;
        option.value = i;
        // Insert new option before 'other' option.
        apnSelector.add(option, otherOption);
        if (this.selectedApnIndex_ != -1)
          continue;
        // If this matches the active Apn name, or LastGoodApn name (or there
        // is no last good APN), set it as the selected Apn.
        if ((activeApn == accessPointName) ||
            (!activeApn && (!lastGoodApn || lastGoodApn == accessPointName))) {
          this.selectedApnIndex_ = i;
        }
      }
      if (this.selectedApnIndex_ == -1 && activeApn) {
        this.userApn_ = activeApn;
        // Create a 'user' entry for any active apn not in the list.
        var userOption = document.createElement('option');
        userOption.textContent = activeApn;
        userOption.value = -1;
        this.insertApnUserOption_(userOption);
      }
    },

    /**
     * Helper method called from initializeDetailsPage to initialize the Apn
     * list.
     * @private
     */
    initializeApnList_: function() {
      this.selectedApnIndex_ = -1;
      this.userApnIndex_ = -1;

      var onc = this.onc_;
      var apnSelector = $('select-apn');

      // Clear APN lists, keep only last element, 'other'.
      while (apnSelector.length != 1)
        apnSelector.remove(0);

      var apnList = onc.getActiveValue('Cellular.APNList');
      if (apnList) {
        // Populate the list with the existing APNs.
        this.populateApnList_(apnList);
      } else {
        // Create a single 'default' entry.
        var otherOption = apnSelector[0];
        var defaultOption = document.createElement('option');
        defaultOption.textContent =
            loadTimeData.getString('cellularApnUseDefault');
        defaultOption.value = -1;
        // Add 'default' entry before 'other' option
        apnSelector.add(defaultOption, otherOption);
        assert(apnSelector.length == 2);  // 'default', 'other'
        this.selectedApnIndex_ = 0;  // Select 'default'
      }
      assert(this.selectedApnIndex_ >= 0);
      apnSelector.selectedIndex = this.selectedApnIndex_;
      updateHidden('.apn-list-view', false);
      updateHidden('.apn-details-view', true);
    },

    /**
     * Helper function for setting APN properties.
     * @param {Object} apnValue Dictionary of APN properties.
     * @private
     */
    setActiveApn_: function(apnValue) {
      var activeApn = {};
      var apnName = apnValue['AccessPointName'];
      if (apnName) {
        activeApn['AccessPointName'] = apnName;
        activeApn['Username'] = stringFromValue(apnValue['Username']);
        activeApn['Password'] = stringFromValue(apnValue['Password']);
      }
      // Set the cached ONC data.
      this.onc_.setProperty('Cellular.APN', activeApn);
      // Set an ONC object with just the APN values.
      var oncData = new OncData({});
      oncData.setProperty('Cellular.APN', activeApn);
      chrome.networkingPrivate.setProperties(this.onc_.guid(),
                                             oncData.getData());
    },

    /**
     * Event Listener for the cellular-apn-use-default button.
     * @private
     */
    setDefaultApn_: function() {
      var apnSelector = $('select-apn');

      // Remove the 'user' entry if it exists.
      if (this.userApnIndex_ != -1) {
        assert(this.userApnIndex_ < apnSelector.length - 1);
        apnSelector.remove(this.userApnIndex_);
        this.userApnIndex_ = -1;
      }

      var apnList = this.onc_.getActiveValue('Cellular.APNList');
      var iApn = (apnList != undefined && apnList.length > 0) ? 0 : -1;
      apnSelector.selectedIndex = iApn;
      this.selectedApnIndex_ = iApn;

      // Clear any user APN entry to inform Chrome to use the default APN.
      this.setActiveApn_({});

      updateHidden('.apn-list-view', false);
      updateHidden('.apn-details-view', true);
    },

    /**
     * Event Listener for the cellular-apn-set button.
     * @private
     */
    setApn_: function(apnValue) {
      if (apnValue == '')
        return;

      var apnSelector = $('select-apn');

      var activeApn = {};
      activeApn['AccessPointName'] = stringFromValue(apnValue);
      activeApn['Username'] = stringFromValue($('cellular-apn-username').value);
      activeApn['Password'] = stringFromValue($('cellular-apn-password').value);
      this.setActiveApn_(activeApn);
      // Set the user selected APN.
      this.userApn_ = activeApn;

      // Remove any existing 'user' entry.
      if (this.userApnIndex_ != -1) {
        assert(this.userApnIndex_ < apnSelector.length - 1);
        apnSelector.remove(this.userApnIndex_);
        this.userApnIndex_ = -1;
      }

      // Create a new 'user' entry with the new active apn.
      var option = document.createElement('option');
      option.textContent = activeApn['AccessPointName'];
      option.value = -1;
      option.selected = true;
      this.insertApnUserOption_(option);

      updateHidden('.apn-list-view', false);
      updateHidden('.apn-details-view', true);
    },

    /**
     * Event Listener for the cellular-apn-cancel button.
     * @private
     */
    cancelApn_: function() {
      this.initializeApnList_();
    },

    /**
     * Event Listener for the select-apn button.
     * @private
     */
    selectApn_: function() {
      var onc = this.onc_;
      var apnSelector = $('select-apn');
      if (apnSelector[apnSelector.selectedIndex].value != -1) {
        var apnList = onc.getActiveValue('Cellular.APNList');
        var apnIndex = apnSelector.selectedIndex;
        assert(apnIndex < apnList.length);
        this.selectedApnIndex_ = apnIndex;
        this.setActiveApn_(apnList[apnIndex]);
      } else if (apnSelector.selectedIndex == this.userApnIndex_) {
        this.selectedApnIndex_ = apnSelector.selectedIndex;
        this.setActiveApn_(this.userApn_);
      } else { // 'Other'
        var apnDict;
        if (this.userApn_['AccessPointName']) {
          // Fill in the details fields with the existing 'user' config.
          apnDict = this.userApn_;
        } else {
          // No 'user' config, use the current values.
          apnDict = {};
          apnDict['AccessPointName'] =
              onc.getActiveValue('Cellular.APN.AccessPointName');
          apnDict['Username'] = onc.getActiveValue('Cellular.APN.Username');
          apnDict['Password'] = onc.getActiveValue('Cellular.APN.Password');
        }
        $('cellular-apn').value = stringFromValue(apnDict['AccessPointName']);
        $('cellular-apn-username').value = stringFromValue(apnDict['Username']);
        $('cellular-apn-password').value = stringFromValue(apnDict['Password']);
        updateHidden('.apn-list-view', true);
        updateHidden('.apn-details-view', false);
      }
    }
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
    chrome.send('setCarrier', [carrier]);
  };

  /**
   * Performs minimal initialization of the InternetDetails dialog in
   * preparation for showing proxy-settings.
   */
  DetailsInternetPage.initializeProxySettings = function() {
    DetailsInternetPage.getInstance().initializePageContents_();
  };

  /**
   * Displays the InternetDetails dialog with only the proxy settings visible.
   */
  DetailsInternetPage.showProxySettings = function() {
    var detailsPage = DetailsInternetPage.getInstance();
    $('network-details-header').hidden = true;
    $('activate-details').hidden = true;
    $('view-account-details').hidden = true;
    $('web-proxy-auto-discovery').hidden = true;
    detailsPage.showProxy_ = true;
    updateHidden('#internet-tab', true);
    updateHidden('#details-tab-strip', true);
    updateHidden('#details-internet-page .action-area', true);
    detailsPage.updateControls();
    detailsPage.visible = true;
    sendChromeMetricsAction('Options_NetworkShowProxyTab');
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
    var detailsPage = DetailsInternetPage.getInstance();
    if (detailsPage.type_ == 'WiFi')
      sendChromeMetricsAction('Options_NetworkConnectToWifi');
    else if (detailsPage.type_ == 'VPN')
      sendChromeMetricsAction('Options_NetworkConnectToVPN');
    // TODO(stevenjb): chrome.networkingPrivate.startConnect
    chrome.send('startConnect', [detailsPage.onc_.guid()]);
    PageManager.closeOverlay();
  };

  DetailsInternetPage.disconnectNetwork = function() {
    var detailsPage = DetailsInternetPage.getInstance();
    if (detailsPage.type_ == 'WiFi')
      sendChromeMetricsAction('Options_NetworkDisconnectWifi');
    else if (detailsPage.type_ == 'VPN')
      sendChromeMetricsAction('Options_NetworkDisconnectVPN');
    chrome.networkingPrivate.startDisconnect(detailsPage.onc_.guid());
    PageManager.closeOverlay();
  };

  DetailsInternetPage.configureNetwork = function() {
    var detailsPage = DetailsInternetPage.getInstance();
    chrome.send('configureNetwork', [detailsPage.onc_.guid()]);
    PageManager.closeOverlay();
  };

  DetailsInternetPage.activateFromDetails = function() {
    var detailsPage = DetailsInternetPage.getInstance();
    if (detailsPage.type_ == 'Cellular') {
      chrome.send('activateNetwork', [detailsPage.onc_.guid()]);
    }
    PageManager.closeOverlay();
  };

  /**
   * Event handler called when the details page is closed. Sends changed
   * properties to Chrome and closes the overlay.
   */
  DetailsInternetPage.setDetails = function() {
    var detailsPage = DetailsInternetPage.getInstance();
    var type = detailsPage.type_;
    var oncData = new OncData({});
    var autoConnectCheckboxId = '';
    if (type == 'WiFi') {
      var preferredCheckbox =
          assertInstanceof($('prefer-network-wifi'), HTMLInputElement);
      if (!preferredCheckbox.hidden && !preferredCheckbox.disabled) {
        var kPreferredPriority = 1;
        var priority = preferredCheckbox.checked ? kPreferredPriority : 0;
        oncData.setProperty('Priority', priority);
        sendChromeMetricsAction('Options_NetworkSetPrefer');
      }
      autoConnectCheckboxId = 'auto-connect-network-wifi';
    } else if (type == 'WiMAX') {
      autoConnectCheckboxId = 'auto-connect-network-wimax';
    } else if (type == 'Cellular') {
      autoConnectCheckboxId = 'auto-connect-network-cellular';
    } else if (type == 'VPN') {
      var providerType = detailsPage.onc_.getActiveValue('VPN.Type');
      if (providerType != 'ThirdPartyVPN') {
        oncData.setProperty('VPN.Type', providerType);
        oncData.setProperty('VPN.Host', $('inet-server-hostname').value);
        autoConnectCheckboxId = 'auto-connect-network-vpn';
      }
    }
    if (autoConnectCheckboxId != '') {
      var autoConnectCheckbox =
          assertInstanceof($(autoConnectCheckboxId), HTMLInputElement);
      if (!autoConnectCheckbox.hidden && !autoConnectCheckbox.disabled) {
        var autoConnectKey = type + '.AutoConnect';
        oncData.setProperty(autoConnectKey, !!autoConnectCheckbox.checked);
        sendChromeMetricsAction('Options_NetworkAutoConnect');
      }
    }

    var nameServerTypes = ['automatic', 'google', 'user'];
    var nameServerType = 'automatic';
    for (var i = 0; i < nameServerTypes.length; ++i) {
      if ($(nameServerTypes[i] + '-dns-radio').checked) {
        nameServerType = nameServerTypes[i];
        break;
      }
    }
    var ipConfig = detailsPage.getIpConfig_(nameServerType);
    var ipAddressType = ('IPAddress' in ipConfig) ? 'Static' : 'DHCP';
    var nameServersType = ('NameServers' in ipConfig) ? 'Static' : 'DHCP';
    oncData.setProperty('IPAddressConfigType', ipAddressType);
    oncData.setProperty('NameServersConfigType', nameServersType);
    oncData.setProperty('StaticIPConfig', ipConfig);

    var data = oncData.getData();
    if (Object.keys(data).length > 0) {
      // TODO(stevenjb): Only set changed properties.
      chrome.networkingPrivate.setProperties(detailsPage.onc_.guid(), data);
    }

    PageManager.closeOverlay();
  };

  /**
   * Event handler called when the name server type changes.
   * @param {string} type The selected name sever type, 'automatic', 'google',
   *                      or 'user'.
   */
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

  /**
   * Method called from Chrome when the ONC properties for the displayed
   * network may have changed.
   * @param {Object} oncData The updated ONC dictionary for the network.
   */
  DetailsInternetPage.updateConnectionData = function(oncData) {
    var detailsPage = DetailsInternetPage.getInstance();
    if (!detailsPage.visible)
      return;

    if (oncData.GUID != detailsPage.onc_.guid())
      return;

    // Update our cached data object.
    detailsPage.onc_ = new OncData(oncData);

    detailsPage.populateHeader_();
    detailsPage.updateConnectionButtonVisibilty_();
    detailsPage.updateDetails_();
  };

  /**
   * Method called from Chrome in response to getManagedProperties.
   * We only use this when we want to call initializeDetailsPage.
   * TODO(stevenjb): Eliminate when we switch to networkingPrivate
   * (initializeDetailsPage will be provided as the callback).
   * @param {Object} oncData Dictionary of ONC properties.
   */
  DetailsInternetPage.getManagedPropertiesResult = function(oncData) {
    DetailsInternetPage.initializeDetailsPage(oncData);
  };

  /**
   * Initializes the details page with the provided ONC data.
   * @param {Object} oncData Dictionary of ONC properties.
   */
  DetailsInternetPage.initializeDetailsPage = function(oncData) {
    var onc = new OncData(oncData);

    var detailsPage = DetailsInternetPage.getInstance();
    detailsPage.onc_ = onc;
    var type = onc.getActiveValue('Type');
    detailsPage.type_ = type;

    sendShowDetailsMetrics(type, onc.getActiveValue('ConnectionState'));

    if (type == 'VPN') {
      // Cache the dialog title, which will contain the provider name in the
      // case of a third-party VPN provider. This caching is important as the
      // provider may go away while the details dialog is being shown, causing
      // subsequent updates to be unable to determine the correct title.
      detailsPage.networkTitle_ = options.VPNProviders.formatNetworkName(onc);
    } else {
      delete detailsPage.networkTitle_;
    }

    detailsPage.populateHeader_();
    detailsPage.updateConnectionButtonVisibilty_();
    detailsPage.updateDetails_();

    // Inform chrome which network to pass events for in InternetOptionsHandler.
    chrome.send('setNetworkGuid', [detailsPage.onc_.guid()]);

    // TODO(stevenjb): Some of the setup below should be moved to
    // updateDetails_() so that updates are reflected in the UI.

    // Only show proxy for remembered networks.
    var remembered = onc.getSource() != 'None';
    if (remembered) {
      detailsPage.showProxy_ = true;
      // Inform Chrome which network to use for proxy configuration.
      chrome.send('selectNetwork', [detailsPage.onc_.guid()]);
    } else {
      detailsPage.showProxy_ = false;
    }

    $('web-proxy-auto-discovery').hidden = true;

    var restricted = onc.getActiveValue('RestrictedConnectivity');
    var restrictedString = loadTimeData.getString(
        restricted ? 'restrictedYes' : 'restrictedNo');

    // These objects contain an 'automatic' property that is displayed when
    // ip-automatic-configuration-checkbox is checked, and a 'value' property
    // that is displayed when unchecked and used to set the associated ONC
    // property for StaticIPConfig on commit.
    var inetAddress = {};
    var inetNetmask = {};
    var inetGateway = {};

    var inetNameServersString;

    var ipconfigList = onc.getActiveValue('IPConfigs');
    if (Array.isArray(ipconfigList)) {
      for (var i = 0; i < ipconfigList.length; ++i) {
        var ipconfig = ipconfigList[i];
        var ipType = ipconfig['Type'];
        if (ipType != 'IPv4') {
          // TODO(stevenjb): Handle IPv6 properties.
          continue;
        }
        var address = ipconfig['IPAddress'];
        inetAddress.automatic = address;
        inetAddress.value = address;
        var netmask = prefixLengthToNetmask(ipconfig['RoutingPrefix']);
        inetNetmask.automatic = netmask;
        inetNetmask.value = netmask;
        var gateway = ipconfig['Gateway'];
        inetGateway.automatic = gateway;
        inetGateway.value = gateway;
        if ('WebProxyAutoDiscoveryUrl' in ipconfig) {
          $('web-proxy-auto-discovery').hidden = false;
          $('web-proxy-auto-discovery-url').value =
              ipconfig['WebProxyAutoDiscoveryUrl'];
        }
        if ('NameServers' in ipconfig) {
          var inetNameServers = ipconfig['NameServers'];
          inetNameServers = inetNameServers.sort();
          inetNameServersString = inetNameServers.join(',');
        }
        break;  // Use the first IPv4 entry.
      }
    }

    // Override the 'automatic' properties with the saved DHCP values if the
    // saved value is set, and set any unset 'value' properties.
    var savedNameServersString;
    var savedIpAddress = onc.getActiveValue('SavedIPConfig.IPAddress');
    if (savedIpAddress != undefined) {
      inetAddress.automatic = savedIpAddress;
      if (!inetAddress.value)
        inetAddress.value = savedIpAddress;
    }
    var savedPrefix = onc.getActiveValue('SavedIPConfig.RoutingPrefix');
    if (savedPrefix != undefined) {
      assert(typeof savedPrefix == 'number');
      var savedNetmask = prefixLengthToNetmask(
          /** @type {number} */(savedPrefix));
      inetNetmask.automatic = savedNetmask;
      if (!inetNetmask.value)
        inetNetmask.value = savedNetmask;
    }
    var savedGateway = onc.getActiveValue('SavedIPConfig.Gateway');
    if (savedGateway != undefined) {
      inetGateway.automatic = savedGateway;
      if (!inetGateway.value)
        inetGateway.value = savedGateway;
    }

    var savedNameServers = onc.getActiveValue('SavedIPConfig.NameServers');
    if (savedNameServers) {
      savedNameServers = savedNameServers.sort();
      savedNameServersString = savedNameServers.join(',');
    }

    var ipAutoConfig = 'automatic';
    if (onc.getActiveValue('IPAddressConfigType') == 'Static') {
      ipAutoConfig = 'user';
      var staticIpAddress = onc.getActiveValue('StaticIPConfig.IPAddress');
      inetAddress.user = staticIpAddress;
      inetAddress.value = staticIpAddress;

      var staticPrefix = onc.getActiveValue('StaticIPConfig.RoutingPrefix');
      if (typeof staticPrefix != 'number')
        staticPrefix = 0;
      var staticNetmask = prefixLengthToNetmask(
          /** @type {number} */ (staticPrefix));
      inetNetmask.user = staticNetmask;
      inetNetmask.value = staticNetmask;

      var staticGateway = onc.getActiveValue('StaticIPConfig.Gateway');
      inetGateway.user = staticGateway;
      inetGateway.value = staticGateway;
    }

    var staticNameServersString;
    if (onc.getActiveValue('NameServersConfigType') == 'Static') {
      var staticNameServers = onc.getActiveValue('StaticIPConfig.NameServers');
      staticNameServers = staticNameServers.sort();
      staticNameServersString = staticNameServers.join(',');
    }

    $('ip-automatic-configuration-checkbox').checked =
        ipAutoConfig == 'automatic';

    inetAddress.autoConfig = ipAutoConfig;
    inetNetmask.autoConfig = ipAutoConfig;
    inetGateway.autoConfig = ipAutoConfig;

    var configureAddressField = function(field, model) {
      IPAddressField.decorate(field);
      field.model = model;
      field.editable = model.autoConfig == 'user';
    };
    configureAddressField($('ip-address'), inetAddress);
    configureAddressField($('ip-netmask'), inetNetmask);
    configureAddressField($('ip-gateway'), inetGateway);

    // Set Nameserver fields. Nameservers are 'automatic' by default. If a
    // static namerserver is set, use that unless it does not match a non
    // empty 'NameServers' value (indicating that the custom nameservers are
    // invalid or not being applied for some reason). TODO(stevenjb): Only
    // set these properites if they change so that invalid custom values do
    // not get lost.
    var nameServerType = 'automatic';
    if (staticNameServersString &&
        (!inetNameServersString ||
         staticNameServersString == inetNameServersString)) {
      if (staticNameServersString == GoogleNameServers.join(','))
        nameServerType = 'google';
      else
        nameServerType = 'user';
    }
    if (nameServerType == 'automatic')
      $('automatic-dns-display').textContent = inetNameServersString;
    else
      $('automatic-dns-display').textContent = savedNameServersString;
    $('google-dns-display').textContent = GoogleNameServers.join(',');

    var nameServersUser = [];
    if (staticNameServers) {
      nameServersUser = staticNameServers;
    } else if (savedNameServers) {
      // Pre-populate with values provided by DHCP server.
      nameServersUser = savedNameServers;
    }

    var nameServerModels = [];
    for (var i = 0; i < 4; ++i)
      nameServerModels.push({value: nameServersUser[i] || ''});

    $(nameServerType + '-dns-radio').checked = true;
    configureAddressField($('ipconfig-dns1'), nameServerModels[0]);
    configureAddressField($('ipconfig-dns2'), nameServerModels[1]);
    configureAddressField($('ipconfig-dns3'), nameServerModels[2]);
    configureAddressField($('ipconfig-dns4'), nameServerModels[3]);

    DetailsInternetPage.updateNameServerDisplay(nameServerType);

    var macAddress = onc.getActiveValue('MacAddress');
    if (macAddress) {
      $('hardware-address').textContent = macAddress;
      $('hardware-address-row').style.display = 'table-row';
    } else {
      // This is most likely a device without a hardware address.
      $('hardware-address-row').style.display = 'none';
    }

    var setOrHideParent = function(field, property) {
      if (property != undefined) {
        $(field).textContent = property;
        $(field).parentElement.hidden = false;
      } else {
        $(field).parentElement.hidden = true;
      }
    };

    var networkName = onc.getTranslatedValue('Name');

    // Signal strength as percentage (for WiFi and WiMAX).
    var signalStrength;
    if (type == 'WiFi' || type == 'WiMAX')
      signalStrength = onc.getActiveValue(type + '.SignalStrength');
    if (!signalStrength)
      signalStrength = 0;
    var strengthFormat = loadTimeData.getString('inetSignalStrengthFormat');
    var strengthString = strengthFormat.replace('$1', signalStrength);

    if (type == 'WiFi') {
      OptionsPage.showTab($('wifi-network-nav-tab'));
      $('wifi-restricted-connectivity').textContent = restrictedString;
      var ssid = onc.getActiveValue('WiFi.SSID');
      $('wifi-ssid').textContent = ssid ? ssid : networkName;
      setOrHideParent('wifi-bssid', onc.getActiveValue('WiFi.BSSID'));
      var security = onc.getWiFiSecurity();
      if (security == 'None')
        security = undefined;
      setOrHideParent('wifi-security', security);
      // Frequency is in MHz.
      var frequency = onc.getActiveValue('WiFi.Frequency');
      if (!frequency)
        frequency = 0;
      var frequencyFormat = loadTimeData.getString('inetFrequencyFormat');
      frequencyFormat = frequencyFormat.replace('$1', frequency);
      $('wifi-frequency').textContent = frequencyFormat;
      $('wifi-signal-strength').textContent = strengthString;
      setOrHideParent('wifi-hardware-address',
                      onc.getActiveValue('MacAddress'));
      var priority = onc.getActiveValue('Priority');
      $('prefer-network-wifi').checked = priority > 0;
      $('prefer-network-wifi').disabled = !remembered;
      $('auto-connect-network-wifi').checked =
          onc.getActiveValue('WiFi.AutoConnect');
      $('auto-connect-network-wifi').disabled = !remembered;
    } else if (type == 'WiMAX') {
      OptionsPage.showTab($('wimax-network-nav-tab'));
      $('wimax-restricted-connectivity').textContent = restrictedString;

      $('auto-connect-network-wimax').checked =
          onc.getActiveValue('WiMAX.AutoConnect');
      $('auto-connect-network-wimax').disabled = !remembered;
      var identity = onc.getActiveValue('WiMAX.EAP.Identity');
      setOrHideParent('wimax-eap-identity', identity);
      $('wimax-signal-strength').textContent = strengthString;
    } else if (type == 'Cellular') {
      OptionsPage.showTab($('cellular-conn-nav-tab'));

      var isGsm = onc.getActiveValue('Cellular.Family') == 'GSM';

      var currentCarrierIndex = -1;
      if (loadTimeData.getValue('showCarrierSelect')) {
        var currentCarrier =
            isGsm ? CarrierGenericUMTS : onc.getActiveValue('Cellular.Carrier');
        var supportedCarriers =
            onc.getActiveValue('Cellular.SupportedCarriers');
        for (var c1 = 0; c1 < supportedCarriers.length; ++c1) {
          if (supportedCarriers[c1] == currentCarrier) {
            currentCarrierIndex = c1;
            break;
          }
        }
        if (currentCarrierIndex != -1) {
          var carrierSelector = $('select-carrier');
          carrierSelector.onchange = DetailsInternetPage.handleCarrierChanged;
          carrierSelector.options.length = 0;
          for (var c2 = 0; c2 < supportedCarriers.length; ++c2) {
            var option = document.createElement('option');
            option.textContent = supportedCarriers[c2];
            carrierSelector.add(option);
          }
          carrierSelector.selectedIndex = currentCarrierIndex;
        }
      }
      if (currentCarrierIndex == -1)
        $('service-name').textContent = networkName;

      // TODO(stevenjb): Ideally many of these should be localized.
      $('network-technology').textContent =
          onc.getActiveValue('Cellular.NetworkTechnology');
      $('roaming-state').textContent =
          onc.getTranslatedValue('Cellular.RoamingState');
      $('cellular-restricted-connectivity').textContent = restrictedString;
      $('error-state').textContent = onc.getActiveValue('ErrorState');
      $('manufacturer').textContent =
          onc.getActiveValue('Cellular.Manufacturer');
      $('model-id').textContent = onc.getActiveValue('Cellular.ModelID');
      $('firmware-revision').textContent =
          onc.getActiveValue('Cellular.FirmwareRevision');
      $('hardware-revision').textContent =
          onc.getActiveValue('Cellular.HardwareRevision');
      $('mdn').textContent = onc.getActiveValue('Cellular.MDN');

      // Show ServingOperator properties only if available.
      var servingOperatorName =
          onc.getActiveValue('Cellular.ServingOperator.Name');
      var servingOperatorCode =
          onc.getActiveValue('Cellular.ServingOperator.Code');
      if (servingOperatorName != undefined &&
          servingOperatorCode != undefined) {
        $('operator-name').textContent = servingOperatorName;
        $('operator-code').textContent = servingOperatorCode;
      } else {
        $('operator-name').parentElement.hidden = true;
        $('operator-code').parentElement.hidden = true;
      }
      // Make sure that GSM/CDMA specific properties that shouldn't be hidden
      // are visible.
      updateHidden('#details-internet-page .gsm-only', false);
      updateHidden('#details-internet-page .cdma-only', false);

      // Show IMEI/ESN/MEID/MIN/PRL only if they are available.
      setOrHideParent('esn', onc.getActiveValue('Cellular.ESN'));
      setOrHideParent('imei', onc.getActiveValue('Cellular.IMEI'));
      setOrHideParent('meid', onc.getActiveValue('Cellular.MEID'));
      setOrHideParent('min', onc.getActiveValue('Cellular.MIN'));
      setOrHideParent('prl-version', onc.getActiveValue('Cellular.PRLVersion'));

      if (isGsm) {
        $('iccid').textContent = onc.getActiveValue('Cellular.ICCID');
        $('imsi').textContent = onc.getActiveValue('Cellular.IMSI');
        detailsPage.initializeApnList_();
      }
      $('auto-connect-network-cellular').checked =
          onc.getActiveValue('Cellular.AutoConnect');
      $('auto-connect-network-cellular').disabled = false;
    } else if (type == 'VPN') {
      OptionsPage.showTab($('vpn-nav-tab'));
      var providerType = onc.getActiveValue('VPN.Type');
      var isThirdPartyVPN = providerType == 'ThirdPartyVPN';
      $('vpn-tab').classList.toggle('third-party-vpn-provider',
                                    isThirdPartyVPN);

      $('inet-service-name').textContent = networkName;
      $('inet-provider-type').textContent =
          onc.getTranslatedValue('VPN.Type');

      if (isThirdPartyVPN) {
        $('inet-provider-name').textContent = '';
        var extensionID = onc.getActiveValue('VPN.ThirdPartyVPN.ExtensionID');
        var providers = options.VPNProviders.getProviders();
        for (var i = 0; i < providers.length; ++i) {
          if (extensionID == providers[i].extensionID) {
            $('inet-provider-name').textContent = providers[i].name;
            break;
          }
        }
      } else {
        var usernameKey;
        if (providerType == 'OpenVPN')
          usernameKey = 'VPN.OpenVPN.Username';
        else if (providerType == 'L2TP-IPsec')
          usernameKey = 'VPN.L2TP.Username';

        if (usernameKey) {
          $('inet-username').parentElement.hidden = false;
          $('inet-username').textContent = onc.getActiveValue(usernameKey);
        } else {
          $('inet-username').parentElement.hidden = true;
        }
        var inetServerHostname = $('inet-server-hostname');
        inetServerHostname.value = onc.getActiveValue('VPN.Host');
        inetServerHostname.resetHandler = function() {
          PageManager.hideBubble();
          var recommended = onc.getRecommendedValue('VPN.Host');
          if (recommended != undefined)
            inetServerHostname.value = recommended;
        };
        $('auto-connect-network-vpn').checked =
            onc.getActiveValue('VPN.AutoConnect');
        $('auto-connect-network-vpn').disabled = false;
      }
    } else {
      OptionsPage.showTab($('internet-nav-tab'));
    }

    // Update controlled option indicators.
    var indicators = cr.doc.querySelectorAll(
        '#details-internet-page .controlled-setting-indicator');
    for (var i = 0; i < indicators.length; i++) {
      var managed = indicators[i].hasAttribute('managed');
      // TODO(stevenjb): Eliminate support for 'data' once 39 is stable.
      var attributeName = managed ? 'managed' : 'data';
      var propName = indicators[i].getAttribute(attributeName);
      if (!propName)
        continue;
      var propValue = managed ?
          onc.getManagedProperty(propName) :
          onc.getActiveValue(propName);
      // If the property is unset or unmanaged (i.e. not an Object) skip it.
      if (propValue == undefined || (typeof propValue != 'object'))
        continue;
      var event;
      if (managed)
        event = detailsPage.createManagedEvent_(propName, propValue);
      else
        event = detailsPage.createControlledEvent_(propName,
            /** @type {{value: *, controlledBy: *, recommendedValue: *}} */(
                propValue));
      indicators[i].handlePrefChange(event);
      var forElement = $(indicators[i].getAttribute('internet-detail-for'));
      if (forElement) {
        if (event.value.controlledBy == 'policy')
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
