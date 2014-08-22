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

  var GetManagedTypes = {
    ACTIVE: 0,
    TRANSLATED: 1,
    RECOMMENDED: 2
  };

  /**
   * Gets the value of a property from a dictionary |data| that includes ONC
   * managed properties, e.g. getManagedValue(data, 'Name'). See notes for
   * getManagedProperty.
   * @param {object} data The properties dictionary.
   * @param {string} key The property key.
   * @param {string} type (Optional) The type of property to get as defined in
   *                      GetManagedTypes:
   *                      'ACTIVE' (default)  - gets the active value
   *                      'TRANSLATED' - gets the traslated or active value
   *                      'RECOMMENDED' - gets the recommended value
   * @return {*} The property value or undefined.
   */
  function getManagedValue(data, key, type) {
    var property = getManagedProperty(data, key);
    if (typeof property != 'object')
      return property;
    if (type == GetManagedTypes.RECOMMENDED)
      return getRecommendedValue(property);
    if (type == GetManagedTypes.TRANSLATED && 'Translated' in property)
      return property['Translated'];
    // Otherwise get the Active value (defalt behavior).
    if ('Active' in property)
      return property['Active'];
    // If no Active value is defined, return the effective value.
    return getEffectiveValue(property);
  }

  /**
   * Get the recommended value from a Managed property ONC dictionary.
   * @param {object} property The managed property ONC dictionary.
   * @return {*} the effective value or undefined.
   */
  function getRecommendedValue(property) {
    if (property['UserEditable'])
      return property['UserPolicy'];
    if (property['DeviceEditable'])
      return property['DevicePolicy'];
    // No value recommended by policy.
    return undefined;
  }

  /**
   * Get the effective value from a Managed property ONC dictionary.
   * @param {object} property The managed property ONC dictionary.
   * @return {*} The effective value or undefined.
   */
  function getEffectiveValue(property) {
    if ('Effective' in property) {
      var effective = property.Effective;
      if (effective in property)
        return property[effective];
    }
    return undefined;
  }

  /**
   * Gets either a managed property dictionary or an unmanaged value from
   * dictionary |data| that includes ONC managed properties. This supports
   * nested dictionaries, e.g. getManagedProperty(data, 'VPN.Type').
   * @param {object} data The properties dictionary.
   * @param {string} key The property key.
   * @return {*} The property value or dictionary if it exists, otherwise
   *             undefined.
   */
  function getManagedProperty(data, key) {
    while (true) {
      var index = key.indexOf('.');
      if (index < 0)
        break;
      var keyComponent = key.substr(0, index);
      if (!(keyComponent in data))
        return undefined;
      data = data[keyComponent];
      key = key.substr(index + 1);
    }
    return data[key];
  }

  /**
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
    if (data.type == 'Ethernet')
      return loadTimeData.getString('ethernetName');
    return getManagedValue(data, 'Name');
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
     * Creates an indicator event for controlled properties using
     * the same dictionary format as CoreOptionsHandler::CreateValueForPref.
     * @param {string} name The name for the Event.
     * @param {Object} data Property dictionary with |value|, |controlledBy|,
     *  and |recommendedValue| properties set.
     * @private
     */
    createControlledEvent_: function(name, propData) {
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
     * @param {Object} data ONC managed network property dictionary.
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
      // which case the value was modifed from the recommended value.
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
      // Only show ipconfig section if network is connected OR if nothing on
      // this device is connected. This is so that you can fix the ip configs
      // if you can't connect to any network.
      // TODO(chocobo): Once ipconfig is moved to flimflam service objects,
      //   we need to redo this logic to allow configuration of all networks.
      $('ipconfig-section').hidden = !this.connected && this.deviceConnected;
      $('ipconfig-dns-section').hidden =
        !this.connected && this.deviceConnected;

      // Network type related.
      updateHidden('#details-internet-page .cellular-details',
                   this.type != 'Cellular');
      updateHidden('#details-internet-page .wifi-details',
                   this.type != 'WiFi');
      updateHidden('#details-internet-page .wimax-details',
                   this.type != 'Wimax');
      updateHidden('#details-internet-page .vpn-details', this.type != 'VPN');
      updateHidden('#details-internet-page .proxy-details', !this.showProxy);

      // Cellular

      // Conditionally call updateHidden on .gsm-only, so that we don't unhide
      // a previously hidden element.
      if (this.gsm)
        updateHidden('#details-internet-page .cdma-only', true);
      else
        updateHidden('#details-internet-page .gsm-only', true);

      // Wifi

      // Hide network tab for VPN.
      updateHidden('#details-internet-page .network-details',
                   this.type == 'VPN');

      // Password and shared.
      updateHidden('#details-internet-page #password-details',
                   this.type != 'WiFi' || !this.hasSecurity);
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
    chrome.send('networkCommand', [data.type, servicePath, 'connect']);
    PageManager.closeOverlay();
  };

  DetailsInternetPage.disconnectNetwork = function() {
    var data = $('connection-state').data;
    var servicePath = data.servicePath;
    chrome.send('networkCommand', [data.type, servicePath, 'disconnect']);
    PageManager.closeOverlay();
  };

  DetailsInternetPage.configureNetwork = function() {
    var data = $('connection-state').data;
    var servicePath = data.servicePath;
    chrome.send('networkCommand', [data.type, servicePath, 'configure']);
    PageManager.closeOverlay();
  };

  DetailsInternetPage.activateFromDetails = function() {
    var data = $('connection-state').data;
    var servicePath = data.servicePath;
    if (data.Type == 'Cellular')
      chrome.send('networkCommand', [data.type, servicePath, 'activate']);
    PageManager.closeOverlay();
  };

  DetailsInternetPage.setDetails = function() {
    var data = $('connection-state').data;
    var servicePath = data.servicePath;
    if (data.type == 'WiFi') {
      sendCheckedIfEnabled(servicePath, 'setPreferNetwork',
                           $('prefer-network-wifi'));
      sendCheckedIfEnabled(servicePath, 'setAutoConnect',
                           $('auto-connect-network-wifi'));
    } else if (data.type == 'Wimax') {
      sendCheckedIfEnabled(servicePath, 'setAutoConnect',
                           $('auto-connect-network-wimax'));
    } else if (data.type == 'Cellular') {
      sendCheckedIfEnabled(servicePath, 'setAutoConnect',
                           $('auto-connect-network-cellular'));
    } else if (data.type == 'VPN') {
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

    var connectState = getManagedValue(data, 'ConnectionState');
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

    var connectable = getManagedValue(data, 'Connectable');
    if (connectState != 'Connected' &&
        (!connectable || this.hasSecurity ||
        (data.type == 'Wimax' || data.type == 'VPN'))) {
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

    var connectionState = getManagedValue(data, 'ConnectionState');
    var connectionStateString = networkOncStateString(connectionState);
    detailsPage.deviceConnected = data.deviceConnected;
    detailsPage.connected = connectionState == 'Connected';
    $('connection-state').textContent = connectionStateString;

    this.updateConnectionButtonVisibilty(data);

    if (data.type == 'WiFi') {
      $('wifi-connection-state').textContent = connectionStateString;
    } else if (data.type == 'Wimax') {
      $('wimax-connection-state').textContent = connectionStateString;
    } else if (data.type == 'Cellular') {
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

    data.type = getManagedValue(data, 'Type');  // Get Active Type value.

    // Populate header
    $('network-details-title').textContent = getNetworkName(data);
    var connectionState = getManagedValue(data, 'ConnectionState');
    var connectionStateString = networkOncStateString(connectionState);
    detailsPage.connected = connectionState == 'Connected';
    $('network-details-subtitle-status').textContent = connectionStateString;
    var typeKey = null;
    switch (data.type) {
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

    // TODO(stevenjb): Find a more appropriate place to cache data.
    $('connection-state').data = data;

    $('buyplan-details').hidden = true;
    $('activate-details').hidden = true;
    $('view-account-details').hidden = true;

    this.updateConnectionButtonVisibilty(data);

    $('web-proxy-auto-discovery').hidden = true;

    detailsPage.deviceConnected = data.deviceConnected;
    detailsPage.connected = connectionState == 'Connected';

    // Only show proxy for remembered networks.
    if (data.remembered) {
      detailsPage.showProxy = true;
      chrome.send('selectNetwork', [data.servicePath]);
    } else {
      detailsPage.showProxy = false;
    }
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

    var macAddress = getManagedValue(data, 'MacAddress');
    if (macAddress) {
      $('hardware-address').textContent = macAddress;
      $('hardware-address-row').style.display = 'table-row';
    } else {
      // This is most likely a device without a hardware address.
      $('hardware-address-row').style.display = 'none';
    }

    var setOrHideParent = function(field, property) {
      if (property) {
        $(field).textContent = property;
        $(field).parentElement.hidden = false;
      } else {
        $(field).parentElement.hidden = true;
      }
    };

    var networkName = getNetworkName(data);

    // Signal strength as percentage (for WiFi and Wimax).
    var signalStrength;
    if (data.type == 'WiFi' || data.type == 'Wimax') {
      signalStrength = getManagedValue(data, data.type + '.SignalStrength');
    }
    if (!signalStrength)
      signalStrength = 0;
    var strengthFormat = loadTimeData.getString('inetSignalStrengthFormat');
    var strengthString = strengthFormat.replace('$1', signalStrength);

    detailsPage.type = data.type;
    if (data.type == 'WiFi') {
      assert('WiFi' in data, 'WiFi network has no WiFi object' + networkName);
      OptionsPage.showTab($('wifi-network-nav-tab'));
      detailsPage.gsm = false;
      detailsPage.shared = data.shared;
      $('wifi-connection-state').textContent = connectionStateString;
      var ssid = getManagedValue(data, 'WiFi.SSID');
      $('wifi-ssid').textContent = ssid ? ssid : networkName;
      setOrHideParent('wifi-bssid', getManagedValue(data, 'WiFi.BSSID'));
      var security = getManagedValue(data, 'WiFi.Security');
      if (security == 'None')
        security = undefined;
      setOrHideParent('wifi-security', security);
      // Frequency is in MHz.
      var frequency = getManagedValue(data, 'WiFi.Frequency');
      if (!frequency)
        frequency = 0;
      var frequencyFormat = loadTimeData.getString('inetFrequencyFormat');
      frequencyFormat = frequencyFormat.replace('$1', frequency);
      $('wifi-frequency').textContent = frequencyFormat;
      $('wifi-signal-strength').textContent = strengthString;
      setOrHideParent('wifi-hardware-address',
                      getManagedValue(data, 'MacAddress'));
      detailsPage.showPreferred = data.remembered;
      var priority = getManagedValue(data, 'Priority');
      $('prefer-network-wifi').checked = priority > 0;
      $('prefer-network-wifi').disabled = !data.remembered;
      $('auto-connect-network-wifi').checked =
          getManagedValue(data, 'AutoConnect');
      $('auto-connect-network-wifi').disabled = !data.remembered;
      detailsPage.hasSecurity = security != undefined;
    } else if (data.type == 'Wimax') {
      assert('Wimax' in data,
             'Wimax network has no Wimax object' + networkName);
      OptionsPage.showTab($('wimax-network-nav-tab'));
      detailsPage.gsm = false;
      detailsPage.shared = data.shared;
      detailsPage.showPreferred = data.remembered;
      $('wimax-connection-state').textContent = connectionStateString;
      $('auto-connect-network-wimax').checked =
          getManagedValue(data, 'AutoConnect');
      $('auto-connect-network-wimax').disabled = !data.remembered;
      var identity;
      if (data.Wimax.EAP)
        identity = getManagedValue(data.Wimax.EAP, 'Identity');
      setOrHideParent('wimax-eap-identity', identity);
      $('wimax-signal-strength').textContent = strengthString;
    } else if (data.type == 'Cellular') {
      assert('Cellular' in data,
             'Cellular network has no Cellular object' + networkName);
      OptionsPage.showTab($('cellular-conn-nav-tab'));
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
        $('service-name').textContent = networkName;
      }

      $('network-technology').textContent =
          getManagedValue(data, 'Cellular.NetworkTechnology');
      $('activation-state').textContent = data.activationState;
      $('roaming-state').textContent = data.roamingState;
      $('restricted-pool').textContent = data.restrictedPool;
      $('error-state').textContent = data.errorMessage;
      $('manufacturer').textContent =
          getManagedValue(data, 'Cellular.Manufacturer');
      $('model-id').textContent = getManagedValue(data, 'Cellular.ModelID');
      $('firmware-revision').textContent =
          getManagedValue(data, 'Cellular.FirmwareRevision');
      $('hardware-revision').textContent =
          getManagedValue(data, 'Cellular.HardwareRevision');
      $('mdn').textContent = getManagedValue(data, 'Cellular.MDN');

      // Show ServingOperator properties only if available.
      var servingOperatorName =
          getManagedValue(data, 'Cellular.ServingOperator.Name');
      var servingOperatorCode =
          getManagedValue(data, 'Cellular.ServingOperator.Code');
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
      setOrHideParent('esn', getManagedValue(data, 'Cellular.ESN'));
      setOrHideParent('imei', getManagedValue(data, 'Cellular.IMEI'));
      setOrHideParent('meid', getManagedValue(data, 'Cellular.MEID'));
      setOrHideParent('min', getManagedValue(data, 'Cellular.MIN'));
      setOrHideParent('prl-version',
                      getManagedValue(data, 'Cellular.PRLVersion'));

      var family = getManagedValue(data, 'Cellular.Family');
      detailsPage.gsm = family == 'GSM';
      if (detailsPage.gsm) {
        $('iccid').textContent = getManagedValue(data, 'Cellular.ICCID');
        $('imsi').textContent = getManagedValue(data, 'Cellular.IMSI');

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
      $('auto-connect-network-cellular').checked =
          getManagedValue(data, 'AutoConnect');
      $('auto-connect-network-cellular').disabled = false;

      $('buyplan-details').hidden = !data.showBuyButton;
      $('view-account-details').hidden = !data.showViewAccountButton;
      $('activate-details').hidden = !data.showActivateButton;
      if (data.showActivateButton) {
        $('details-internet-login').hidden = true;
      }
    } else if (data.type == 'VPN') {
      OptionsPage.showTab($('vpn-nav-tab'));
      detailsPage.gsm = false;
      $('inet-service-name').textContent = networkName;
      $('inet-provider-type').textContent =
          getManagedValue(data, 'VPN.Type', GetManagedTypes.TRANSLATED);
      var providerType =
          getManagedValue(data, 'VPN.Type', GetManagedTypes.ACTIVE);
      var providerKey = 'VPN.' + providerType;
      $('inet-username').textContent =
          getManagedValue(data, providerKey + '.Username');
      var inetServerHostname = $('inet-server-hostname');
      inetServerHostname.value = getManagedValue(data, 'VPN.Host');
      inetServerHostname.resetHandler = function() {
        PageManager.hideBubble();
        var recommended =
            getManagedValue(data, 'VPN.Host', GetManagedTypes.RECOMMENDED);
        if (recommended != undefined)
          inetServerHostname.value = recommended;
      };
      $('auto-connect-network-vpn').checked =
          getManagedValue(data, 'AutoConnect');
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
      var propValue =
          managed ? getManagedProperty(data, propName) : data[propName];
      if (propValue == undefined)
        continue;
      var event;
      if (managed)
        event = detailsPage.createManagedEvent_(propName, propValue);
      else
        event = detailsPage.createControlledEvent_(propName, propValue);
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
