// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.system.bluetooth', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var DeletableItem = options.DeletableItem;
  /** @const */ var DeletableItemList = options.DeletableItemList;

  /**
   * Bluetooth settings constants.
   */
  function Constants() {}

  /**
   * Creates a new bluetooth list item.
   * @param {{name: string,
   *          address: string,
   *          paired: boolean,
   *          bonded: boolean,
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
     *         paired: boolean,
     *         bonded: boolean,
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
            loadTimeData.getString('bluetoothDeviceNotConnected') + ')';
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

    /**
     * Height of a list entry in px.
     * @type {number}
     * @private
     */
    itemHeight_: 32,

    /**
     * Width of a list entry in px.
     * @type {number}
     * @private.
     */
    itemWidth_: 400,

    /** @inheritDoc */
    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);
      // Force layout of all items even if not in the viewport to address
      // calculation errors when the list is hidden.  The impact on performance
      // should be minimal given that the list is not expected to grow very
      // large.
      this.autoExpand = true;
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
     *          paired: boolean,
     *          bonded: boolean,
     *          connected: boolean,
     *          pairing: string|undefined,
     *          passkey: number|undefined,
     *          entered: number|undefined}} device
     *     Description of the bluetooth device.
     * @return {boolean} True if the devies was successfully added or updated.
     */
    appendDevice: function(device) {
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
     * Forces a revailidation of the list content. Content added while the list
     * is hidden is not properly rendered when the list becomes visible. In
     * addition, deleting a single item from the list results in a stale cache
     * requiring an invalidation.
     */
    refresh: function() {
      // TODO(kevers): Investigate if the root source of the problems can be
      // fixed in cr.ui.list.
      this.invalidate();
      this.redraw();
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

    /**
     * Overrides the default implementation, which is used to compute the
     * size of an element in the list.  The default implementation relies
     * on adding a placeholder item to the list and fetching its size and
     * position. This strategy does not work if an item is added to the list
     * while it is hidden, as the computed metrics will all be zero in that
     * case.
     * @return {{height: number, marginTop: number, marginBottom: number,
     *     width: number, marginLeft: number, marginRight: number}}
     *     The height and width of the item, taking margins into account,
     *     and the margins themselves.
     */
    measureItem: function() {
      return {
        height: this.itemHeight_,
        marginTop: 0,
        marginBotton: 0,
        width: this.itemWidth_,
        marginLeft: 0,
        marginRight: 0
      };
    },

    /**
     * Override the default implementation to return a predetermined size,
     * which in turns allows proper layout of items even if the list is hidden.
     * @return {height: number, width: number} Dimensions of a single item in
     *     the list of bluetooth device.
     * @private.
     */
    getDefaultItemSize_: function() {
      return {
        height: this.itemHeight_,
        width: this.itemWidth_
      };
    },

    /**
     * Override base implementation of handleClick_, which unconditionally
     * removes the item.  In this case, removal of the element is deferred
     * pending confirmation from the Bluetooth adapter.
     * @param {Event} e The click event object.
     * @private
     */
    handleClick_: function(e) {
      if (this.disabled)
        return;

      var target = e.target;
      if (!target.classList.contains('row-delete-button'))
        return;

      var listItem = this.getListItemAncestor(target);
      var selected = this.selectionModel.selectedIndexes;
      var index = this.getIndexOfListItem(listItem);
      if (selected.indexOf(index) == -1)
        selected = [index];
      for (var j = selected.length - 1; j >= 0; j--) {
        var index = selected[j];
        var item = this.getListItemByIndex(index);
        if (item && item.deletable) {
          // Device is busy until we hear back from the Bluetooth adapter.
          // Prevent double removal request.
          item.deletable = false;
          // TODO(kevers): Provide visual feedback that the device is busy.

          // Inform the bluetooth adapter that we are disconnecting or
          // forgetting the device.
          chrome.send('updateBluetoothDevice',
            [item.data.address, item.connected ? 'disconnect' : 'forget']);
        }
      }
    },

    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      this.dataModel.splice(index, 1);
      this.refresh();
      this.updateListVisibility_();
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
        if (this.hidden != empty) {
          this.hidden = empty;
          $(listPlaceHolderID).hidden = !empty;
          this.refresh();
        }
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
