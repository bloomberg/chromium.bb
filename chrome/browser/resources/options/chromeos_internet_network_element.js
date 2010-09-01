// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.internet', function() {
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
      // Handle left button click
      if (e.button == 0) {
        var el = e.target;

        // If click is on action buttons of a network item.
        if (el.buttonType && el.networkType && el.servicePath) {
          chrome.send('buttonClickCallback',
              [String(el.networkType), el.servicePath, el.buttonType]);
        } else {
          // If click is on a network item or its label, walk up the DOM tree
          // to find the network item.
          var item = el;
          while (item && !item.data) {
            item = item.parentNode;
          }

          if (item) {
            var data = item.data;

            // If clicked on other networks item.
            if (data && data.servicePath == '?') {
              chrome.send('buttonClickCallback',
                  [String(data.networkType), data.servicePath, 'options']);
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
      remembered: network[7]
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
      var isOtherNetworksItem = this.data.servicePath == '?';

      this.className = 'network-item';
      this.connected = this.data.connected;

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

      if (isOtherNetworksItem) {
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
      if (!this.data.remembered) {
        if (this.data.connected) {
          // disconnect button (if not ethernet)
          if (this.data.networkType != 1)
             buttonsDiv.appendChild(this.createButton_('disconnect_button',
                 'disconnect'));

          // options button
          buttonsDiv.appendChild(this.createButton_('options_button',
              'options'));
        } else if (!this.data.connecting) {
          // connect button (if not ethernet)
          if (this.data.networkType != 1) {
            buttonsDiv.appendChild(this.createButton_('connect_button',
                'connect'));
          }
        }
      } else {
        // forget button
        buttonsDiv.appendChild(this.createButton_('forget_button', 'forget'));
      }
      this.appendChild(buttonsDiv);
    },

    /**
     * Creates a button for interacting with a network.
     * @param {Object} name The name of the localStrings to use for the text.
     * @param {Object} type The type of button.
     */
    createButton_: function(name, type) {
      var buttonEl = this.ownerDocument.createElement('button');
      buttonEl.textContent = localStrings.getString(name);
      buttonEl.buttonType = type;
      buttonEl.networkType = this.data.networkType;
      buttonEl.servicePath = this.data.servicePath;
      return buttonEl;
    }
  };

  /**
   * Whether the underlying network is connected. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(NetworkItem, 'connected', cr.PropertyKind.BOOL_ATTR);

  return {
    NetworkElement: NetworkElement
  };
});
