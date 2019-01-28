// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DeviceDetailsPage which displays all of the details of a
 * device. The page is generated and managed dynamically in bluetooth_internals.
 * served from chrome://bluetooth-internals/.
 */

cr.define('device_details_page', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var Snackbar = snackbar.Snackbar;
  /** @const */ var SnackbarType = snackbar.SnackbarType;

  /**
   * Property names that will be displayed in the ObjectFieldSet which contains
   * the DeviceInfo object.
   */
  var PROPERTY_NAMES = {
    name: 'Name',
    address: 'Address',
    isGattConnected: 'GATT Connected',
    'rssi.value': 'Latest RSSI',
    'services.length': 'Services',
  };

  /**
   * Page that displays all of the details of a device. This page is generated
   * and managed dynamically in bluetooth_internals. The page contains two
   * sections: Status and Services. The Status section displays information from
   * the DeviceInfo object and the Services section contains a ServiceList
   * compononent that lists all of the active services on the device.
   */
  class DeviceDetailsPage extends Page {
    /**
     * @param {string} id
     * @param {!bluetooth.mojom.DeviceInfo} deviceInfo
     */
    constructor(id, deviceInfo) {
      super(id, deviceInfo.nameForDisplay, id);

      /** @type {!bluetooth.mojom.DeviceInfo} */
      this.deviceInfo = deviceInfo;

      /** @type {?Array<bluetooth.mojom.ServiceInfo>} */
      this.services = null;

      /** @private {?bluetooth.mojom.DeviceProxy} */
      this.deviceProxy_ = null;

      /** @private {!object_fieldset.ObjectFieldSet} */
      this.deviceFieldSet_ = new object_fieldset.ObjectFieldSet();
      this.deviceFieldSet_.setPropertyDisplayNames(PROPERTY_NAMES);

      /** @private {!service_list.ServiceList} */
      this.serviceList_ = new service_list.ServiceList();

      /** @private {!device_collection.ConnectionStatus} */
      this.status_ = device_collection.ConnectionStatus.DISCONNECTED;

      /** @private {?Element} */
      this.connectBtn_ = null;

      this.pageDiv.appendChild(document.importNode(
          $('device-details-template').content, true /* deep */));

      this.pageDiv.querySelector('.device-details')
          .appendChild(this.deviceFieldSet_);
      this.pageDiv.querySelector('.services').appendChild(this.serviceList_);

      this.pageDiv.querySelector('.forget').addEventListener(
          'click', function() {
            this.disconnect();
            this.pageDiv.dispatchEvent(new CustomEvent('forgetpressed', {
              detail: {
                address: this.deviceInfo.address,
              },
            }));
          }.bind(this));

      this.connectBtn_ = this.pageDiv.querySelector('.disconnect');
      this.connectBtn_.addEventListener('click', function() {
        this.deviceProxy_ !== null ? this.disconnect() : this.connect();
      }.bind(this));

      this.redraw();
    }

    /** Creates a connection to the Bluetooth device. */
    connect() {
      if (this.status_ !== device_collection.ConnectionStatus.DISCONNECTED) {
        return;
      }

      this.updateConnectionStatus_(
          device_collection.ConnectionStatus.CONNECTING);

      device_broker.connectToDevice(this.deviceInfo.address)
          .then(function(deviceProxy) {
            this.deviceProxy_ = deviceProxy;

            this.updateConnectionStatus_(
                device_collection.ConnectionStatus.CONNECTED);

            // Fetch services asynchronously.
            return this.deviceProxy_.getServices();
          }.bind(this))
          .then(function(response) {
            this.services = response.services;
            this.serviceList_.load(this.deviceInfo.address);
            this.redraw();
            this.fireDeviceInfoChanged_();
          }.bind(this))
          .catch(function(error) {
            // If a connection error occurs while fetching the services, the
            // DeviceProxy reference must be removed.
            if (this.deviceProxy_) {
              this.deviceProxy_.disconnect();
              this.deviceProxy_ = null;
            }

            Snackbar.show(
                this.deviceInfo.nameForDisplay + ': ' + error.message,
                SnackbarType.ERROR, 'Retry', this.connect.bind(this));

            this.updateConnectionStatus_(
                device_collection.ConnectionStatus.DISCONNECTED);
          }.bind(this));
    }

    /** Disconnects the page from the Bluetooth device. */
    disconnect() {
      if (!this.deviceProxy_) {
        return;
      }

      this.deviceProxy_.disconnect();
      this.deviceProxy_ = null;
      this.updateConnectionStatus_(
          device_collection.ConnectionStatus.DISCONNECTED);
    }

    /** Redraws the contents of the page with the current |deviceInfo|. */
    redraw() {
      var isConnected = this.deviceInfo.isGattConnected;

      // Update status if connection has changed.
      if (isConnected) {
        this.connect();
      } else {
        this.disconnect();
      }

      var connectedText = isConnected ? 'Connected' : 'Not Connected';

      var rssi = this.deviceInfo.rssi || {};
      var services = this.services;

      var rssiValue = 'Unknown';
      if (rssi.value != null && rssi.value <= 0) {
        rssiValue = rssi.value;
      }

      var serviceCount = 'Unknown';
      if (services != null && services.length >= 0) {
        serviceCount = services.length;
      }

      var deviceViewObj = {
        name: this.deviceInfo.nameForDisplay,
        address: this.deviceInfo.address,
        isGattConnected: connectedText,
        'rssi.value': rssiValue,
        'services.length': serviceCount,
      };

      this.deviceFieldSet_.setObject(deviceViewObj);
      this.serviceList_.redraw();
    }

    /**
     * Sets the page's device info and forces a redraw.
     * @param {!bluetooth.mojom.DeviceInfo} info
     */
    setDeviceInfo(info) {
      this.deviceInfo = info;
      this.redraw();
    }

    /**
     * Fires an 'infochanged' event with the current |deviceInfo|
     * @private
     */
    fireDeviceInfoChanged_() {
      this.pageDiv.dispatchEvent(new CustomEvent('infochanged', {
        bubbles: true,
        detail: {
          info: this.deviceInfo,
        },
      }));
    }

    /**
     * Updates the current connection status. Caches the latest status, updates
     * the connection button message, and fires a 'connectionchanged' event when
     * finished.
     * @param {!device_collection.ConnectionStatus} status
     * @private
     */
    updateConnectionStatus_(status) {
      if (this.status_ === status) {
        return;
      }

      this.status_ = status;
      if (status === device_collection.ConnectionStatus.DISCONNECTED) {
        this.connectBtn_.textContent = 'Connect';
        this.connectBtn_.disabled = false;
      } else if (status === device_collection.ConnectionStatus.CONNECTING) {
        this.connectBtn_.textContent = 'Connecting';
        this.connectBtn_.disabled = true;
      } else {
        this.connectBtn_.textContent = 'Disconnect';
        this.connectBtn_.disabled = false;
      }

      this.pageDiv.dispatchEvent(new CustomEvent('connectionchanged', {
        bubbles: true,
        detail: {
          address: this.deviceInfo.address,
          status: status,
        }
      }));
    }
  }

  return {
    DeviceDetailsPage: DeviceDetailsPage,
  };
});
