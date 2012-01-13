// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.system.bluetooth', function() {
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const DeletableItem = options.DeletableItem;
  const DeletableItemList = options.DeletableItemList;

  /**
   * Bluetooth settings constants.
   */
  function Constants() {}

  /**
   * Enumeration of supported device types.
   * @enum {string}
   */
  // TODO(kevers): Prune list based on the set of devices that will be
  // supported for V1 of the feature.  The set will likely be restricted to
  // mouse and keyboard.  Others are temporarily included for testing device
  // discovery.
  Constants.DEVICE_TYPE = {
    COMPUTER: 'computer',
    HEADSET: 'headset',
    KEYBOARD: 'input-keyboard',
    MOUSE: 'input-mouse',
    PHONE: 'phone',
  };

  /**
   * Creates a new bluetooth list item.
   * @param {{name: string,
   *          address: string,
   *          icon: Constants.DEVICE_TYPE,
   *          paired: boolean,
   *          connected: boolean,
   *          pairing: string|undefined,
   *          passkey: number|undefined,
   *          entered: number|undefined}} device
   *    Description of the Bluetooth device.
   * @constructor
   * @extends {options.DeletableItem}
   */
  function BluetoothListItem(device) {
    var el = cr.doc.createElement('div');
    el.__proto__ = BluetoothListItem.prototype;
    el.data = {};
    for (var key in device)
      el.data[key] = device[key];
    el.decorate();
    // Only show the close button for paired devices.
    el.deletable = device.paired;
    return el;
  }

  BluetoothListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /**
     * Description of the Bluetooth device.
     * @type {{name: string,
     *         address: string,
     *         icon: Constants.DEVICE_TYPE,
     *         paired: boolean,
     *         connected: boolean,
     *         pairing: string|undefined,
     *         passkey: number|undefined,
     *         entered: number|undefined}}
     */
    data: null,

    /** @inheritDoc */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);
      var label = this.ownerDocument.createElement('div');
      label.className = 'bluetooth-device-label';
      this.classList.add('bluetooth-device');
      this.connected = this.data.connected;
      // Though strictly speaking, a connected device will also be paired, we
      // are interested in tracking paired devices that are not connected.
      this.paired = this.data.paired && !this.data.connected;
      this.connecting = !!this.data.pairing;
      var content = this.data.name;
      // Update label for devices that are paired but not connected.
      if (this.paired) {
        content = content + ' (' +
            templateData['bluetoothDeviceNotConnected'] + ')';
      }
      label.textContent = content;
      this.contentElement.appendChild(label);
    },
  };

  /**
   * Class for displaying a list of Bluetooth devices.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var BluetoothDeviceList = cr.ui.define('list');

  BluetoothDeviceList.prototype = {
    __proto__: DeletableItemList.prototype,

    /** @inheritDoc */
    decorate:  function() {
      DeletableItemList.prototype.decorate.call(this);
      this.addEventListener('blur', this.onBlur_);
      this.clear();
    },

    /**
     * When the list loses focus, unselect all items in the list.
     * @private
     */
    onBlur_: function() {
      // TODO(kevers): Should this be pushed up to the list class?
      this.selectionModel.unselectAll();
    },

    /**
     * Adds a bluetooth device to the list of available devices. A check is
     * made to see if the device is already in the list, in which case the
     * existing device is updated.
     * @param {{name: string,
     *          address: string,
     *          icon: Constants.DEVICE_TYPE,
     *          paired: boolean,
     *          connected: boolean,
     *          pairing: string|undefined,
     *          passkey: number|undefined,
     *          entered: number|undefined}} device
     *     Description of the bluetooth device.
     * @return {boolean} True if the devies was successfully added or updated.
     */
    appendDevice: function(device) {
      if (!this.isSupported_(device))
        return false;
      var index = this.find(device.address);
      if (index == undefined) {
        this.dataModel.push(device);
        this.redraw();
      } else {
        this.dataModel.splice(index, 1, device);
        this.redrawItem(index);
      }
      this.updateListVisibility_();
      return true;
    },

    /**
     * Perges all devices from the list.
     */
    clear: function() {
      this.dataModel = new ArrayDataModel([]);
      this.redraw();
      this.updateListVisibility_();
    },

    /**
     * Returns the index of the list entry with the matching address.
     * @param {string} address Unique address of the Bluetooth device.
     * @return {number|undefined} Index of the matching entry or
     * undefined if no match found.
     */
    find: function(address) {
      var size = this.dataModel.length;
      for (var i = 0; i < size; i++) {
        var entry = this.dataModel.item(i);
        if (entry.address == address)
          return i;
      }
    },

    /** @inheritDoc */
    createItem: function(entry) {
      return new BluetoothListItem(entry);
    },

    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      var item = this.dataModel.item(index);
      if (item && (item.connected || item.paired)) {
        // Inform the bluetooth adapter that we are disconnecting the device.
        chrome.send('updateBluetoothDevice',
            [item.address, item.connected ? 'disconnect' : 'forget']);
      }
      this.dataModel.splice(index, 1);
      // Invalidate the list since it has a stale cache after a splice
      // involving a deletion.
      this.invalidate();
      this.redraw();
      this.updateListVisibility_();
    },

    /**
     * Tests if the bluetooth device is supported based on the type of device.
     * @param {Object.<string,string>} device Desription of the device.
     * @return {boolean} true if the device is supported.
     * @private
     */
    isSupported_: function(device) {
      var target = device.icon;
      for (var key in Constants.DEVICE_TYPE) {
        if (Constants.DEVICE_TYPE[key] == target)
          return true;
      }
      return false;
    },

    /**
     * If the list has an associated empty list placholder then update the
     * visibility of the list and placeholder.
     * @private
     */
    updateListVisibility_: function() {
      var empty = this.dataModel.length == 0;
      var listPlaceHolderID = this.id + '-empty-placeholder';
      if ($(listPlaceHolderID)) {
        this.hidden = empty;
        $(listPlaceHolderID).hidden = !empty;
      }
    },
  };

  cr.defineProperty(BluetoothListItem, 'connected', cr.PropertyKind.BOOL_ATTR);

  cr.defineProperty(BluetoothListItem, 'paired', cr.PropertyKind.BOOL_ATTR);

  cr.defineProperty(BluetoothListItem, 'connecting', cr.PropertyKind.BOOL_ATTR);

  return {
    BluetoothListItem: BluetoothListItem,
    BluetoothDeviceList: BluetoothDeviceList,
    Constants: Constants
  };
});
