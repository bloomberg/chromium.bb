// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: onc_data.js

// NOTE(stevenjb): This code is in the process of being converted to be
// compatible with the networkingPrivate extension API:
// * The network property dictionaries are being converted to use ONC values.
// * chrome.send calls will be replaced with an API object that simulates the
//   networkingPrivate API. See network_config.js.
// See crbug.com/279351 for more info.

/** @typedef {{address: (string|undefined),
 *             gateway: (string|undefined),
 *             nameServers: (string|undefined),
 *             netmask: (string|undefined),
 *             prefixLength: (number|undefined)}}
 * @see chrome/browser/ui/webui/options/chromeos/internet_options_handler.cc
 */
var IPInfo;

/**
 * InternetDetailedInfo argument passed to showDetailedInfo.
 * @see chrome/browser/ui/webui/options/chromeos/internet_options_handler.cc
 * @typedef {{
 *   servicePath: string,
 *   showCarrierSelect: (boolean|undefined),
 *   showViewAccountButton: (boolean|undefined)
 * }}
 */
var InternetDetailedInfo;

cr.define('options.internet', function() {
  var OncData = cr.onc.OncData;
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;
  /** @const */ var IPAddressField = options.internet.IPAddressField;

  /** @const */ var GoogleNameServersString = '8.8.4.4,8.8.8.8';
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
   * Sends the 'checked' state of a control to chrome for a network.
   * @param {string} path The service path of the network.
   * @param {string} message The message to send to chrome.
   * @param {string} checkboxId The id of the checkbox with the value to send.
   * @param {string=} opt_action Optional action to record.
   */
  function sendCheckedIfEnabled(path, message, checkboxId, opt_action) {
    var checkbox = assertInstanceof($(checkboxId), HTMLInputElement);
    if (!checkbox.hidden && !checkbox.disabled) {
      chrome.send(message, [path, checkbox.checked ? 'true' : 'false']);
      if (opt_action)
        sendChromeMetricsAction(opt_action);
    }
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

  /////////////////////////////////////////////////////////////////////////////
  // DetailsInternetPage class:

  /**
   * Encapsulated handling of ChromeOS internet details overlay page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function DetailsInternetPage() {
    // Cached Apn properties
    this.userApnIndex_ = -1;
    this.selectedApnIndex_ = -1;
    this.userApn_ = {};
    // We show the Proxy configuration tab for remembered networks and when
    // configuring a proxy from the login screen.
    this.showProxy_ = false;
    // In Chrome we sometimes want to enable the Cellular carrier select UI.
    this.showCarrierSelect_ = false;
    // In Chrome we sometimes want to show the 'View Account' button.
    this.showViewAccountButton_ = false;

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
      var servicePath = parseQueryParams(window.location).servicePath;
      if (!servicePath || !servicePath.length)
        return;
      var networkType = '';  // ignored for 'showDetails'
      chrome.send('networkCommand', [networkType, servicePath, 'showDetails']);
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
                    [DetailsInternetPage.getInstance().servicePath_]);
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
      // initialized in sendNetworkDetails.
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
     * Helper method called from showDetailedInfo and updateConnectionData.
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
     * Helper method called from showDetailedInfo and updateConnectionData.
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
        showViewAccount = this.showViewAccountButton_;
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
     * Helper method called from showDetailedInfo and updateConnectionData.
     * Updates the fields in the header section of the details frame.
     * @private
     */
    populateHeader_: function() {
      var onc = this.onc_;

      $('network-details-title').textContent = onc.getTranslatedValue('Name');
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
     * Helper method called from showDetailedInfo to initialize the Apn list.
     * @private
     */
    initializeApnList_: function() {
      var onc = this.onc_;

      var apnSelector = $('select-apn');
      // Clear APN lists, keep only last element that "other".
      while (apnSelector.length != 1) {
        apnSelector.remove(0);
      }
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
      var apnList = onc.getActiveValue('Cellular.APNList');
      for (var i = 0; i < apnList.length; i++) {
        var apnDict = apnList[i];
        var option = document.createElement('option');
        var localizedName = apnDict['LocalizedName'];
        var name = localizedName ? localizedName : apnDict['Name'];
        var accessPointName = apnDict['AccessPointName'];
        option.textContent =
            name ? (name + ' (' + accessPointName + ')') : accessPointName;
        option.value = i;
        // If this matches the active Apn, or LastGoodApn, set it as the
        // selected Apn.
        if ((activeApn == accessPointName &&
            activeUsername == apnDict['Username'] &&
            activePassword == apnDict['Password']) ||
            (!activeApn &&
            lastGoodApn == accessPointName &&
            lastGoodUsername == apnDict['Username'] &&
            lastGoodPassword == apnDict['Password'])) {
          this.selectedApnIndex_ = i;
        }
        // Insert new option before "other" option.
        apnSelector.add(option, otherOption);
      }
      if (this.selectedApnIndex_ == -1 && activeApn) {
        var activeOption = document.createElement('option');
        activeOption.textContent = activeApn;
        activeOption.value = -1;
        apnSelector.add(activeOption, otherOption);
        this.selectedApnIndex_ = apnSelector.length - 2;
        this.userApnIndex_ = this.selectedApnIndex_;
      }
      assert(this.selectedApnIndex_ >= 0);
      apnSelector.selectedIndex = this.selectedApnIndex_;
      updateHidden('.apn-list-view', false);
      updateHidden('.apn-details-view', true);
    },

    /**
     * Event Listener for the cellular-apn-use-default button.
     * @private
     */
    setDefaultApn_: function() {
      var onc = this.onc_;
      var apnSelector = $('select-apn');

      if (this.userApnIndex_ != -1) {
        apnSelector.remove(this.userApnIndex_);
        this.userApnIndex_ = -1;
      }

      var iApn = -1;
      var apnList = onc.getActiveValue('Cellular.APNList');
      if (apnList != undefined && apnList.length > 0) {
        iApn = 0;
        var defaultApn = apnList[iApn];
        var activeApn = {};
        activeApn['AccessPointName'] =
            stringFromValue(defaultApn['AccessPointName']);
        activeApn['Username'] = stringFromValue(defaultApn['Username']);
        activeApn['Password'] = stringFromValue(defaultApn['Password']);
        onc.setManagedProperty('Cellular.APN', activeApn);
        chrome.send('setApn', [this.servicePath_,
                               activeApn['AccessPointName'],
                               activeApn['Username'],
                               activeApn['Password']]);
      }
      apnSelector.selectedIndex = iApn;
      this.selectedApnIndex_ = iApn;

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

      var onc = this.onc_;
      var apnSelector = $('select-apn');

      var activeApn = {};
      activeApn['AccessPointName'] = stringFromValue(apnValue);
      activeApn['Username'] = stringFromValue($('cellular-apn-username').value);
      activeApn['Password'] = stringFromValue($('cellular-apn-password').value);
      onc.setManagedProperty('Cellular.APN', activeApn);
      this.userApn_ = activeApn;
      chrome.send('setApn', [this.servicePath_,
                             activeApn['AccessPointName'],
                             activeApn['Username'],
                             activeApn['Password']]);

      if (this.userApnIndex_ != -1) {
        apnSelector.remove(this.userApnIndex_);
        this.userApnIndex_ = -1;
      }

      var option = document.createElement('option');
      option.textContent = activeApn['AccessPointName'];
      option.value = -1;
      option.selected = true;
      apnSelector.add(option, apnSelector[apnSelector.length - 1]);
      this.userApnIndex_ = apnSelector.length - 2;
      this.selectedApnIndex_ = this.userApnIndex_;

      updateHidden('.apn-list-view', false);
      updateHidden('.apn-details-view', true);
    },

    /**
     * Event Listener for the cellular-apn-cancel button.
     * @private
     */
    cancelApn_: function() {
      $('select-apn').selectedIndex = this.selectedApnIndex_;
      updateHidden('.apn-list-view', false);
      updateHidden('.apn-details-view', true);
    },

    /**
     * Event Listener for the select-apn button.
     * @private
     */
    selectApn_: function() {
      var onc = this.onc_;
      var apnSelector = $('select-apn');
      var apnDict;
      if (apnSelector[apnSelector.selectedIndex].value != -1) {
        var apnList = onc.getActiveValue('Cellular.APNList');
        var apnIndex = apnSelector.selectedIndex;
        assert(apnIndex < apnList.length);
        apnDict = apnList[apnIndex];
        chrome.send('setApn', [this.servicePath_,
                               stringFromValue(apnDict['AccessPointName']),
                               stringFromValue(apnDict['Username']),
                               stringFromValue(apnDict['Password'])]);
        this.selectedApnIndex_ = apnIndex;
      } else if (apnSelector.selectedIndex == this.userApnIndex_) {
        apnDict = this.userApn_;
        chrome.send('setApn', [this.servicePath_,
                               stringFromValue(apnDict['AccessPointName']),
                               stringFromValue(apnDict['Username']),
                               stringFromValue(apnDict['Password'])]);
        this.selectedApnIndex_ = apnSelector.selectedIndex;
      } else {
        $('cellular-apn').value =
            stringFromValue(onc.getActiveValue('Cellular.APN.AccessPointName'));
        $('cellular-apn-username').value =
            stringFromValue(onc.getActiveValue('Cellular.APN.Username'));
        $('cellular-apn-password').value =
            stringFromValue(onc.getActiveValue('Cellular.APN.Password'));
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
    chrome.send('setCarrier', [
      DetailsInternetPage.getInstance().servicePath_, carrier]);
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
    // TODO(stevenjb): chrome.networkingPrivate.disableNetworkType
    chrome.send('startConnect', [detailsPage.servicePath_]);
    PageManager.closeOverlay();
  };

  DetailsInternetPage.disconnectNetwork = function() {
    var detailsPage = DetailsInternetPage.getInstance();
    if (detailsPage.type_ == 'WiFi')
      sendChromeMetricsAction('Options_NetworkDisconnectWifi');
    else if (detailsPage.type_ == 'VPN')
      sendChromeMetricsAction('Options_NetworkDisconnectVPN');
    // TODO(stevenjb): chrome.networkingPrivate.startDisconnect
    chrome.send('startDisconnect', [detailsPage.servicePath_]);
    PageManager.closeOverlay();
  };

  DetailsInternetPage.configureNetwork = function() {
    var detailsPage = DetailsInternetPage.getInstance();
    chrome.send('networkCommand',
                [detailsPage.type_, detailsPage.servicePath_, 'configure']);
    PageManager.closeOverlay();
  };

  DetailsInternetPage.activateFromDetails = function() {
    var detailsPage = DetailsInternetPage.getInstance();
    if (detailsPage.type_ == 'Cellular') {
      chrome.send('networkCommand',
                  [detailsPage.type_, detailsPage.servicePath_, 'activate']);
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
    var servicePath = detailsPage.servicePath_;
    if (type == 'WiFi') {
      sendCheckedIfEnabled(servicePath,
                           'setPreferNetwork',
                           'prefer-network-wifi',
                           'Options_NetworkSetPrefer');
      sendCheckedIfEnabled(servicePath,
                           'setAutoConnect',
                           'auto-connect-network-wifi',
                           'Options_NetworkAutoConnect');
    } else if (type == 'WiMAX') {
      sendCheckedIfEnabled(servicePath,
                           'setAutoConnect',
                           'auto-connect-network-wimax',
                           'Options_NetworkAutoConnect');
    } else if (type == 'Cellular') {
      sendCheckedIfEnabled(servicePath,
                           'setAutoConnect',
                           'auto-connect-network-cellular',
                           'Options_NetworkAutoConnect');
    } else if (type == 'VPN') {
      chrome.send('setServerHostname',
                  [servicePath, $('inet-server-hostname').value]);
      sendCheckedIfEnabled(servicePath,
                           'setAutoConnect',
                           'auto-connect-network-vpn',
                           'Options_NetworkAutoConnect');
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

    userNameServers = userNameServers.sort();
    chrome.send('setIPConfig',
                [servicePath,
                 Boolean($('ip-automatic-configuration-checkbox').checked),
                 $('ip-address').model.value || '',
                 $('ip-netmask').model.value || '',
                 $('ip-gateway').model.value || '',
                 nameServerType,
                 userNameServers.join(',')]);
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
   * Method called from Chrome with a dictionary of non ONC configuration
   * properties, including the HUID and service path to be used for requesting
   * the ONC properties. Note: currently GUID is only used to confirm that the
   * selected network still exists. It will be used instead of servicePath
   * once switching to the networkingPrivate API (see TODO below).
   * @param {InternetDetailedInfo} info
   */
  DetailsInternetPage.showDetailedInfo = function(info) {
    if (!('GUID' in info)) {
      // No network was found for, close the overlay.
      PageManager.closeOverlay();
      return;
    }
    var detailsPage = DetailsInternetPage.getInstance();
    detailsPage.servicePath_ = info.servicePath;
    detailsPage.showCarrierSelect_ = info.showCarrierSelect;
    detailsPage.showViewAccountButton_ = info.showViewAccountButton;
    // Ask Chrome to call sendNetworkDetails with the ONC properties.
    // TODO(stevenjb): Use networkingPrivate.getManagedProperties(info.guid).
    chrome.send('getManagedProperties', [info.servicePath]);
  };

  /**
   * Method called from Chrome when the ONC properties for the displayed
   * network may have changed.
   * @param {Object} update The updated ONC dictionary for the network.
   */
  DetailsInternetPage.updateConnectionData = function(oncData) {
    var detailsPage = DetailsInternetPage.getInstance();
    if (!detailsPage.visible)
      return;

    if (oncData.servicePath != detailsPage.servicePath_)
      return;

    // Update our cached data object.
    detailsPage.onc_ = new OncData(oncData);

    detailsPage.populateHeader_();
    detailsPage.updateConnectionButtonVisibilty_();
    detailsPage.updateDetails_();
  };

  /**
   * Method called from Chrome when the initial dictionary of ONC configuration
   * properties is available.
   * @param {Object} oncData Dictionary of ONC properties.
   */
  DetailsInternetPage.sendNetworkDetails = function(oncData) {
    var onc = new OncData(oncData);

    var detailsPage = DetailsInternetPage.getInstance();
    detailsPage.onc_ = onc;
    var type = onc.getActiveValue('Type');
    detailsPage.type_ = type;

    sendShowDetailsMetrics(type, onc.getActiveValue('ConnectionState'));

    detailsPage.populateHeader_();
    detailsPage.updateConnectionButtonVisibilty_();
    detailsPage.updateDetails_();

    // TODO(stevenjb): Some of the setup below should be moved to
    // updateDetails_() so that updates are reflected in the UI.

    // Only show proxy for remembered networks.
    var remembered = onc.getSource() != 'None';
    if (remembered) {
      detailsPage.showProxy_ = true;
      // Inform Chrome which network to use for proxy configuration.
      chrome.send('selectNetwork', [detailsPage.servicePath_]);
    } else {
      detailsPage.showProxy_ = false;
    }

    $('web-proxy-auto-discovery').hidden = true;

    var restricted = onc.getActiveValue('RestrictedConnectivity');
    var restrictedString = loadTimeData.getString(
        restricted ? 'restrictedYes' : 'restrictedNo');

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

    // Override the "automatic" values with the real saved DHCP values,
    // if they are set.
    var savedNameServersString;
    var savedIpAddress = onc.getActiveValue('SavedIPConfig.IPAddress');
    if (savedIpAddress != undefined) {
      inetAddress.automatic = savedIpAddress;
      inetAddress.value = savedIpAddress;
    }
    var savedPrefix = onc.getActiveValue('SavedIPConfig.RoutingPrefix');
    if (savedPrefix != undefined) {
      var savedNetmask = prefixLengthToNetmask(savedPrefix);
      inetNetmask.automatic = savedNetmask;
      inetNetmask.value = savedNetmask;
    }
    var savedGateway = onc.getActiveValue('SavedIPConfig.Gateway');
    if (savedGateway != undefined) {
      inetGateway.automatic = savedGateway;
      inetGateway.value = savedGateway;
    }
    var savedNameServers = onc.getActiveValue('SavedIPConfig.NameServers');
    if (savedNameServers) {
      savedNameServers = savedNameServers.sort();
      savedNameServersString = savedNameServers.join(',');
    }

    var ipAutoConfig = 'automatic';

    var staticNameServersString;
    var staticIpAddress = onc.getActiveValue('StaticIPConfig.IPAddress');
    if (staticIpAddress != undefined) {
      ipAutoConfig = 'user';
      inetAddress.user = staticIpAddress;
      inetAddress.value = staticIpAddress;
    }
    var staticPrefix = onc.getActiveValue('StaticIPConfig.RoutingPrefix');
    if (staticPrefix != undefined) {
      var staticNetmask = prefixLengthToNetmask(staticPrefix);
      inetNetmask.user = staticNetmask;
      inetNetmask.value = staticNetmask;
    }
    var staticGateway = onc.getActiveValue('StaticIPConfig.Gateway');
    if (staticGateway != undefined) {
      inetGateway.user = staticGateway;
      inetGateway.value = staticGateway;
    }
    var staticNameServers = onc.getActiveValue('StaticIPConfig.NameServers');
    if (staticNameServers) {
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

    // Set Nameserver fields.
    var nameServerType = 'automatic';
    if (staticNameServersString &&
        staticNameServersString == inetNameServersString) {
      nameServerType = 'user';
    }
    if (inetNameServersString == GoogleNameServersString)
      nameServerType = 'google';

    $('automatic-dns-display').textContent = inetNameServersString;
    $('google-dns-display').textContent = GoogleNameServersString;

    var nameServersUser = [];
    if (staticNameServers)
      nameServersUser = staticNameServers;

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
      if (this.showCarrierSelect_) {
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

      $('network-technology').textContent =
          onc.getActiveValue('Cellular.NetworkTechnology');
      $('roaming-state').textContent =
          onc.getTranslatedValue('Cellular.RoamingState');
      $('cellular-restricted-connectivity').textContent = restrictedString;
      // 'errorMessage' is a non ONC property added by Chrome.
      $('error-state').textContent = onc.getActiveValue('errorMessage');
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
      $('inet-service-name').textContent = networkName;
      $('inet-provider-type').textContent =
          onc.getTranslatedValue('VPN.Type');
      var providerType = onc.getActiveValue('VPN.Type');
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
    } else {
      OptionsPage.showTab($('internet-nav-tab'));
    }

    // Update controlled option indicators.
    var indicators = cr.doc.querySelectorAll(
        '#details-internet-page .controlled-setting-indicator');
    for (var i = 0; i < indicators.length; i++) {
      var managed = indicators[i].hasAttribute('managed');
      var attributeName = managed ? 'managed' : 'data';
      var propName = indicators[i].getAttribute(attributeName);
      if (!propName)
        continue;
      var propValue = managed ?
          onc.getManagedProperty(propName) :
          onc.getActiveValue(propName);
      if (propValue == undefined)
        continue;
      propValue = assertInstanceof(propValue, Object);
      var event;
      if (managed)
        event = detailsPage.createManagedEvent_(propName, propValue);
      else
        event = detailsPage.createControlledEvent_(propName,
            /** @type {{value: *, controlledBy: *, recommendedValue: *}} */(
                propValue));
      indicators[i].handlePrefChange(event);
      var forElement = $(indicators[i].getAttribute('for'));
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
