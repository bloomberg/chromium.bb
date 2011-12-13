// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.internet', function() {

  /**
   * Network settings constants. These enums usually match their C++
   * counterparts.
   */
  function Constants() {}
  // Minimum length for wireless network password.
  Constants.MIN_WIRELESS_PASSWORD_LENGTH = 5;
  // Minimum length for SSID name.
  Constants.MIN_WIRELESS_SSID_LENGTH = 1;
  // Cellular activation states:
  Constants.ACTIVATION_STATE_UNKNOWN             = 0;
  Constants.ACTIVATION_STATE_ACTIVATED           = 1;
  Constants.ACTIVATION_STATE_ACTIVATING          = 2;
  Constants.ACTIVATION_STATE_NOT_ACTIVATED       = 3;
  Constants.ACTIVATION_STATE_PARTIALLY_ACTIVATED = 4;
  // Network types:
  Constants.TYPE_UNKNOWN   = 0;
  Constants.TYPE_ETHERNET  = 1;
  Constants.TYPE_WIFI      = 2;
  Constants.TYPE_WIMAX     = 3;
  Constants.TYPE_BLUETOOTH = 4;
  Constants.TYPE_CELLULAR  = 5;
  Constants.TYPE_VPN       = 6;
  // ONC sources:
  Constants.ONC_SOURCE_USER_IMPORT   = 1;
  Constants.ONC_SOURCE_DEVICE_POLICY = 2;
  Constants.ONC_SOURCE_USER_POLICY   = 3;

  /**
   * Creates a new network list div.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var NetworkElement = cr.ui.define('div');

  NetworkElement.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.addEventListener('click', this.handleClick_);
    },

    /**
     * Loads given network list.
     * @param {Array} networks An array of network object.
     */
    load: function(networks) {
      this.textContent = '';

      for (var i = 0; i < networks.length; ++i) {
        this.appendChild(new NetworkItem(networks[i]));
      }
    },

    /**
     * Handles click on network list and triggers actions when clicked on
     * a NetworkListItem button.
     * @private
     * @param {!Event} e The click event object.
     */
    handleClick_: function(e) {
      // We shouldn't respond to click events selecting an input,
      // so return on those.
      if (e.target.tagName == 'INPUT') {
        return;
      }
      // Handle left button click
      if (e.button == 0) {
        var el = e.target;
        // If click is on action buttons of a network item.
        if (!(el.buttonType && el.networkType && el.servicePath)) {
          if (el.buttonType) {
            return;
          }
          // If click is on a network item or its label, walk up the DOM tree
          // to find the network item.
          var item = el;
          while (item && !item.data) {
            item = item.parentNode;
          }
          if (item.connecting)
            return;

          if (item) {
            var data = item.data;
            // Don't try to connect to Ethernet or unactivated Cellular.
            if (data && (data.networkType == 1 ||
                        (data.networkType == 5 && data.activation_state != 1)))
              return;
            // If clicked on other networks item.
            if (data && data.servicePath == '?') {
              chrome.send('buttonClickCallback',
                          [String(data.networkType),
                           data.servicePath,
                           'connect']);
            }
          }
        }
      }
    }
  };

  /**
   * Creates a new network item.
   * @param {Object} network The network this represents.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function NetworkItem(network) {
    var el = cr.doc.createElement('div');
    el.data = network;
    NetworkItem.decorate(el);
    return el;
  }


  /**
   * Decorates an element as a network item.
   * @param {!HTMLElement} el The element to decorate.
   */
  NetworkItem.decorate = function(el) {
    el.__proto__ = NetworkItem.prototype;
    el.decorate();
  };

  NetworkItem.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.className = 'network-item';
      this.connectable = this.data.connectable;
      this.connected = this.data.connected;
      this.connecting = this.data.connecting;
      this.other = this.data.servicePath == '?';
      this.id = this.data.servicePath;

      // Insert a div holding the policy-managed indicator.
      var policyIndicator = this.ownerDocument.createElement('div');
      policyIndicator.className = 'controlled-setting-indicator';
      cr.ui.decorate(policyIndicator, options.ControlledSettingIndicator);

      if (this.data.policyManaged) {
        policyIndicator.controlledBy = 'policy';
        policyIndicator.setAttribute('textPolicy',
                                     localStrings.getString('managedNetwork'));
      }
      this.appendChild(policyIndicator);

      // textDiv holds icon, name and status text.
      var textDiv = this.ownerDocument.createElement('div');
      textDiv.className = 'network-item-text';
      if (this.data.iconURL) {
        textDiv.style.backgroundImage = url(this.data.iconURL);
      }

      var nameEl = this.ownerDocument.createElement('div');
      nameEl.className = 'network-name-label';
      nameEl.textContent = this.data.networkName;
      textDiv.appendChild(nameEl);

      if (this.other) {
        // No status and buttons for "Other..."
        this.appendChild(textDiv);
        return;
      }

      // Only show status text if not empty.
      if (this.data.networkStatus) {
        var statusEl = this.ownerDocument.createElement('div');
        statusEl.className = 'network-status-label';
        statusEl.textContent = this.data.networkStatus;
        textDiv.appendChild(statusEl);
      }

      this.appendChild(textDiv);

      var spacerDiv = this.ownerDocument.createElement('div');
      spacerDiv.className = 'network-item-box-spacer';
      this.appendChild(spacerDiv);

      var buttonsDiv = this.ownerDocument.createElement('div');
      var self = this;
      if (!this.data.remembered) {
        var no_plan =
            this.data.networkType == Constants.TYPE_CELLULAR &&
            this.data.needs_new_plan;
        var show_activate =
          (this.data.networkType == Constants.TYPE_CELLULAR &&
           this.data.activation_state !=
               Constants.ACTIVATION_STATE_ACTIVATED &&
           this.data.activation_state !=
               Constants.ACTIVATION_STATE_ACTIVATING);

        // Show [Activate] button for non-activated Cellular network.
        if (show_activate || no_plan) {
          var button_name = no_plan ? 'buyplan_button' : 'activate_button';
          buttonsDiv.appendChild(
              this.createButton_(button_name, 'activate',
                                 function(e) {
                chrome.send('buttonClickCallback',
                            [String(self.data.networkType),
                             self.data.servicePath,
                             'activate']);
              }));
        }
        // Show disconnect button if not ethernet.
        if (this.data.networkType != Constants.TYPE_ETHERNET &&
            this.data.connected) {
          buttonsDiv.appendChild(
              this.createButton_('disconnect_button', 'disconnect',
                                  function(e) {
                 chrome.send('buttonClickCallback',
                             [String(self.data.networkType),
                              self.data.servicePath,
                              'disconnect']);
               }));
        }
        if (!this.data.connected && !this.data.connecting) {
          // connect button (if not ethernet and not showing activate button)
          if (this.data.networkType != Constants.TYPE_ETHERNET &&
              !show_activate && !no_plan) {
            buttonsDiv.appendChild(
                this.createButton_('connect_button', 'connect',
                                   function(e) {
                  chrome.send('buttonClickCallback',
                              [String(self.data.networkType),
                               self.data.servicePath,
                               'connect']);
                }));
          }
        }
        if (this.data.connected ||
                this.data.networkType == Constants.TYPE_ETHERNET ||
                this.data.networkType == Constants.TYPE_VPN ||
                this.data.networkType == Constants.TYPE_WIFI ||
                this.data.networkType == Constants.TYPE_CELLULAR) {
          buttonsDiv.appendChild(
              this.createButton_('options_button', 'options',
                                 function(e) {
                options.ProxyOptions.getInstance().setNetworkName(
                    self.data.networkName);
                chrome.send('buttonClickCallback',
                            [String(self.data.networkType),
                             self.data.servicePath,
                             'options']);
              }));
        }
      } else {
        // Put "Forget this network" button.
        var button = this.createButton_('forget_button', 'forget',
                                        function(e) {
                       chrome.send('buttonClickCallback',
                                   [String(self.data.networkType),
                                   self.data.servicePath,
                                   'forget']);
                     });
        button.disabled = this.data.policyManaged;
        buttonsDiv.appendChild(button);
      }
      this.appendChild(buttonsDiv);
    },

    /**
     * Creates a button for interacting with a network.
     * @param {Object} name The name of the localStrings to use for the text.
     * @param {Object} type The type of button.
     */
    createButton_: function(name, type, callback) {
      var buttonEl = this.ownerDocument.createElement('button');
      buttonEl.buttonType = type;
      buttonEl.textContent = localStrings.getString(name);
      buttonEl.addEventListener('click', callback);
      return buttonEl;
    }
  };

  /**
   * Whether the underlying network is connected. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(NetworkItem, 'connected', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the underlying network is currently connecting.
   * Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(NetworkItem, 'connecting', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the underlying network is an other network for adding networks.
   * Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(NetworkItem, 'other', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the underlying network is connectable.
   * @type {boolean}
   */
  cr.defineProperty(NetworkItem, 'connectable', cr.PropertyKind.BOOL_ATTR);

  return {
    Constants: Constants,
    NetworkElement: NetworkElement
  };
});
