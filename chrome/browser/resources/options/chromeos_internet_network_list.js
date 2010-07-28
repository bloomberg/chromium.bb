// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.internet', function() {
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;
  const ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Creates a new network list.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.List}
   */
  var NetworkList = cr.ui.define('list');

  NetworkList.prototype = {
    __proto__: List.prototype,

    createItem: function(network) {
      return new NetworkListItem(network);
    },

    /**
     * Loads given network list.
     * @param {Array} users An array of user object.
     */
    load: function(networks) {
      this.dataModel = new ArrayDataModel(networks);
    },

  };

  /**
   * Creates a new network list item.
   * @param {!ListValue} network The network this represents.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function NetworkListItem(network) {
    var el = cr.doc.createElement('div');
    el.servicePath = network[0];
    el.networkName = network[1];
    el.networkStatus = network[2];
    el.networkType = network[3];
    el.connected = network[4];
    el.connecting = network[5];
    el.iconURL = network[6];
    el.remembered = network[7];
    NetworkListItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a network list item.
   * @param {!HTMLElement} el The element to decorate.
   */
  NetworkListItem.decorate = function(el) {
    el.__proto__ = NetworkListItem.prototype;
    el.decorate();
  };

  NetworkListItem.prototype = {
    __proto__: ListItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      // icon and name
      var nameEl = this.ownerDocument.createElement('span');
      nameEl.className = 'label';
      // TODO(xiyuan): Use css for this.
      if (this.connected)
        nameEl.style.fontWeight = 'bold';
      nameEl.textContent = this.networkName;
      nameEl.style.backgroundImage = url(this.iconURL);
      this.appendChild(nameEl);

      // status
      var statusEl = this.ownerDocument.createElement('span');
      statusEl.className = 'label';
      statusEl.textContent = this.networkStatus;
      this.appendChild(statusEl);

      if (!this.remembered) {
        if (this.connected) {
          // disconnect button (if not ethernet)
          if (this.networkType != 1)
             this.appendChild(this.createButton_('disconnect_button',
                 'disconnect'));

          // options button
          this.appendChild(this.createButton_('options_button', 'options'));
        } else if (!this.connecting) {
          // connect button
          this.appendChild(this.createButton_('connect_button', 'connect'));
        }
      } else {
        // forget button
        this.appendChild(this.createButton_('forget_button', 'forget'));
      }
    },

    /**
     * Creates a button for interacting with a network.
     * @param {Object} name The name of the localStrings to use for the text.
     * @param {Object} type The type of button.
     */
    createButton_: function(name, type) {
        var buttonEl = this.ownerDocument.createElement('button');
        buttonEl.textContent = localStrings.getString(name);
        var networkType = this.networkType;
        var servicePath = this.servicePath;
        buttonEl.onclick = function(event) {
            chrome.send('buttonClickCallback',
                [String(networkType), String(servicePath), String(type)]);
        };
        return buttonEl;
    }
  };

  return {
    NetworkList: NetworkList
  };
});
