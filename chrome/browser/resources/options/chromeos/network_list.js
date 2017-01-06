// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Partial definition of the result of networkingPrivate.getProperties()).
 * TODO(stevenjb): Replace with chrome.networkingPrivate.NetworkStateProperties
 * once that is fully speced.
 * @typedef {{
 *   ConnectionState: string,
 *   Cellular: ?{
 *     Family: ?string,
 *     SIMPresent: ?boolean,
 *     SIMLockStatus: ?{ LockType: ?string },
 *     SupportNetworkScan: ?boolean
 *   },
 *   GUID: string,
 *   Name: string,
 *   Source: string,
 *   Type: string,
 *   VPN: ?{
 *     Type: string,
 *     ThirdPartyVPN: chrome.networkingPrivate.ThirdPartyVPNProperties
 *   }
 * }}
 * @see extensions/common/api/networking_private.idl
 */
var NetworkProperties;

/** @typedef {chrome.management.ExtensionInfo} */ var ExtensionInfo;

cr.define('options.network', function() {
  var ArrayDataModel = cr.ui.ArrayDataModel;
  var List = cr.ui.List;
  var ListItem = cr.ui.ListItem;
  var ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;
  var Menu = cr.ui.Menu;
  var MenuItem = cr.ui.MenuItem;
  var ControlledSettingIndicator = options.ControlledSettingIndicator;

  /**
   * Network settings constants. These enums usually match their C++
   * counterparts.
   */
  function Constants() {}

  /**
   * Valid network type names.
   */
  Constants.NETWORK_TYPES = ['Ethernet', 'WiFi', 'WiMAX', 'Cellular', 'VPN'];

  /**
   * Helper function to check whether |type| is a valid network type.
   * @param {string} type A string that may contain a valid network type.
   * @return {boolean} Whether the string represents a valid network type.
   */
  function isNetworkType(type) {
    return (Constants.NETWORK_TYPES.indexOf(type) != -1);
  }

  /**
   * Order in which controls are to appear in the network list sorted by key.
   */
  Constants.NETWORK_ORDER = ['Ethernet',
                             'WiFi',
                             'WiMAX',
                             'Cellular',
                             'VPN',
                             'addConnection'];

  /**
   * ID of the menu that is currently visible.
   * @type {?string}
   * @private
   */
  var activeMenu_ = null;

  /**
   * The state of the cellular device or undefined if not available.
   * @type {?chrome.networkingPrivate.DeviceStateProperties}
   * @private
   */
  var cellularDevice_ = null;

  /**
   * The active cellular network or null if none.
   * @type {?NetworkProperties}
   * @private
   */
  var cellularNetwork_ = null;

  /**
   * The active ethernet network or null if none.
   * @type {?NetworkProperties}
   * @private
   */
  var ethernetNetwork_ = null;

  /**
   * The state of the WiFi device or undefined if not available.
   * @type {string|undefined}
   * @private
   */
  var wifiDeviceState_ = undefined;

  /**
   * The state of the WiMAX device or undefined if not available.
   * @type {string|undefined}
   * @private
   */
  var wimaxDeviceState_ = undefined;

  /**
   * The current list of third-party VPN providers.
   * @type {!Array<!chrome.networkingPrivate.ThirdPartyVPNProperties>}}
   * @private
   */
  var vpnProviders_ = [];

  /**
   * Indicates if mobile data roaming is enabled.
   * @type {boolean}
   * @private
   */
  var enableDataRoaming_ = false;

  /**
   * Returns the display name for 'network'.
   * @param {NetworkProperties} data The network data dictionary.
   */
  function getNetworkName(data) {
    if (data.Type == 'Ethernet')
      return loadTimeData.getString('ethernetName');
    var name = data.Name;
    if (data.Type == 'VPN' && data.VPN && data.VPN.Type == 'ThirdPartyVPN' &&
        data.VPN.ThirdPartyVPN) {
      var providerName = data.VPN.ThirdPartyVPN.ProviderName;
      if (providerName)
        return loadTimeData.getStringF('vpnNameTemplate', providerName, name);
    }
    return name;
  }

  /**
   * Create an element in the network list for controlling network
   * connectivity.
   * @param {Object} data Description of the network list or command.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function NetworkListItem(data) {
    var el = cr.doc.createElement('li');
    el.data_ = {};
    for (var key in data)
      el.data_[key] = data[key];
    NetworkListItem.decorate(el);
    return el;
  }

  /**
   * @param {string} action An action to send to coreOptionsUserMetricsAction.
   */
  function sendChromeMetricsAction(action) {
    chrome.send('coreOptionsUserMetricsAction', [action]);
  }

  /**
   * @param {string} guid The network GUID.
   */
  function showDetails(guid) {
    chrome.networkingPrivate.getManagedProperties(
      guid, DetailsInternetPage.initializeDetailsPage);
  }

  /**
   * Decorate an element as a NetworkListItem.
   * @param {!Element} el The element to decorate.
   */
  NetworkListItem.decorate = function(el) {
    el.__proto__ = NetworkListItem.prototype;
    el.decorate();
  };

  NetworkListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * Description of the network group or control.
     * @type {Object<Object>}
     * @private
     */
    data_: null,

    /**
     * Element for the control's subtitle.
     * @type {?Element}
     * @private
     */
    subtitle_: null,

    /**
     * Div containing the list item icon.
     * @type {?Element}
     * @private
     */
    iconDiv_: null,

    /**
     * Description of the network control.
     * @type {Object}
     */
    get data() {
      return this.data_;
    },

    /**
     * Text label for the subtitle.
     * @type {string}
     */
    set subtitle(text) {
      if (text)
        this.subtitle_.textContent = text;
      this.subtitle_.hidden = !text;
    },

    /**
     * Sets the icon based on a network state object.
     * @param {!NetworkProperties} data Network state properties.
     */
    set iconData(data) {
      if (!isNetworkType(data.Type))
        return;
      var networkIcon = this.getNetworkIcon();
      networkIcon.networkState =
          /** @type {chrome.networkingPrivate.NetworkStateProperties} */ (data);
    },

    /**
     * Sets the icon based on a network type or a special type indecator, e.g.
     * 'add-connection'
     * @type {string}
     */
    set iconType(type) {
      if (isNetworkType(type)) {
        var networkIcon = this.getNetworkIcon();
        networkIcon.networkState = {
          GUID: '',
          Type: /** @type {CrOnc.Type} */ (type),
        };
      } else {
        // Special cases. e.g. 'add-connection'. Background images are
        // defined in browser_options.css.
        var oldIcon = /** @type {CrNetworkIconElement} */ (
            this.iconDiv_.querySelector('cr-network-icon'));
        if (oldIcon)
          this.iconDiv_.removeChild(oldIcon);
        this.iconDiv_.classList.add('network-' + type.toLowerCase());
      }
    },

    /**
     * Returns any existing network icon for the list item or creates a new one.
     * @return {!CrNetworkIconElement} The network icon for the list item.
     */
    getNetworkIcon: function() {
      var networkIcon = /** @type {CrNetworkIconElement} */ (
          this.iconDiv_.querySelector('cr-network-icon'));
      if (!networkIcon) {
        networkIcon = /** @type {!CrNetworkIconElement} */ (
            document.createElement('cr-network-icon'));
        networkIcon.isListItem = false;
        this.iconDiv_.appendChild(networkIcon);
      }
      return networkIcon;
    },

    /**
     * Set the direction of the text.
     * @param {string} direction The direction of the text, e.g. 'ltr'.
     */
    setSubtitleDirection: function(direction) {
      this.subtitle_.dir = direction;
    },

    /**
     * Indicate that the selector arrow should be shown.
     */
    showSelector: function() {
      this.subtitle_.classList.add('network-selector');
    },

    /**
     * Adds an indicator to show that the network is policy managed.
     */
    showManagedNetworkIndicator: function() {
      this.appendChild(new ManagedNetworkIndicator());
    },

    /** @override */
    decorate: function() {
      ListItem.prototype.decorate.call(this);
      this.className = 'network-group';
      this.iconDiv_ = this.ownerDocument.createElement('div');
      this.iconDiv_.className = 'network-icon';
      this.appendChild(this.iconDiv_);
      var textContent = this.ownerDocument.createElement('div');
      textContent.className = 'network-group-labels';
      this.appendChild(textContent);
      var categoryLabel = this.ownerDocument.createElement('div');
      var title;
      if (this.data_.key == 'addConnection')
        title = 'addConnectionTitle';
      else
        title = this.data_.key.toLowerCase() + 'Title';
      categoryLabel.className = 'network-title';
      categoryLabel.textContent = loadTimeData.getString(title);
      textContent.appendChild(categoryLabel);
      this.subtitle_ = this.ownerDocument.createElement('div');
      this.subtitle_.className = 'network-subtitle';
      textContent.appendChild(this.subtitle_);
    },
  };

  /**
   * Creates a control that displays a popup menu when clicked.
   * @param {Object} data  Description of the control.
   * @constructor
   * @extends {NetworkListItem}
   */
  function NetworkMenuItem(data) {
    var el = new NetworkListItem(data);
    el.__proto__ = NetworkMenuItem.prototype;
    el.decorate();
    return el;
  }

  NetworkMenuItem.prototype = {
    __proto__: NetworkListItem.prototype,

    /**
     * Popup menu element.
     * @type {?Element}
     * @private
     */
    menu_: null,

    /** @override */
    decorate: function() {
      this.subtitle = null;
      if (this.data.iconType)
        this.iconType = this.data.iconType;
      this.addEventListener('click', (function() {
        this.showMenu();
      }).bind(this));
    },

    /**
     * Retrieves the ID for the menu.
     */
    getMenuName: function() {
      return this.data_.key.toLowerCase() + '-network-menu';
    },

    /**
     * Creates a popup menu for the control.
     * @return {Element} The newly created menu.
     */
    createMenu: function() {
      if (this.data.menu) {
        var menu = this.ownerDocument.createElement('div');
        menu.id = this.getMenuName();
        menu.className = 'network-menu';
        menu.hidden = true;
        Menu.decorate(menu);
        menu.menuItemSelector = '.network-menu-item';
        for (var i = 0; i < this.data.menu.length; i++) {
          var entry = this.data.menu[i];
          createCallback_(menu, null, entry.label, entry.command);
        }
        return menu;
      }
      return null;
    },

    /**
     * Determines if a menu can be updated on the fly. Menus that cannot be
     * updated are fully regenerated using createMenu. The advantage of
     * updating a menu is that it can preserve ordering of networks avoiding
     * entries from jumping around after an update.
     * @return {boolean} Whether the menu can be updated on the fly.
     */
    canUpdateMenu: function() {
      return false;
    },

    /**
     * Removes the current menu contents, causing it to be regenerated when the
     * menu is shown the next time. If the menu is showing right now, its
     * contents are regenerated immediately and the menu remains visible.
     */
    refreshMenu: function() {
      this.menu_ = null;
      if (activeMenu_ == this.getMenuName())
        this.showMenu();
    },

    /**
     * Displays a popup menu.
     */
    showMenu: function() {
      var rebuild = false;
      // Force a rescan if opening the menu for WiFi networks to ensure the
      // list is up to date. Networks are periodically rescanned, but depending
      // on timing, there could be an excessive delay before the first rescan
      // unless forced.
      var rescan = !activeMenu_ && this.data_.key == 'WiFi';
      if (!this.menu_) {
        rebuild = true;
        var existing = $(this.getMenuName());
        if (existing) {
          if (this.canUpdateMenu() && this.updateMenu())
            return;
          closeMenu_();
        }
        this.menu_ = this.createMenu();
        this.menu_.addEventListener('mousedown', function(e) {
          // Prevent blurring of list, which would close the menu.
          e.preventDefault();
        });
        var parent = $('network-menus');
        if (existing)
          parent.replaceChild(this.menu_, existing);
        else
          parent.appendChild(this.menu_);
      }
      var top = this.offsetTop + this.clientHeight;
      var menuId = this.getMenuName();
      if (menuId != activeMenu_ || rebuild) {
        closeMenu_();
        activeMenu_ = menuId;
        this.menu_.style.setProperty('top', top + 'px');
        this.menu_.hidden = false;
      }
      if (rescan) {
        chrome.networkingPrivate.requestNetworkScan();
      }
    }
  };

  /**
   * Creates a control for selecting or configuring a network connection based
   * on the type of connection (e.g. wifi versus vpn).
   * @param {{key: string, networkList: Array<!NetworkProperties>}} data
   *     An object containing the network type (key) and an array of networks.
   * @constructor
   * @extends {NetworkMenuItem}
   */
  function NetworkSelectorItem(data) {
    var el = new NetworkMenuItem(data);
    el.__proto__ = NetworkSelectorItem.prototype;
    el.decorate();
    return el;
  }

  /**
   * Returns true if |source| is a policy managed source.
   * @param {string} source The ONC source of a network.
   * @return {boolean} Whether |source| is a managed source.
   */
  function isManaged(source) {
    return (source == 'DevicePolicy' || source == 'UserPolicy');
  }

  /**
   * Returns true if |network| is visible.
   * @param {!chrome.networkingPrivate.NetworkStateProperties} network The
   *     network state properties.
   * @return {boolean} Whether |network| is visible.
   */
  function networkIsVisible(network) {
    if (network.Type == 'WiFi')
      return !!(network.WiFi && (network.WiFi.SignalStrength > 0));
    if (network.Type == 'WiMAX')
      return !!(network.WiMAX && (network.WiMAX.SignalStrength > 0));
    // Other network types are always considered 'visible'.
    return true;
  }

  /**
   * Returns true if |cellular| is a GSM network with no sim present.
   * @param {?chrome.networkingPrivate.DeviceStateProperties} cellularDevice
   * @return {boolean} Whether |network| is missing a SIM card.
   */
  function isCellularSimAbsent(cellularDevice) {
    return !!cellularDevice && cellularDevice.SimPresent === false;
  }

  /**
   * Returns true if |cellular| has a locked SIM card.
   * @param {?chrome.networkingPrivate.DeviceStateProperties} cellularDevice
   * @return {boolean} Whether |network| has a locked SIM card.
   */
  function isCellularSimLocked(cellularDevice) {
    return !!cellularDevice && !!cellularDevice.SimLockType;
  }

  NetworkSelectorItem.prototype = {
    __proto__: NetworkMenuItem.prototype,

    /** @override */
    decorate: function() {
      // TODO(kevers): Generalize method of setting default label.
      this.subtitle = loadTimeData.getString('OncConnectionStateNotConnected');
      var list = this.data_.networkList;
      var candidateData = null;
      for (var i = 0; i < list.length; i++) {
        var networkDetails = list[i];
        if (networkDetails.ConnectionState == 'Connecting' ||
            networkDetails.ConnectionState == 'Connected') {
          this.subtitle = getNetworkName(networkDetails);
          this.setSubtitleDirection('ltr');
          candidateData = networkDetails;
          // Only break when we see a connecting network as it is possible to
          // have a connected network and a connecting network at the same
          // time.
          if (networkDetails.ConnectionState == 'Connecting')
            break;
        }
      }
      if (candidateData)
        this.iconData = candidateData;
      else
        this.iconType = this.data.key;

      this.showSelector();

      if (candidateData && isManaged(candidateData.Source))
        this.showManagedNetworkIndicator();

      if (activeMenu_ == this.getMenuName()) {
        // Menu is already showing and needs to be updated. Explicitly calling
        // show menu will force the existing menu to be replaced.  The call
        // is deferred in order to ensure that position of this element has
        // beem properly updated.
        var self = this;
        setTimeout(function() {self.showMenu();}, 0);
      }
    },

    /**
     * Creates a menu for selecting, configuring or disconnecting from a
     * network.
     * @return {!Element} The newly created menu.
     */
    createMenu: function() {
      var menu = this.ownerDocument.createElement('div');
      menu.id = this.getMenuName();
      menu.className = 'network-menu';
      menu.hidden = true;
      Menu.decorate(menu);
      menu.menuItemSelector = '.network-menu-item';
      var addendum = [];
      if (this.data_.key == 'WiFi') {
        var item = {
          label: loadTimeData.getString('joinOtherNetwork'),
          data: {}
        };
        if (allowUnmanagedNetworks_()) {
          item.command = createAddNonVPNConnectionCallback_('WiFi');
        } else {
          item.command = null;
          item.tooltip = loadTimeData.getString('prohibitedNetworkOther');
        }
        addendum.push(item);
      } else if (this.data_.key == 'Cellular') {
        if (cellularDevice_.State == 'Enabled' &&
            cellularNetwork_ && cellularNetwork_.Cellular &&
            cellularNetwork_.Cellular.SupportNetworkScan) {
          addendum.push({
            label: loadTimeData.getString('otherCellularNetworks'),
            command: createAddNonVPNConnectionCallback_('Cellular'),
            addClass: ['other-cellulars'],
            data: {}
          });
        }

        var label = enableDataRoaming_ ? 'disableDataRoaming' :
            'enableDataRoaming';
        var disabled = !loadTimeData.getValue('loggedInAsOwner');
        var entry = {label: loadTimeData.getString(label),
                     data: {}};
        if (disabled) {
          entry.command = null;
          entry.tooltip =
              loadTimeData.getString('dataRoamingDisableToggleTooltip');
        } else {
          var self = this;
          entry.command = function() {
            options.Preferences.setBooleanPref(
                'cros.signed.data_roaming_enabled',
                !enableDataRoaming_, true);
            // Force revalidation of the menu the next time it is displayed.
            self.menu_ = null;
          };
        }
        addendum.push(entry);
      } else if (this.data_.key == 'VPN') {
        addendum = addendum.concat(createAddVPNConnectionEntries_());
      }

      var list = this.data.rememberedNetworks;
      if (list && list.length > 0) {
        var callback = function(list) {
          $('remembered-network-list').clear();
          var dialog = options.PreferredNetworks.getInstance();
          PageManager.showPageByName('preferredNetworksPage', false);
          dialog.update(list);
          sendChromeMetricsAction('Options_NetworkShowPreferred');
        };
        addendum.push({label: loadTimeData.getString('preferredNetworks'),
                       command: callback,
                       data: list});
      }

      var networkGroup = this.ownerDocument.createElement('div');
      networkGroup.className = 'network-menu-group';
      list = this.data.networkList;
      var empty = !list || list.length == 0;
      if (list) {
        var connectedVpnGuid = '';
        for (var i = 0; i < list.length; i++) {
          var data = list[i];
          this.createNetworkOptionsCallback_(networkGroup, data);
          // For VPN only, append a 'Disconnect' item to the dropdown menu.
          if (!connectedVpnGuid && data.Type == 'VPN' &&
              (data.ConnectionState == 'Connected' ||
               data.ConnectionState == 'Connecting')) {
            connectedVpnGuid = data.GUID;
          }
        }
        if (connectedVpnGuid) {
          var disconnectCallback = function() {
            sendChromeMetricsAction('Options_NetworkDisconnectVPN');
            chrome.networkingPrivate.startDisconnect(connectedVpnGuid);
          };
          // Add separator
          addendum.push({});
          addendum.push({label: loadTimeData.getString('disconnectNetwork'),
                         command: disconnectCallback,
                         data: data});
        }
      }
      if (this.data_.key == 'WiFi' || this.data_.key == 'WiMAX' ||
          this.data_.key == 'Cellular') {
        addendum.push({});
        if (this.data_.key == 'WiFi') {
          addendum.push({
            label: loadTimeData.getString('turnOffWifi'),
            command: function() {
              sendChromeMetricsAction('Options_NetworkWifiToggle');
              chrome.networkingPrivate.disableNetworkType(
                  chrome.networkingPrivate.NetworkType.WI_FI);
            },
            data: {}});
        } else if (this.data_.key == 'WiMAX') {
          addendum.push({
            label: loadTimeData.getString('turnOffWimax'),
            command: function() {
              chrome.networkingPrivate.disableNetworkType(
                  chrome.networkingPrivate.NetworkType.WI_MAX);
            },
            data: {}});
        } else if (this.data_.key == 'Cellular') {
          addendum.push({
            label: loadTimeData.getString('turnOffCellular'),
            command: function() {
              chrome.networkingPrivate.disableNetworkType(
                  chrome.networkingPrivate.NetworkType.CELLULAR);
            },
            data: {}});
        }
      }
      if (!empty)
        menu.appendChild(networkGroup);
      if (addendum.length > 0) {
        var separator = false;
        if (!empty) {
          menu.appendChild(MenuItem.createSeparator());
          separator = true;
        }
        for (var i = 0; i < addendum.length; i++) {
          var value = addendum[i];
          if (value.data) {
            var item = createCallback_(menu, value.data, value.label,
                                       value.command);
            if (value.tooltip)
              item.title = value.tooltip;
            if (value.addClass)
              item.classList.add(value.addClass);
            separator = false;
          } else if (!separator) {
            menu.appendChild(MenuItem.createSeparator());
            separator = true;
          }
        }
      }
      return menu;
    },

    /** @override */
    canUpdateMenu: function() {
      return this.data_.key == 'WiFi' && activeMenu_ == this.getMenuName();
    },

    /**
     * Updates an existing menu.  Updated menus preserve ordering of prior
     * entries.  During the update process, the ordering may differ from the
     * preferred ordering as determined by the network library.  If the
     * ordering becomes potentially out of sync, then the updated menu is
     * marked for disposal on close.  Reopening the menu will force a
     * regeneration, which will in turn fix the ordering. This method must only
     * be called if canUpdateMenu() returned |true|.
     * @return {boolean} True if successfully updated.
     */
    updateMenu: function() {
      var oldMenu = $(this.getMenuName());
      var group = oldMenu.getElementsByClassName('network-menu-group')[0];
      if (!group)
        return false;
      var newMenu = this.createMenu();
      var discardOnClose = false;
      var oldNetworkButtons = this.extractNetworkConnectButtons_(oldMenu);
      var newNetworkButtons = this.extractNetworkConnectButtons_(newMenu);
      for (var key in oldNetworkButtons) {
        if (newNetworkButtons[key]) {
          group.replaceChild(newNetworkButtons[key].button,
                             oldNetworkButtons[key].button);
          if (newNetworkButtons[key].index != oldNetworkButtons[key].index)
            discardOnClose = true;
          newNetworkButtons[key] = null;
        } else {
          // Leave item in list to prevent network items from jumping due to
          // deletions.
          oldNetworkButtons[key].disabled = true;
          discardOnClose = true;
        }
      }
      for (var key in newNetworkButtons) {
        var entry = newNetworkButtons[key];
        if (entry) {
          group.appendChild(entry.button);
          discardOnClose = true;
        }
      }
      oldMenu.data = {discardOnClose: discardOnClose};
      return true;
    },

    /**
     * Extracts a mapping of network names to menu element and position.
     * @param {!Element} menu The menu to process.
     * @return {Object<?{index: number, button: Element}>}
     *     Network mapping.
     * @private
     */
    extractNetworkConnectButtons_: function(menu) {
      var group = menu.getElementsByClassName('network-menu-group')[0];
      var networkButtons = {};
      if (!group)
        return networkButtons;
      var buttons = group.getElementsByClassName('network-menu-item');
      for (var i = 0; i < buttons.length; i++) {
        var label = buttons[i].data.label;
        networkButtons[label] = {index: i, button: buttons[i]};
      }
      return networkButtons;
    },

    /**
     * Adds a menu item for showing network details.
     * @param {!Element} parent The parent element.
     * @param {NetworkProperties} data Description of the network.
     * @private
     */
    createNetworkOptionsCallback_: function(parent, data) {
      var menuItem = null;
      if (data.Type == 'WiFi' && !allowUnmanagedNetworks_() &&
          !isManaged(data.Source)) {
        menuItem = createCallback_(parent,
                                   data,
                                   getNetworkName(data),
                                   null);
        menuItem.title = loadTimeData.getString('prohibitedNetwork');
      } else {
        menuItem = createCallback_(parent,
                                   data,
                                   getNetworkName(data),
                                   showDetails.bind(null, data.GUID));
      }
      if (isManaged(data.Source))
        menuItem.appendChild(new ManagedNetworkIndicator());
      if (data.ConnectionState == 'Connected' ||
          data.ConnectionState == 'Connecting') {
        var label = menuItem.getElementsByClassName(
            'network-menu-item-label')[0];
        label.classList.add('active-network');
      }
    }
  };

  /**
   * Creates a button-like control for configurating internet connectivity.
   * @param {{key: string, subtitle: string, command: Function}} data
   *     Description of the network control.
   * @constructor
   * @extends {NetworkListItem}
   */
  function NetworkButtonItem(data) {
    var el = new NetworkListItem(data);
    el.__proto__ = NetworkButtonItem.prototype;
    el.decorate();
    return el;
  }

  NetworkButtonItem.prototype = {
    __proto__: NetworkListItem.prototype,

    /** @override */
    decorate: function() {
      if (this.data.subtitle)
        this.subtitle = this.data.subtitle;
      else
       this.subtitle = null;
      if (this.data.command)
        this.addEventListener('click', this.data.command);
      if (this.data.iconData)
        this.iconData = this.data.iconData;
      else if (this.data.iconType)
        this.iconType = this.data.iconType;
      if (isManaged(this.data.Source))
        this.showManagedNetworkIndicator();
    },
  };

  /**
   * Adds a command to a menu for modifying network settings.
   * @param {!Element} menu Parent menu.
   * @param {?NetworkProperties} data Description of the network.
   * @param {!string} label Display name for the menu item.
   * @param {Function} command Callback function.
   * @return {!Element} The created menu item.
   * @private
   */
  function createCallback_(menu, data, label, command) {
    var button = menu.ownerDocument.createElement('div');
    button.className = 'network-menu-item';

    var buttonIconDiv = menu.ownerDocument.createElement('div');
    buttonIconDiv.className = 'network-icon';
    button.appendChild(buttonIconDiv);
    if (data && isNetworkType(data.Type)) {
      var networkIcon = /** @type {!CrNetworkIconElement} */ (
          document.createElement('cr-network-icon'));
      buttonIconDiv.appendChild(networkIcon);
      networkIcon.isListItem = true;
      networkIcon.networkState =
          /** @type {chrome.networkingPrivate.NetworkStateProperties} */ (data);
    }

    var buttonLabel = menu.ownerDocument.createElement('span');
    buttonLabel.className = 'network-menu-item-label';
    buttonLabel.textContent = label;
    button.appendChild(buttonLabel);
    var callback = null;
    if (command != null) {
      if (data) {
        callback = function() {
          (/** @type {Function} */(command))(data);
          closeMenu_();
        };
      } else {
        callback = function() {
          (/** @type {Function} */(command))();
          closeMenu_();
        };
      }
    }
    if (callback != null)
      button.addEventListener('activate', callback);
    else
      buttonLabel.classList.add('network-disabled-control');

    button.data = {label: label};
    MenuItem.decorate(button);
    menu.appendChild(button);
    return button;
  }

  /**
   * A list of controls for manipulating network connectivity.
   * @constructor
   * @extends {cr.ui.List}
   */
  var NetworkList = cr.ui.define('list');

  NetworkList.prototype = {
    __proto__: List.prototype,

    /** @override */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.startBatchUpdates();
      this.autoExpands = true;
      this.dataModel = new ArrayDataModel([]);
      this.selectionModel = new ListSingleSelectionModel();
      this.addEventListener('blur', this.onBlur_.bind(this));
      this.selectionModel.addEventListener('change',
                                           this.onSelectionChange_.bind(this));

      // Wi-Fi control is always visible.
      this.update({key: 'WiFi', networkList: []});

      this.updateAddConnectionMenuEntries_();

      var prefs = options.Preferences.getInstance();
      prefs.addEventListener('cros.signed.data_roaming_enabled',
          function(event) {
            enableDataRoaming_ = event.value.value;
          });
      this.endBatchUpdates();

      this.onNetworkListChanged_();  // Trigger an initial network update

      chrome.networkingPrivate.onNetworkListChanged.addListener(
          this.onNetworkListChanged_.bind(this));
      chrome.networkingPrivate.onDeviceStateListChanged.addListener(
          this.onNetworkListChanged_.bind(this));

      chrome.management.onInstalled.addListener(
          this.onExtensionAdded_.bind(this));
      chrome.management.onEnabled.addListener(
          this.onExtensionAdded_.bind(this));
      chrome.management.onUninstalled.addListener(
          this.onExtensionRemoved_.bind(this));
      chrome.management.onDisabled.addListener(function(extension) {
        this.onExtensionRemoved_(extension.id);
      }.bind(this));

      chrome.management.getAll(this.onGetAllExtensions_.bind(this));
      chrome.networkingPrivate.requestNetworkScan();
    },

    /**
     * networkingPrivate event called when the network list has changed.
     */
    onNetworkListChanged_: function() {
      var networkList = this;
      chrome.networkingPrivate.getDeviceStates(function(deviceStates) {
        var filter = {
          networkType: chrome.networkingPrivate.NetworkType.ALL
        };
        chrome.networkingPrivate.getNetworks(filter, function(networkStates) {
          networkList.updateNetworkStates(deviceStates, networkStates);
        });
      });
    },

    /**
     * chrome.management.getAll callback.
     * @param {!Array<!ExtensionInfo>} extensions
     * @private
     */
    onGetAllExtensions_: function(extensions) {
      vpnProviders_ = [];
      for (var extension of extensions)
        this.addVpnProvider_(extension);
    },

    /**
     * If |extension| is a third-party VPN provider, add it to vpnProviders_.
     * @param {!ExtensionInfo} extension
     * @private
     */
    addVpnProvider_: function(extension) {
      if (!extension.enabled ||
          extension.permissions.indexOf('vpnProvider') == -1) {
        return;
      }
      // Ensure that we haven't already added this provider, e.g. if
      // the onExtensionAdded_ callback gets invoked after onGetAllExtensions_
      // for an extension in the returned list.
      for (var provider of vpnProviders_) {
        if (provider.ExtensionID == extension.id)
          return;
      }
      var newProvider = {
        ExtensionID: extension.id,
        ProviderName: extension.name
      };
      vpnProviders_.push(newProvider);
      this.refreshVpnProviders_();
    },

    /**
     * chrome.management.onInstalled or onEnabled event.
     * @param {!ExtensionInfo} extension
     * @private
     */
    onExtensionAdded_: function(extension) {
      this.addVpnProvider_(extension);
    },

    /**
     * chrome.management.onUninstalled or onDisabled event.
     * @param {string} extensionId
     * @private
     */
    onExtensionRemoved_: function(extensionId) {
      for (var i = 0; i < vpnProviders_.length; ++i) {
        var provider = vpnProviders_[i];
        if (provider.ExtensionID == extensionId) {
          vpnProviders_.splice(i, 1);
          this.refreshVpnProviders_();
          break;
        }
      }
    },

    /**
     * Rebuilds the list of VPN providers.
     * @private
     */
    refreshVpnProviders_: function() {
      // Refresh the contents of the VPN menu.
      var index = this.indexOf('VPN');
      if (index != undefined)
        this.getListItemByIndex(index).refreshMenu();

      // Refresh the contents of the "add connection" menu.
      this.updateAddConnectionMenuEntries_();
      index = this.indexOf('addConnection');
      if (index != undefined)
        this.getListItemByIndex(index).refreshMenu();
    },

    /**
     * Updates the entries in the "add connection" menu, based on the VPN
     * providers currently enabled in the user's profile.
     * @private
     */
    updateAddConnectionMenuEntries_: function() {
      var entries = [{
        label: loadTimeData.getString('addConnectionWifi'),
        command: createAddNonVPNConnectionCallback_('WiFi')
      }];
      entries = entries.concat(createAddVPNConnectionEntries_());
      this.update({key: 'addConnection',
                   iconType: 'add-connection',
                   menu: entries
                  });
    },

    /**
     * When the list loses focus, unselect all items in the list and close the
     * active menu.
     * @private
     */
    onBlur_: function() {
      this.selectionModel.unselectAll();
      closeMenu_();
    },

    /** @override */
    handleKeyDown: function(e) {
      if (activeMenu_) {
        // keyIdentifier does not report 'Esc' correctly
        if (e.keyCode == 27 /* Esc */) {
          closeMenu_();
          return;
        }

        if ($(activeMenu_).handleKeyDown(e)) {
          e.preventDefault();
          e.stopPropagation();
        }
        return;
      }

      if (e.key == 'Enter' ||
          e.key == ' ' /* Space */) {
        var selectedListItem = this.getListItemByIndex(
            this.selectionModel.selectedIndex);
        if (selectedListItem) {
          selectedListItem.click();
          return;
        }
      }

      List.prototype.handleKeyDown.call(this, e);
    },

    /**
     * Close bubble and menu when a different list item is selected.
     * @param {Event} event Event detailing the selection change.
     * @private
     */
    onSelectionChange_: function(event) {
      PageManager.hideBubble();
      // A list item may temporarily become unselected while it is constructing
      // its menu. The menu should therefore only be closed if a different item
      // is selected, not when the menu's owner item is deselected.
      if (activeMenu_) {
        for (var i = 0; i < event.changes.length; ++i) {
          if (event.changes[i].selected) {
            var item = this.dataModel.item(event.changes[i].index);
            if (!item.getMenuName || item.getMenuName() != activeMenu_) {
              closeMenu_();
              return;
            }
          }
        }
      }
    },

    /**
     * Finds the index of a network item within the data model based on
     * category.
     * @param {string} key Unique key for the item in the list.
     * @return {(number|undefined)} The index of the network item, or
     *     |undefined| if it is not found.
     */
    indexOf: function(key) {
      var size = this.dataModel.length;
      for (var i = 0; i < size; i++) {
        var entry = this.dataModel.item(i);
        if (entry.key == key)
          return i;
      }
      return undefined;
    },

    /**
     * Updates a network control.
     * @param {Object} data Description of the entry.
     */
    update: function(data) {
      this.startBatchUpdates();
      var index = this.indexOf(data.key);
      if (index == undefined) {
        // Find reference position for adding the element.  We cannot hide
        // individual list elements, thus we need to conditionally add or
        // remove elements and cannot rely on any element having a fixed index.
        for (var i = 0; i < Constants.NETWORK_ORDER.length; i++) {
          if (data.key == Constants.NETWORK_ORDER[i]) {
            data.sortIndex = i;
            break;
          }
        }
        var referenceIndex = -1;
        for (var i = 0; i < this.dataModel.length; i++) {
          var entry = this.dataModel.item(i);
          if (entry.sortIndex < data.sortIndex)
            referenceIndex = i;
          else
            break;
        }
        if (referenceIndex == -1) {
          // Prepend to the start of the list.
          this.dataModel.splice(0, 0, data);
        } else if (referenceIndex == this.dataModel.length) {
          // Append to the end of the list.
          this.dataModel.push(data);
        } else {
          // Insert after the reference element.
          this.dataModel.splice(referenceIndex + 1, 0, data);
        }
      } else {
        var entry = this.dataModel.item(index);
        data.sortIndex = entry.sortIndex;
        this.dataModel.splice(index, 1, data);
      }
      this.endBatchUpdates();
    },

    /**
     * @override
     * @param {Object} entry
     */
    createItem: function(entry) {
      if (entry.networkList)
        return new NetworkSelectorItem(
            /** @type {{key: string, networkList: Array<!NetworkProperties>}} */
            (entry));
      if (entry.command)
        return new NetworkButtonItem(
            /** @type {{key: string, subtitle: string, command: Function}} */(
                entry));
      if (entry.menu)
        return new NetworkMenuItem(entry);
      assertNotReached();
    },

    /**
     * Deletes an element from the list.
     * @param {string} key  Unique identifier for the element.
     */
    deleteItem: function(key) {
      var index = this.indexOf(key);
      if (index != undefined)
        this.dataModel.splice(index, 1);
    },

    /**
     * Updates the state of network devices and services.
     * @param {!Array<!chrome.networkingPrivate.DeviceStateProperties>}
     *     deviceStates The result from networkingPrivate.getDeviceStates.
     * @param {!Array<!chrome.networkingPrivate.NetworkStateProperties>}
     *     networkStates The result from networkingPrivate.getNetworks.
     */
    updateNetworkStates: function(deviceStates, networkStates) {
      // Update device states.
      cellularDevice_ = null;
      wifiDeviceState_ = undefined;
      wimaxDeviceState_ = undefined;
      for (var i = 0; i < deviceStates.length; ++i) {
        var device = deviceStates[i];
        var type = device.Type;
        var state = device.State;
        if (type == 'Cellular')
          cellularDevice_ = cellularDevice_ || device;
        else if (type == 'WiFi')
          wifiDeviceState_ = wifiDeviceState_ || state;
        else if (type == 'WiMAX')
          wimaxDeviceState_ = wimaxDeviceState_ || state;
      }

      // Update active network states.
      cellularNetwork_ = null;
      ethernetNetwork_ = null;
      for (var i = 0; i < networkStates.length; i++) {
        // Note: This cast is valid since
        // networkingPrivate.NetworkStateProperties is a subset of
        // NetworkProperties and all missing properties are optional.
        var entry = /** @type {NetworkProperties} */ (networkStates[i]);
        switch (entry.Type) {
          case 'Cellular':
            cellularNetwork_ = cellularNetwork_ || entry;
            break;
          case 'Ethernet':
            // Ignore any EAP Parameters networks (which lack ConnectionState).
            if (entry.ConnectionState)
              ethernetNetwork_ = ethernetNetwork_ || entry;
            break;
        }
        if (cellularNetwork_ && ethernetNetwork_)
          break;
      }

      if (cellularNetwork_ && cellularNetwork_.GUID) {
        // Get the complete set of cellular properties which includes SIM and
        // Scan properties.
        var networkList = this;
        chrome.networkingPrivate.getProperties(
            cellularNetwork_.GUID, function(cellular) {
              cellularNetwork_ = /** @type {NetworkProperties} */ (cellular);
              networkList.updateControls(networkStates);
            });
      } else {
        this.updateControls(networkStates);
      }
    },

    /**
     * Updates network controls.
     * @param {!Array<!chrome.networkingPrivate.NetworkStateProperties>}
     *     networkStates The result from networkingPrivate.getNetworks.
     */
    updateControls: function(networkStates) {
      this.startBatchUpdates();

      // Only show Ethernet control if available.
      if (ethernetNetwork_) {
        var ethernetOptions = showDetails.bind(null, ethernetNetwork_.GUID);
        var state = ethernetNetwork_.ConnectionState;
        var subtitle;
        if (state == 'Connected')
          subtitle = loadTimeData.getString('OncConnectionStateConnected');
        else if (state == 'Connecting')
          subtitle = loadTimeData.getString('OncConnectionStateConnecting');
        else
          subtitle = loadTimeData.getString('OncConnectionStateNotConnected');
        this.update(
          { key: 'Ethernet',
            subtitle: subtitle,
            iconData: ethernetNetwork_,
            command: ethernetOptions,
            Source: ethernetNetwork_.Source }
        );
      } else {
        this.deleteItem('Ethernet');
      }

      if (wifiDeviceState_ == 'Enabled')
        loadData_('WiFi', networkStates);
      else if (wifiDeviceState_ ==
          chrome.networkingPrivate.DeviceStateType.PROHIBITED)
        setTechnologiesProhibited_(chrome.networkingPrivate.NetworkType.WI_FI);
      else
        addEnableNetworkButton_(chrome.networkingPrivate.NetworkType.WI_FI);

      // Only show cellular control if available.
      if (cellularDevice_) {
        if (cellularDevice_.State == 'Enabled' &&
            !isCellularSimAbsent(cellularDevice_) &&
            !isCellularSimLocked(cellularDevice_)) {
          loadData_('Cellular', networkStates);
        } else if (cellularDevice_.State ==
            chrome.networkingPrivate.DeviceStateType.PROHIBITED) {
          setTechnologiesProhibited_(
              chrome.networkingPrivate.NetworkType.CELLULAR);
        } else {
          addEnableNetworkButton_(
              chrome.networkingPrivate.NetworkType.CELLULAR);
        }
      } else {
        this.deleteItem('Cellular');
      }

      // Only show wimax control if available. Uses cellular icons.
      if (wimaxDeviceState_) {
        if (wimaxDeviceState_ == 'Enabled') {
          loadData_('WiMAX', networkStates);
        } else if (wimaxDeviceState_ ==
            chrome.networkingPrivate.DeviceStateType.PROHIBITED) {
          setTechnologiesProhibited_(
              chrome.networkingPrivate.NetworkType.WI_MAX);
        } else {
          addEnableNetworkButton_(chrome.networkingPrivate.NetworkType.WI_MAX);
        }
      } else {
        this.deleteItem('WiMAX');
      }

      // Only show VPN control if there is at least one VPN configured.
      if (loadData_('VPN', networkStates) == 0)
        this.deleteItem('VPN');

      this.endBatchUpdates();
    }
  };

  /**
   * Replaces a network menu with a button for enabling the network type.
   * @param {chrome.networkingPrivate.NetworkType} type
   * @private
   */
  function addEnableNetworkButton_(type) {
    var subtitle = loadTimeData.getString('networkDisabled');
    var enableNetwork = function() {
      if (type == chrome.networkingPrivate.NetworkType.WI_FI)
        sendChromeMetricsAction('Options_NetworkWifiToggle');
      if (type == chrome.networkingPrivate.NetworkType.CELLULAR) {
        if (isCellularSimLocked(cellularDevice_)) {
          chrome.send('simOperation', ['unlock']);
          return;
        } else if (isCellularSimAbsent(cellularDevice_)) {
          chrome.send('simOperation', ['configure']);
          return;
        }
      }
      chrome.networkingPrivate.enableNetworkType(type);
    };
    $('network-list').update({key: type,
                              subtitle: subtitle,
                              iconType: type,
                              command: enableNetwork});
  }

  /**
   * Replaces a network menu with a button with nothing to do.
   * @param {!chrome.networkingPrivate.NetworkType} type
   * @private
   */
  function setTechnologiesProhibited_(type) {
    var subtitle = loadTimeData.getString('networkProhibited');
    var doNothingButRemoveClickShadow = function() {
      this.removeAttribute('lead');
      this.removeAttribute('selected');
      this.parentNode.removeAttribute('has-element-focus');
    };
    $('network-list').update({key: type,
                              subtitle: subtitle,
                              iconType: type,
                              command: doNothingButRemoveClickShadow});
  }

  /**
   * Element for indicating a policy managed network.
   * @constructor
   * @extends {options.ControlledSettingIndicator}
   */
  function ManagedNetworkIndicator() {
    var el = cr.doc.createElement('span');
    el.__proto__ = ManagedNetworkIndicator.prototype;
    el.decorate();
    return el;
  }

  ManagedNetworkIndicator.prototype = {
    __proto__: ControlledSettingIndicator.prototype,

    /** @override */
    decorate: function() {
      ControlledSettingIndicator.prototype.decorate.call(this);
      this.controlledBy = 'policy';
      var policyLabel = loadTimeData.getString('managedNetwork');
      this.setAttribute('textPolicy', policyLabel);
      this.removeAttribute('tabindex');
    },

    /** @override */
    handleEvent: function(event) {
      // Prevent focus blurring as that would close any currently open menu.
      if (event.type == 'mousedown')
        return;
      ControlledSettingIndicator.prototype.handleEvent.call(this, event);
    },

    /**
     * Handle mouse events received by the bubble, preventing focus blurring as
     * that would close any currently open menu and preventing propagation to
     * any elements located behind the bubble.
     * @param {Event} event Mouse event.
     */
    stopEvent: function(event) {
      event.preventDefault();
      event.stopPropagation();
    },

    /** @override */
    toggleBubble: function() {
      if (activeMenu_ && !$(activeMenu_).contains(this))
        closeMenu_();
      ControlledSettingIndicator.prototype.toggleBubble.call(this);
      if (this.showingBubble) {
        var bubble = PageManager.getVisibleBubble();
        bubble.addEventListener('mousedown', this.stopEvent);
        bubble.addEventListener('click', this.stopEvent);
      }
    }
  };

  /**
   * Updates the list of available networks and their status, filtered by
   * network type.
   * @param {string} type The type of network.
   * @param {Array<!chrome.networkingPrivate.NetworkStateProperties>} networks
   *     The list of network objects.
   * @return {number} The number of visible networks matching |type|.
   */
  function loadData_(type, networks) {
    var res = 0;
    var availableNetworks = [];
    var rememberedNetworks = [];
    for (var i = 0; i < networks.length; i++) {
      var network = networks[i];
      if (network.Type != type)
        continue;
      if (networkIsVisible(network)) {
        availableNetworks.push(network);
        ++res;
      }
      if ((type == 'WiFi' || type == 'VPN') && network.Source &&
          network.Source != 'None') {
        rememberedNetworks.push(network);
      }
    }
    var data = {
      key: type,
      networkList: availableNetworks,
      rememberedNetworks: rememberedNetworks
    };
    $('network-list').update(data);
    return res;
  }

  /**
   * Hides the currently visible menu.
   * @private
   */
  function closeMenu_() {
    if (activeMenu_) {
      var menu = $(activeMenu_);
      menu.hidden = true;
      if (menu.data && menu.data.discardOnClose)
        menu.parentNode.removeChild(menu);
      activeMenu_ = null;
    }
  }

  /**
   * Creates a callback function that adds a new connection of the given type.
   * This method may be used for all network types except VPN.
   * @param {string} type An ONC network type
   * @return {function()} The created callback.
   * @private
   */
  function createAddNonVPNConnectionCallback_(type) {
    return function() {
      if (type == 'WiFi')
        sendChromeMetricsAction('Options_NetworkJoinOtherWifi');
      chrome.send('addNonVPNConnection', [type]);
    };
  }

  /**
   * Creates a callback function that shows the "add network" dialog for a VPN
   * provider. If |opt_extensionID| is omitted, the dialog for the built-in
   * OpenVPN/L2TP provider is shown. Otherwise, |opt_extensionID| identifies the
   * third-party provider for which the dialog should be shown.
   * @param {string=} opt_extensionID Extension ID identifying the third-party
   *     VPN provider for which the dialog should be shown.
   * @return {function()} The created callback.
   * @private
   */
  function createVPNConnectionCallback_(opt_extensionID) {
    return function() {
      sendChromeMetricsAction(opt_extensionID ?
          'Options_NetworkAddVPNThirdParty' :
          'Options_NetworkAddVPNBuiltIn');
      chrome.send('addVPNConnection',
                  opt_extensionID ? [opt_extensionID] : undefined);
    };
  }

  /**
   * Generates an "add network" entry for each VPN provider currently enabled in
   * the user's profile.
   * @return {!Array<{label: string, command: function(), data: !Object}>} The
   *     list of entries.
   * @private
   */
  function createAddVPNConnectionEntries_() {
    var entries = [];
    for (var i = 0; i < vpnProviders_.length; ++i) {
      var provider = vpnProviders_[i];
      entries.push({
        label: loadTimeData.getStringF('addConnectionVPNTemplate',
                                       provider.ProviderName),
        command: createVPNConnectionCallback_(provider.ExtensionID),
        data: {}
      });
    }
    // Add an entry for the built-in OpenVPN/L2TP provider.
    entries.push({
      label: loadTimeData.getString('vpnBuiltInProvider'),
      command: createVPNConnectionCallback_(),
      data: {}
    });
    return entries;
  }

  /**
   * Return whether connecting to or viewing unmanaged networks is allowed.
   * @private
   */
  function allowUnmanagedNetworks_() {
    if (loadTimeData.valueExists('allowOnlyPolicyNetworksToConnect') &&
        loadTimeData.getBoolean('allowOnlyPolicyNetworksToConnect')) {
      return false;
    }
    return true;
  }

  /**
   * Whether the Network list is disabled. Only used for display purpose.
   */
  cr.defineProperty(NetworkList, 'disabled', cr.PropertyKind.BOOL_ATTR);

  // Export
  return {
    NetworkList: NetworkList
  };
});
