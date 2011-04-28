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
            for (var i = 0; i < this.childNodes.length; i++) {
              if (this.childNodes[i] != item)
                this.childNodes[i].hidePassword();
            }
            InternetOptions.unlockUpdates();
            // If clicked on other networks item.
            if (data && data.servicePath == '?') {
              if (InternetOptions.useSettingsUI &&
                  data.type != options.internet.Constants.TYPE_CELLULAR) {
                item.showOtherLogin();
              } else {
                chrome.send('buttonClickCallback',
                            [String(data.networkType),
                             data.servicePath,
                             'connect']);
              }
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
    el.data = {
      servicePath: network[0],
      networkName: network[1],
      networkStatus: network[2],
      networkType: network[3],
      connected: network[4],
      connecting: network[5],
      iconURL: network[6],
      remembered: network[7],
      activation_state: network[8],
      needs_new_plan: network[9],
      connectable: network[10]
    };
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
      this.connected = this.data.connected;
      this.connectable = this.data.connectable;
      this.other = this.data.servicePath == '?';
      this.id = this.data.servicePath;
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

      // Only show status text for networks other than "remembered".
      if (!this.data.remembered) {
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
            this.data.networkType == Constants.TYPE_CELLULAR) {
          buttonsDiv.appendChild(
              this.createButton_('options_button', 'options',
                                 function(e) {
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
        if (!AccountsOptions.currentUserIsOwner()) {
          // Disable this for guest non-Owners.
          button.disabled = true;
        }

        buttonsDiv.appendChild(button);
      }
      this.appendChild(buttonsDiv);
    },

    showPassword: function() {
      if (this.connecting)
        return;

      InternetOptions.lockUpdates();

      var passwordDiv = this.ownerDocument.createElement('div');
      passwordDiv.className = 'network-password';
      var passInput = this.ownerDocument.createElement('input');
      passwordDiv.appendChild(passInput);
      passInput.placeholder = localStrings.getString('inetPassPrompt');
      passInput.type = 'password';
      var buttonEl = this.ownerDocument.createElement('button');
      buttonEl.textContent = localStrings.getString('inetLogin');
      buttonEl.addEventListener('click', this.handleLogin_);
      buttonEl.servicePath = this.data.servicePath;
      buttonEl.style.right = '0';
      buttonEl.style.position = 'absolute';
      buttonEl.style.visibility = 'visible';
      buttonEl.disabled = true;

      var togglePassLabel = this.ownerDocument.createElement('label');
      togglePassLabel.style.display = 'inline';
      var togglePassSpan = this.ownerDocument.createElement('span');
      var togglePassCheckbox = this.ownerDocument.createElement('input');
      togglePassCheckbox.type = 'checkbox';
      togglePassCheckbox.checked = false;
      togglePassCheckbox.target = passInput;
      togglePassCheckbox.addEventListener('change', this.handleShowPass_);
      togglePassSpan.textContent = localStrings.getString('inetShowPass');
      togglePassLabel.appendChild(togglePassCheckbox);
      togglePassLabel.appendChild(togglePassSpan);
      passwordDiv.appendChild(togglePassLabel);

      // Disable login button if there is no password.
      passInput.addEventListener('keyup', function(e) {
        buttonEl.disabled =
          passInput.value.length < Constants.MIN_WIRELESS_PASSWORD_LENGTH;
      });

      passwordDiv.appendChild(buttonEl);
      this.connecting = true;
      this.appendChild(passwordDiv);
    },

    handleShowPass_: function(e) {
      var target = e.target;
      if (target.checked) {
        target.target.type = 'text';
      } else {
        target.target.type = 'password';
      }
    },

    hidePassword: function() {
      this.connecting = false;
      var children = this.childNodes;
      // Remove all password divs starting from the end.
      for (var i = children.length-1; i >= 0; i--) {
        if (children[i].className == 'network-password') {
          this.removeChild(children[i]);
        }
      }
    },

    showOtherLogin: function() {
      if (this.connecting)
        return;

      InternetOptions.lockUpdates();

      var ssidDiv = this.ownerDocument.createElement('div');
      ssidDiv.className = 'network-password';
      var ssidInput = this.ownerDocument.createElement('input');
      ssidInput.placeholder = localStrings.getString('inetSsidPrompt');
      ssidDiv.appendChild(ssidInput);

      var securityDiv = this.ownerDocument.createElement('div');
      securityDiv.className = 'network-password';
      var securityInput = this.ownerDocument.createElement('select');
      var securityNoneOption = this.ownerDocument.createElement('option');
      securityNoneOption.value = 'none';
      securityNoneOption.label = localStrings.getString('inetSecurityNone');
      securityInput.appendChild(securityNoneOption);
      var securityWEPOption = this.ownerDocument.createElement('option');
      securityWEPOption.value = 'wep';
      securityWEPOption.label = localStrings.getString('inetSecurityWEP');
      securityInput.appendChild(securityWEPOption);
      var securityWPAOption = this.ownerDocument.createElement('option');
      securityWPAOption.value = 'wpa';
      securityWPAOption.label = localStrings.getString('inetSecurityWPA');
      securityInput.appendChild(securityWPAOption);
      var securityRSNOption = this.ownerDocument.createElement('option');
      securityRSNOption.value = 'rsn';
      securityRSNOption.label = localStrings.getString('inetSecurityRSN');
      securityInput.appendChild(securityRSNOption);
      securityDiv.appendChild(securityInput);

      var passwordDiv = this.ownerDocument.createElement('div');
      passwordDiv.className = 'network-password';
      var passInput = this.ownerDocument.createElement('input');
      passInput.placeholder = localStrings.getString('inetPassPrompt');
      passInput.type = 'password';
      passInput.disabled = true;
      passwordDiv.appendChild(passInput);

      var togglePassLabel = this.ownerDocument.createElement('label');
      togglePassLabel.style.display = 'inline';
      var togglePassSpan = this.ownerDocument.createElement('span');
      var togglePassCheckbox = this.ownerDocument.createElement('input');
      togglePassCheckbox.type = 'checkbox';
      togglePassCheckbox.checked = false;
      togglePassCheckbox.target = passInput;
      togglePassCheckbox.addEventListener('change', this.handleShowPass_);
      togglePassSpan.textContent = localStrings.getString('inetShowPass');
      togglePassLabel.appendChild(togglePassCheckbox);
      togglePassLabel.appendChild(togglePassSpan);
      passwordDiv.appendChild(togglePassLabel);

      var buttonEl =
        this.createButton_('inetLogin', true, this.handleOtherLogin_);
      buttonEl.style.right = '0';
      buttonEl.style.position = 'absolute';
      buttonEl.style.visibility = 'visible';
      buttonEl.disabled = true;
      passwordDiv.appendChild(buttonEl);

      this.appendChild(ssidDiv);
      this.appendChild(securityDiv);
      this.appendChild(passwordDiv);

      securityInput.addEventListener('change', function(e) {
        // If changed to None, then disable passInput and clear it out.
        // Otherwise enable it.
        if (securityInput.value == 'none') {
          passInput.disabled = true;
          passInput.value = '';
        } else {
          passInput.disabled = false;
        }
      });

      var keyup_listener = function(e) {
        // Disable login button if ssid is not long enough or
        // password is not long enough (unless no security)
        var ssid_good =
          ssidInput.value.length >= Constants.MIN_WIRELESS_SSID_LENGTH;
        var pass_good =
          securityInput.value == 'none' ||
            passInput.value.length >= Constants.MIN_WIRELESS_PASSWORD_LENGTH;
        buttonEl.disabled = !ssid_good || !pass_good;
      };
      ssidInput.addEventListener('keyup', keyup_listener);
      securityInput.addEventListener('change', keyup_listener);
      passInput.addEventListener('keyup', keyup_listener);
      this.connecting = true;
    },

    handleLogin_: function(e) {
      // The user has clicked on the Login button. It's now safe to
      // unclock UI updates.
      InternetOptions.unlockUpdates();
      var el = e.target;
      var parent = el.parentNode;
      el.disabled = true;
      var input = parent.firstChild;
      input.disabled = true;
      chrome.send('loginToNetwork', [el.servicePath, input.value]);
    },

    handleOtherLogin_: function(e) {
      // See comments in handleLogin_().
      InternetOptions.unlockUpdates();
      var el = e.target;
      var parent = el.parentNode.parentNode;
      el.disabled = true;
      var ssid = parent.childNodes[1].firstChild;
      var sec = parent.childNodes[2].firstChild;
      var pass = parent.childNodes[3].firstChild;
      sec.disabled = true;
      ssid.disabled = true;
      pass.disabled = true;
      chrome.send('loginToOtherNetwork', [sec.value, ssid.value, pass.value]);
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
