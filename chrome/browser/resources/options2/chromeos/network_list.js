// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.network', function() {

  var ArrayDataModel = cr.ui.ArrayDataModel;
  var List = cr.ui.List;
  var ListItem = cr.ui.ListItem;

  /**
   * Network settings constants. These enums usually match their C++
   * counterparts.
   */
  function Constants() {}

  // Network types:
  Constants.TYPE_UNKNOWN   = 0;
  Constants.TYPE_ETHERNET  = 1;
  Constants.TYPE_WIFI      = 2;
  Constants.TYPE_WIMAX     = 3;
  Constants.TYPE_BLUETOOTH = 4;
  Constants.TYPE_CELLULAR  = 5;
  Constants.TYPE_VPN       = 6;

  /**
   * Create an element in the network list for controlling network
   * connectivity.
   * @constructor
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
   * Decorates an element as a NetworkList.
   * @param{!Element} el The element to decorate.
   */
  NetworkListItem.decorate = function(el) {
    el.__proto__ = NetworkListItem.prototype;
    el.decorate();
  };

  NetworkListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * Description of the network.
     * @type {{key: string,
     *         networkList: Array}}
     * @private
     */
    data_: null,

    /* @inheritDoc */
    decorate: function() {
      ListItem.prototype.decorate.call(this);
      this.className = 'network-group';
      var networkIcon = this.ownerDocument.createElement('div');
      networkIcon.className = 'network-icon';
      this.appendChild(networkIcon);
      var textContent = this.ownerDocument.createElement('div');
      this.appendChild(textContent);
      var categoryLabel = this.ownerDocument.createElement('div');
      var title = this.data_.key + 'Title';
      categoryLabel.className = 'network-title';
      categoryLabel.textContent = templateData[title];
      textContent.appendChild(categoryLabel);
      var selector = this.ownerDocument.createElement('span');
      selector.className = 'network-selector';
      textContent.appendChild(selector);
      if (this.data_.networkList) {
        // TODO(kevers): Generalize method of setting default label.
        var defaultMessage = this.data_.key == 'wifi' ?
            'networkOffline' : 'joinNetwork';
        selector.textContent = templateData[defaultMessage];
        var list = this.data_.networkList;
        for (var i = 0; i < list.length; i++) {
          var networkDetails = list[i];
          if (networkDetails.connected || networkDetails.connecting) {
            selector.textContent = networkDetails.networkName;
            networkIcon.style.backgroundImage = url(networkDetails.iconURL);
            break;
          }
        }
      }
      // TODO(kevers): Create popup menu for network lists.
      // TODO(kevers): Add default icon when no network is connected or
      // connecting.
      // TODO(kevers): Add support for other types of list items including a
      // toggle control for airplane mode, and a control for a new conenction
      // dialog.
    },

  };

  /**
   * A list of controls for manipulating network connectivity.
   * @constructor
   */
  var NetworkList = cr.ui.define('list');

  NetworkList.prototype = {
    __proto__: List.prototype,

    /** @inheritDoc */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.addEventListener('blur', this.onBlur_);
      this.dataModel = new ArrayDataModel([]);
    },

    /**
     * When the list loses focus, unselect all items in the list.
     * @private
     */
    onBlur_: function() {
      this.selectionModel.unselectAll();
    },

    /**
     * Finds the index of a network item within the data model based on
     * category.
     * @param {string} key Unique key for the item in the list.
     * @return {number|undefined}
     */
    indexOf: function(key) {
      var size = this.dataModel.length;
      for (var i = 0; i < size; i++) {
        var entry = this.dataModel.item(i);
        if (entry.key == key)
          return i;
      }
    },

    /**
     * Updates a network control.
     * @param{Object.<string,string>} data Description of the entry.
     */
    update: function(data) {
      var index = this.indexOf(data.key);
      if (index == undefined) {
        this.dataModel.push(data);
        this.redraw();
      } else {
        this.dataModel.splice(index, 1, data);
        this.redrawItem(index);
      }
    },

    /** @inheritDoc */
    createItem: function(entry) {
      return new NetworkListItem(entry);
    },
  };

  /**
   * Chrome callback for updating network controls.
   * @param{Object} data Description of available network devices and their
   *     corresponding state.
   */
  NetworkList.refreshNetworkData = function(data) {
    loadData_('wifi',
              data.wirelessList,
              function(data) {
      return data.networkType == Constants.TYPE_WIFI;
    });
    loadData_('cellular',
              data.wirelessList,
              function(data) {
      return data.networkType == Constants.TYPE_CELLULAR;
    });
    loadData_('vpn', data.vpnList);
  };

  /**
   * Updates the list of available networks and their status filtered by
   * network type.
   * @param{string} category The type of network.
   * @param{Array} list The list of networks and their status.
   * @param{function(Object)=} opt_filter Optional filter for pruning the list
   *     of networks.
   */
  function loadData_(category, list, opt_filter) {
    var data = {key: category, networkList: list};
    if (opt_filter) {
      var filteredList = [];
      for (var i = 0; i < list.length; i++) {
        if (opt_filter(list[i]))
          filteredList.push(list[i]);
      }
      data.networkList = filteredList;
    }
    $('network-list').update(data);
  }

  // Export
  return {
    NetworkList: NetworkList
  };

});
