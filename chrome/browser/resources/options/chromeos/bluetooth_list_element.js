// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.system.bluetooth', function() {
  /**
   * Bluetooth settings constants.
   */
  function Constants() {}

  /**
   * Enumeration of supported device types.  Each device type has an
   * associated icon and CSS style.
   * @enum {string}
   */
  Constants.DEVICE_TYPE = {
    COMPUTER: 'computer',
    HEADSET: 'headset',
    KEYBOARD: 'input-keyboard',
    MOUSE: 'input-mouse',
    PHONE: 'phone',
  };

  /**
   * Enumeration of possible states for a bluetooth device.  The value
   * associated with each state maps to a localized string in the global
   * variable 'templateData'.
   * @enum {string}
   */
  Constants.DEVICE_STATUS = {
    CONNECTED: 'bluetoothDeviceConnected',
    NOT_PAIRED: 'bluetoothDeviceNotPaired'
  };

  /**
   * Creates an element for storing a list of bluetooth devices.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var BluetoothListElement = cr.ui.define('div');

  BluetoothListElement.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
    },

    /**
     * Loads given list of bluetooth devices. This list will comprise of
     * devices that are currently connected. New devices are discovered
     * via the 'Find devices' button.
     * @param {Array} devices An array of bluetooth devices.
     */
    load: function(devices) {
      this.textContent = '';
      for (var i = 0; i < devices.length; i++) {
        if (this.isSupported_(devices[i]))
          this.appendChild(new BluetoothItem(devices[i]));
      }
    },

    /**
     * Adds a bluetooth device to the list of available devices. A check is
     * made to see if the device is already in the list, in which case the
     * existing device is updated.
     * @param {Object.<string,string>} device Description of the bluetooth
     *     device.
     */
    appendDevice: function(device) {
      if (!this.isSupported_(device))
        return;

      var item = new BluetoothItem(device);
      var candidate = this.firstChild;
      while (candidate) {
        if (candidate.data.address  == device.address) {
          this.replaceChild(item, candidate);
          return;
        }
        candidate = candidate.nextSibling;
      }
      this.appendChild(item);
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
    }
  };

  /**
   * Creates an element in the list of bluetooth devices.
   * @param{{'deviceName': string,
   *         'deviceId': string,
   *         'deviceType': Constants.DEVICE_TYPE,
   *         'deviceStatus': Constants.DEVICE_STATUS} device
   *     Decription of the bluetooth device.
   * @constructor
   */
  function BluetoothItem(device) {
    var el = cr.doc.createElement('div');
    el.data = {};
    for (var key in device)
      el.data[key] = device[key];
    BluetoothItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a network item.
   * @param {!HTMLElement} el The element to decorate.
   */
  BluetoothItem.decorate = function(el) {
    el.__proto__ = BluetoothItem.prototype;
    el.decorate();
  };

  BluetoothItem.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.className = 'network-item';
      this.connected = this.data.connected;
      if (this.data.deviceId)
        this.id = this.data.address;

      // |textDiv| holds icon, name and status text.
      var textDiv = this.ownerDocument.createElement('div');
      textDiv.className = 'network-item-text';

      var deviceSpecificClassName = 'bluetooth-' + this.data.icon;
      this.classList.add(deviceSpecificClassName);

      var nameEl = this.ownerDocument.createElement('div');
      nameEl.className = 'network-name-label';
      nameEl.textContent = this.data.name;
      textDiv.appendChild(nameEl);
      var buttonsDiv = null;
      var status = (this.data.connected) ?
           Constants.DEVICE_STATUS.CONNECTED :
           Constants.DEVICE_STATUS.NOT_PAIRED;
      var statusMessage = templateData[status];
      if (statusMessage) {
        var statusEl = this.ownerDocument.createElement('div');
        statusEl.className = 'network-status-label';
        statusEl.textContent = statusMessage;
        textDiv.appendChild(statusEl);
      }
      buttonsDiv = this.ownerDocument.createElement('div');
      buttonsDiv.className = 'bluetooth-button-group';
      var buttonLabelKey = null;
      var callbackType = null;
      if (status == Constants.DEVICE_STATUS.CONNECTED) {
        buttonLabelKey = 'bluetoothDisconnectDevice';
        callbackType = 'disconnect';
      } else if (status == Constants.DEVICE_STATUS.NOT_PAIRED) {
        buttonLabelKey = 'bluetoothConnectDevice';
        callbackType = 'connect';
      }
      if (buttonLabelKey && callbackType) {
        var buttonEl = this.ownerDocument.createElement('button');
        buttonEl.textContent = localStrings.getString(buttonLabelKey);
        var self = this;
        var callback = function(e) {
          chrome.send('updateBluetoothDevice',
              [self.data.address, callbackType]);
        }
        buttonEl.addEventListener('click', callback);
        buttonsDiv.appendChild(buttonEl);
      }
      this.appendChild(textDiv);
      if (buttonsDiv)
        this.appendChild(buttonsDiv);
    }
  };

  cr.defineProperty(BluetoothItem, 'connected', cr.PropertyKind.BOOL_ATTR);

  return {
    Constants: Constants,
    BluetoothListElement: BluetoothListElement
  };
});
