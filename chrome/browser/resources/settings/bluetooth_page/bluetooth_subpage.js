// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-bluetooth-subpage' is the settings subpage for managing bluetooth
 *  properties and devices.
 */

// NOTE(dbeam): even though this behavior is only used privately, it must
// be globally accessible for Closure's --polymer_pass to compile happily.

Polymer({
  is: 'settings-bluetooth-subpage',

  behaviors: [
    I18nBehavior,
    CrScrollableBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /** Reflects the bluetooth-page property. */
    bluetoothEnabled: {
      type: Boolean,
      notify: true,
    },

    /**
     * The bluetooth adapter state, cached by bluetooth-page.
     * @type {!chrome.bluetooth.AdapterState|undefined}
     */
    adapterState: Object,

    /** Informs bluetooth-page whether to show the spinner in the header. */
    showSpinner_: {
      type: Boolean,
      notify: true,
      computed: 'computeShowSpinner_(adapterState.*, dialogId_)',
    },

    /**
     * The ordered list of bluetooth devices.
     * @type {!Array<!chrome.bluetooth.Device>}
     * @private
     */
    deviceList_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The ordered list of paired or connecting bluetooth devices.
     * @type {!Array<!chrome.bluetooth.Device>}
     */
    pairedDeviceList_: {
      type: Array,
      value: /** @return {Array} */ function() {
        return [];
      },
    },

    /**
     * Reflects the iron-list selecteditem property.
     * @type {!chrome.bluetooth.Device}
     * @private
     */
    selectedPairedItem_: {
      type: Object,
      observer: 'selectedPairedItemChanged_',
    },

    /**
     * The ordered list of unpaired bluetooth devices.
     * @type {!Array<!chrome.bluetooth.Device>}
     */
    unpairedDeviceList_: {
      type: Array,
      value: /** @return {Array} */ function() {
        return [];
      },
    },

    /**
     * Reflects the iron-list selecteditem property.
     * @type {!chrome.bluetooth.Device}
     */
    selectedUnpairedItem_: {
      type: Object,
      observer: 'selectedUnpairedItemChanged_',
    },

    /**
     * Set to the name of the dialog to show. This page uses a single
     * paper-dialog to host one of two dialog elements: 'pairDevice' or
     * 'connectError'. This allows a seamless transition between dialogs.
     * Note: This property should be set before opening the dialog and setting
     * the property will not itself cause the dialog to open.
     * @private
     */
    dialogId_: {
      type: String,
      value: '',
    },

    /**
     * Current Pairing device.
     * @type {!chrome.bluetooth.Device|undefined}
     * @private
     */
    pairingDevice_: Object,

    /**
     * The translated error message to show when a connect error occurs.
     * @private
     */
    errorMessage_: String,

    /**
     * Interface for bluetooth calls. Set in bluetooth-page.
     * @type {Bluetooth}
     * @private
     */
    bluetooth: {
      type: Object,
      value: chrome.bluetooth,
    },

    /**
     * Interface for bluetoothPrivate calls. Set in bluetooth-page.
     * @type {BluetoothPrivate}
     * @private
     */
    bluetoothPrivate: {
      type: Object,
      value: chrome.bluetoothPrivate,
    },
  },

  observers: [
    'adapterStateChanged_(adapterState.*)',
    'deviceListChanged_(deviceList_.*)',
  ],

  /**
   * Listener for chrome.bluetooth.onBluetoothDeviceAdded/Changed events.
   * @type {?function(!chrome.bluetooth.Device)}
   * @private
   */
  bluetoothDeviceUpdatedListener_: null,

  /**
   * Listener for chrome.bluetooth.onBluetoothDeviceRemoved events.
   * @type {?function(!chrome.bluetooth.Device)}
   * @private
   */
  bluetoothDeviceRemovedListener_: null,

  /** @override */
  attached: function() {
    this.bluetoothDeviceUpdatedListener_ =
        this.bluetoothDeviceUpdatedListener_ ||
        this.onBluetoothDeviceUpdated_.bind(this);
    this.bluetooth.onDeviceAdded.addListener(
        this.bluetoothDeviceUpdatedListener_);
    this.bluetooth.onDeviceChanged.addListener(
        this.bluetoothDeviceUpdatedListener_);

    this.bluetoothDeviceRemovedListener_ =
        this.bluetoothDeviceRemovedListener_ ||
        this.onBluetoothDeviceRemoved_.bind(this);
    this.bluetooth.onDeviceRemoved.addListener(
        this.bluetoothDeviceRemovedListener_);
  },

  /** @override */
  detached: function() {
    this.bluetooth.onDeviceAdded.removeListener(
        assert(this.bluetoothDeviceUpdatedListener_));
    this.bluetooth.onDeviceChanged.removeListener(
        assert(this.bluetoothDeviceUpdatedListener_));
    this.bluetooth.onDeviceRemoved.removeListener(
        assert(this.bluetoothDeviceRemovedListener_));
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    this.updateDiscovery_();
  },

  /** @private */
  computeShowSpinner_: function() {
    return !this.dialogId_ && this.get('adapterState.discovering');
  },

  /** @private */
  adapterStateChanged_: function() {
    this.updateDiscovery_();
    this.updateDeviceList_();
  },

  /** @private */
  deviceListChanged_: function() {
    this.saveScroll(this.$.pairedDevices);
    this.saveScroll(this.$.unpairedDevices);
    this.pairedDeviceList_ = this.deviceList_.filter(function(device) {
      return !!device.paired || !!device.connecting;
    });
    this.unpairedDeviceList_ = this.deviceList_.filter(function(device) {
      return !device.paired;
    });
    this.updateScrollableContents();
    this.restoreScroll(this.$.unpairedDevices);
    this.restoreScroll(this.$.pairedDevices);
  },

  /** @private */
  selectedPairedItemChanged_: function() {
    if (this.selectedPairedItem_)
      this.connectDevice_(this.selectedPairedItem_);
  },

  /** @private */
  selectedUnpairedItemChanged_: function() {
    if (this.selectedUnpairedItem_)
      this.connectDevice_(this.selectedUnpairedItem_);
  },

  /** @private */
  updateDiscovery_: function() {
    if (!this.adapterState || !this.adapterState.powered)
      return;
    if (settings.getCurrentRoute() == settings.Route.BLUETOOTH_DEVICES)
      this.startDiscovery_();
    else
      this.stopDiscovery_();
  },

  /**
   * If bluetooth is enabled, request the complete list of devices and update
   * this.deviceList_.
   * @private
   */
  updateDeviceList_: function() {
    if (!this.bluetoothEnabled) {
      this.deviceList_ = [];
      return;
    }
    this.bluetooth.getDevices(function(devices) {
      this.deviceList_ = devices;
    }.bind(this));
  },

  /**
   * Process bluetooth.onDeviceAdded and onDeviceChanged events.
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  onBluetoothDeviceUpdated_: function(device) {
    var address = device.address;
    if (this.dialogId_ && this.pairingDevice_ &&
        this.pairingDevice_.address == address) {
      this.pairingDevice_ = device;
    }
    var index = this.deviceList_.findIndex(function(device) {
      return device.address == address;
    });
    if (index >= 0) {
      this.set('deviceList_.' + index, device);
      return;
    }
    this.push('deviceList_', device);
  },

  /**
   * Process bluetooth.onDeviceRemoved events.
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  onBluetoothDeviceRemoved_: function(device) {
    var address = device.address;
    var index = this.deviceList_.findIndex(function(device) {
      return device.address == address;
    });
    if (index >= 0)
      this.splice('deviceList_', index, 1);
  },

  /** @private */
  startDiscovery_: function() {
    if (!this.adapterState || this.adapterState.discovering)
      return;

    this.bluetooth.startDiscovery(function() {
      var lastError = chrome.runtime.lastError;
      if (lastError) {
        if (lastError.message == 'Starting discovery failed')
          return;  // May happen if also started elsewhere, ignore.
        console.error('startDiscovery Error: ' + lastError.message);
      }
    });
  },

  /** @private */
  stopDiscovery_: function() {
    if (!this.get('adapterState.discovering'))
      return;

    this.bluetooth.stopDiscovery(function() {
      var lastError = chrome.runtime.lastError;
      if (lastError) {
        if (lastError.message == 'Failed to stop discovery')
          return;  // May happen if also stopped elsewhere, ignore.
        console.error('stopDiscovery Error: ' + lastError.message);
      }
    });
  },

  /**
   * @param {!{detail: {action: string, device: !chrome.bluetooth.Device}}} e
   * @private
   */
  onDeviceEvent_: function(e) {
    var action = e.detail.action;
    var device = e.detail.device;
    if (action == 'connect')
      this.connectDevice_(device);
    else if (action == 'disconnect')
      this.disconnectDevice_(device);
    else if (action == 'remove')
      this.forgetDevice_(device);
    else
      console.error('Unexected action: ' + action);
  },

  /**
   * @param {boolean} enabled
   * @param {string} onstr
   * @param {string} offstr
   * @return {string}
   * @private
   */
  getOnOffString_: function(enabled, onstr, offstr) {
    return enabled ? onstr : offstr;
  },

  /**
   * @param {boolean} bluetoothEnabled
   * @param {!Array<!chrome.bluetooth.Device>} deviceList
   * @return {boolean}
   * @private
   */
  showDevices_: function(bluetoothEnabled, deviceList) {
    return bluetoothEnabled && deviceList.length > 0;
  },

  /**
   * @param {boolean} bluetoothEnabled
   * @param {!Array<!chrome.bluetooth.Device>} deviceList
   * @return {boolean}
   * @private
   */
  showNoDevices_: function(bluetoothEnabled, deviceList) {
    return bluetoothEnabled && deviceList.length == 0;
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  connectDevice_: function(device) {
    // If the device is not paired, show the pairing dialog before connecting.
    if (!device.paired) {
      this.pairingDevice_ = device;
      this.openDialog_('pairDevice');
    }

    this.bluetoothPrivate.connect(device.address, function(result) {
      var error;
      if (chrome.runtime.lastError) {
        error = chrome.runtime.lastError.message;
      } else {
        switch (result) {
          case chrome.bluetoothPrivate.ConnectResultType.IN_PROGRESS:
            return;  // Do not close the dialog
          case chrome.bluetoothPrivate.ConnectResultType.ALREADY_CONNECTED:
          case chrome.bluetoothPrivate.ConnectResultType.AUTH_CANCELED:
          case chrome.bluetoothPrivate.ConnectResultType.SUCCESS:
            break;
          default:
            error = result;
        }
      }

      if (!error) {
        this.$.deviceDialog.close();
        return;
      }

      var name = device.name || device.address;
      var id = 'bluetooth_connect_' + error;
      if (this.i18nExists(id)) {
        this.errorMessage_ = this.i18n(id, name);
      } else {
        this.errorMessage_ = error;
        console.error('Unexpected error connecting to: ' + name + ': ' + error);
      }
      this.openDialog_('connectError');
    }.bind(this));
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  disconnectDevice_: function(device) {
    this.bluetoothPrivate.disconnectAll(device.address, function() {
      if (chrome.runtime.lastError) {
        console.error(
            'Error disconnecting: ' + device.address +
            chrome.runtime.lastError.message);
      }
    });
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  forgetDevice_: function(device) {
    this.bluetoothPrivate.forgetDevice(device.address, function() {
      if (chrome.runtime.lastError) {
        console.error(
            'Error forgetting: ' + device.name + ': ' +
            chrome.runtime.lastError.message);
      }
      this.updateDeviceList_();
    }.bind(this));
  },

  /**
   * @param {string} dialogId
   * @private
   */
  openDialog_: function(dialogId) {
    if (this.dialogId_) {
      // Dialog already opened, just update the contents.
      this.dialogId_ = dialogId;
      return;
    }
    this.dialogId_ = dialogId;
    // Call flush so that the dialog gets sized correctly before it is opened.
    Polymer.dom.flush();
    this.$.deviceDialog.open();
  },

  /** @private */
  onDialogClosed_: function() {
    this.dialogId_ = '';
    this.pairingDevice_ = undefined;
  },
});
