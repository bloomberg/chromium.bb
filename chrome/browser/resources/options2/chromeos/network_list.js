// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.network', function() {

  var ArrayDataModel = cr.ui.ArrayDataModel;
  var List = cr.ui.List;
  var ListItem = cr.ui.ListItem;
  var Menu = cr.ui.Menu;
  var MenuItem = cr.ui.MenuItem;

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
   * ID of the menu that is currently visible.
   * @type {?string}
   * @private
   */
  var activeMenu_ = null;

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
        this.createMenu_();
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
      // TODO(kevers): Add default icon when no network is connected or
      // connecting.
      // TODO(kevers): Add support for other types of list items including a
      // toggle control for airplane mode, and a control for a new conenction
      // dialog.

      this.addEventListener('click', function() {
        this.showMenu();
      });
    },

    /**
     * Creates a menu for selecting, configuring or disconnecting from a
     * network.
     * @private
     */
    createMenu_: function() {
      var menu = this.ownerDocument.createElement('div');
      menu.id = this.getMenuName_();
      menu.className = 'network-menu';
      menu.hidden = true;
      Menu.decorate(menu);
      var addendum = [];
      var list = this.data_.networkList;
      if (list) {
        for (var i = 0; i < list.length; i++) {
          var data = list[i];
          if (!data.connected && !data.connecting) {
            // TODO(kevers): Check for a non-activated Cellular network.
            // If found, the menu item should trigger 'activate' instead
            // of 'connect'.
            if (data.networkType != Constants.ETHERNET)
              this.createConnectCallback_(menu, data);
          } else if (data.connected) {
            addendum.push({label: localStrings.getString('networkOptions'),
                           command: 'options',
                           data: data});
            if (data.networkType == Constants.TYPE_WIFI) {
              // TODO(kevers): Add support for disconnecting from other
              // networks types.
              addendum.push({label: localStrings.getString('disconnectWifi'),
                             command: 'disconnect',
                             data: data});
            }
          }
        }
      }
      if (addendum.length > 0) {
        menu.appendChild(MenuItem.createSeparator());
        for (var i = 0; i < addendum.length; i++) {
          this.createCallback_(menu,
                               addendum[i].data,
                               addendum[i].label,
                               addendum[i].command);
        }
      }
      var parent = $('network-menus');
      var existing = $(menu.id);
      if (existing)
        parent.replaceChild(menu, existing);
      else
        parent.appendChild(menu);
    },

    /**
     * Adds a command to a menu, which calls back into chrome
     * for modifying network settings.
     * @param {!Element} menu Parent menu.
     * @param {Object} data Description of the network.
     * @param {string} label Display name for the menu item.
     * @param {string} command Name of the command for the
     *    callback.
     * @private
     */
    createCallback_: function(menu, data, label, command) {
      var button = this.ownerDocument.createElement('div');
      button.textContent = label;
      // TODO(kevers): Add icons as requireds.
      var type = String(data.networkType);
      var path = data.servicePath;
      button.addEventListener('click', function() {
        chrome.send('buttonClickCallback',
                    [type, path, command]);
        closeMenu_();
      });
      MenuItem.decorate(button);
      menu.appendChild(button);
    },

    /**
     * Adds a menu item for connecting to a network.
     * @param {!Element} menu Parent menu.
     * @param {Object} data Description of the network.
     * @privte
     */
    createConnectCallback_: function(menu, data) {
      this.createCallback_(menu, data, data.networkName, 'connect');
    },

    /**
     * Retrieves the ID for the menu.
     * @param{string} The menu ID.
     * @private
     */
    getMenuName_: function() {
      return this.data_.key + '-netowrk-menu';
    },

    /**
     * Displays a popup menu.
     */
    showMenu: function() {
      var top = this.offsetTop + this.clientHeight;
      var menuId = this.getMenuName_();
      if (menuId != activeMenu_) {
        closeMenu_();
        activeMenu_ = menuId;
        var menu = $(menuId);
        menu.style.setProperty('top', top + 'px');
        menu.hidden = false;
        setTimeout(function() {
          $('settings').addEventListener('click', closeMenu_);
        }, 0);
      }
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

  /**
   * Hides the currently visible menu.
   * @private
   */
  function closeMenu_() {
    if (activeMenu_) {
      $(activeMenu_).hidden = true;
      activeMenu_ = null;
      $('settings').removeEventListener('click', closeMenu_);
    }
  }

  // Export
  return {
    NetworkList: NetworkList
  };
});
