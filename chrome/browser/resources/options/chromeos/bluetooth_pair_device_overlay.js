// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enumeration of possible states during pairing.  The value associated with
 * each state maps to a localized string in the global variable
 * |loadTimeData|.
 * @enum {string}
 */
var BluetoothPairingEventType = {
  CONNECTING: 'bluetoothStartConnecting',
  ENTER_PIN_CODE: 'bluetoothEnterPinCode',
  ENTER_PASSKEY: 'bluetoothEnterPasskey',
  REMOTE_PIN_CODE: 'bluetoothRemotePinCode',
  REMOTE_PASSKEY: 'bluetoothRemotePasskey',
  CONFIRM_PASSKEY: 'bluetoothConfirmPasskey',
  CONNECT_FAILED: 'bluetoothConnectFailed',
  CANCELED: 'bluetoothPairingCanceled',
  DISMISSED: 'bluetoothPairingDismissed', // pairing dismissed (succeeded or
                                          // canceled).
  NOOP: ''  // Update device but do not show or update the dialog.
};

/**
 * @typedef {{pairing: (BluetoothPairingEventType|undefined),
 *            device: !chrome.bluetooth.Device,
 *            pincode: (string|undefined),
 *            passkey: (number|undefined),
 *            enteredKey: (number|undefined)}}
 */
var BluetoothPairingEvent;

/**
 * Returns a BluetoothPairingEventType corresponding to |event_type|.
 * @param {!chrome.bluetoothPrivate.PairingEventType} event_type
 * @return {BluetoothPairingEventType}
 */
function GetBluetoothPairingEvent(event_type) {
  switch (event_type) {
    case chrome.bluetoothPrivate.PairingEventType.REQUEST_PINCODE:
      return BluetoothPairingEventType.ENTER_PIN_CODE;
    case chrome.bluetoothPrivate.PairingEventType.DISPLAY_PINCODE:
      return BluetoothPairingEventType.REMOTE_PIN_CODE;
    case chrome.bluetoothPrivate.PairingEventType.REQUEST_PASSKEY:
      return BluetoothPairingEventType.ENTER_PASSKEY;
    case chrome.bluetoothPrivate.PairingEventType.DISPLAY_PASSKEY:
      return BluetoothPairingEventType.REMOTE_PASSKEY;
    case chrome.bluetoothPrivate.PairingEventType.KEYS_ENTERED:
      return BluetoothPairingEventType.NOOP;
    case chrome.bluetoothPrivate.PairingEventType.CONFIRM_PASSKEY:
      return BluetoothPairingEventType.CONFIRM_PASSKEY;
    case chrome.bluetoothPrivate.PairingEventType.REQUEST_AUTHORIZATION:
      return BluetoothPairingEventType.NOOP;
    case chrome.bluetoothPrivate.PairingEventType.COMPLETE:
      return BluetoothPairingEventType.NOOP;
  }
  return BluetoothPairingEventType.NOOP;
}

cr.define('options', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;

  /**
   * List of IDs for conditionally visible elements in the dialog.
   * @type {Array<string>}
   * @const
   */
  var ELEMENTS = ['bluetooth-pairing-passkey-display',
                  'bluetooth-pairing-passkey-entry',
                  'bluetooth-pairing-pincode-entry',
                  'bluetooth-pair-device-connect-button',
                  'bluetooth-pair-device-cancel-button',
                  'bluetooth-pair-device-accept-button',
                  'bluetooth-pair-device-reject-button',
                  'bluetooth-pair-device-dismiss-button'];

  /**
   * Encapsulated handling of the Bluetooth device pairing page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function BluetoothPairing() {
    Page.call(this, 'bluetoothPairing',
              loadTimeData.getString('bluetoothOptionsPageTabTitle'),
              'bluetooth-pairing');
  }

  cr.addSingletonGetter(BluetoothPairing);

  BluetoothPairing.prototype = {
    __proto__: Page.prototype,

    /**
     * Device pairing event.
     * @type {?BluetoothPairingEvent}
     * @private
     */
    event_: null,

    /**
     * Can the dialog be programmatically dismissed.
     * @type {boolean}
     */
    dismissible_: true,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);
      var self = this;
      $('bluetooth-pair-device-cancel-button').onclick = function() {
        PageManager.closeOverlay();
      };
      $('bluetooth-pair-device-reject-button').onclick = function() {
        var options = {
          device: self.event_.device,
          response: chrome.bluetoothPrivate.PairingResponse.REJECT
        };
        chrome.bluetoothPrivate.setPairingResponse(options);
        self.event_.pairing = BluetoothPairingEventType.DISMISSED;
        PageManager.closeOverlay();
      };
      $('bluetooth-pair-device-connect-button').onclick = function() {
        // Prevent sending a 'connect' command twice.
        $('bluetooth-pair-device-connect-button').disabled = true;

        var options = {
          device: self.event_.device,
          response: chrome.bluetoothPrivate.PairingResponse.CONFIRM
        };
        var passkey = self.event_.passkey;
        if (passkey) {
          options.passkey = passkey;
        } else if (!$('bluetooth-pairing-passkey-entry').hidden) {
          options.passkey = parseInt($('bluetooth-passkey').value, 10);
        } else if (!$('bluetooth-pairing-pincode-entry').hidden) {
          options.pincode = $('bluetooth-pincode').value;
        } else {
          BluetoothPairing.connect(self.event_.device);
          return;
        }
        chrome.bluetoothPrivate.setPairingResponse(options);
        var event = /** @type {!BluetoothPairingEvent} */ ({
          pairing: BluetoothPairingEventType.CONNECTING,
          device: self.event_.device
        });
        BluetoothPairing.showDialog(event);
      };
      $('bluetooth-pair-device-accept-button').onclick = function() {
        var options = {
          device: self.event_.device,
          response: chrome.bluetoothPrivate.PairingResponse.CONFIRM
        };
        chrome.bluetoothPrivate.setPairingResponse(options);
        // Prevent sending a 'accept' command twice.
        $('bluetooth-pair-device-accept-button').disabled = true;
      };
      $('bluetooth-pair-device-dismiss-button').onclick = function() {
        PageManager.closeOverlay();
      };
      $('bluetooth-passkey').oninput = function() {
        var inputField = $('bluetooth-passkey');
        var value = inputField.value;
        // Note that using <input type="number"> is insufficient to restrict
        // the input as it allows negative numbers and does not limit the
        // number of charactes typed even if a range is set.  Furthermore,
        // it sometimes produces strange repaint artifacts.
        var filtered = value.replace(/[^0-9]/g, '');
        if (filtered != value)
          inputField.value = filtered;
        $('bluetooth-pair-device-connect-button').disabled =
            inputField.value.length == 0;
      };
      $('bluetooth-pincode').oninput = function() {
        $('bluetooth-pair-device-connect-button').disabled =
            $('bluetooth-pincode').value.length == 0;
      };
      $('bluetooth-passkey').addEventListener('keydown',
          this.keyDownEventHandler_.bind(this));
      $('bluetooth-pincode').addEventListener('keydown',
          this.keyDownEventHandler_.bind(this));
      $('bluetooth-pairing-close-button').addEventListener('click',
          this.onClose_.bind(this));
    },

    /** @override */
    didClosePage: function() {
      if (this.event_ &&
          this.event_.pairing != BluetoothPairingEventType.DISMISSED &&
          this.event_.pairing != BluetoothPairingEventType.CONNECT_FAILED) {
        this.event_.pairing = BluetoothPairingEventType.CANCELED;
        var options = {
          device: this.event_.device,
          response: chrome.bluetoothPrivate.PairingResponse.CANCEL
        };
        chrome.bluetoothPrivate.setPairingResponse(options);
      }
    },

    /**
     * Override to prevent showing the overlay if the Bluetooth device details
     * have not been specified.  Prevents showing an empty dialog if the user
     * quits and restarts Chrome while in the process of pairing with a device.
     * @return {boolean} True if the overlay can be displayed.
     */
    canShowPage: function() {
      return !!(this.event_ && this.event_.device.address &&
                this.event_.pairing);
    },

    /**
     * Sets input focus on the passkey or pincode field if appropriate.
     */
    didShowPage: function() {
      if (!$('bluetooth-pincode').hidden)
        $('bluetooth-pincode').focus();
      else if (!$('bluetooth-passkey').hidden)
        $('bluetooth-passkey').focus();
    },

    /**
     * Configures the overlay for pairing a device.
     * @param {!BluetoothPairingEvent} event
     * @param {boolean=} opt_notDismissible
     */
    update: function(event, opt_notDismissible) {
      assert(event);
      assert(event.device);
      if (this.event_ == undefined ||
          this.event_.device.address != event.device.address) {
        // New event or device, create a new BluetoothPairingEvent.
        this.event_ =
            /** @type {BluetoothPairingEvent} */ ({device: event.device});
      } else {
        // Update to an existing event; just update |device| in case it changed.
        this.event_.device = event.device;
      }

      if (event.pairing)
        this.event_.pairing = event.pairing;

      if (!this.event_.pairing)
        return;

      if (this.event_.pairing == BluetoothPairingEventType.CANCELED) {
        // If we receive an update after canceling a pairing (e.g. a key
        // press), ignore it and clear the device so that future updates for
        // the device will also be ignored.
        this.event_.device.address = '';
        return;
      }

      if (event.pincode != undefined)
        this.event_.pincode = event.pincode;
      if (event.passkey != undefined)
        this.event_.passkey = event.passkey;
      if (event.enteredKey != undefined)
        this.event_.enteredKey = event.enteredKey;

      // Update the list model (in case, e.g. the name changed).
      if (this.event_.device.name) {
        var list = $('bluetooth-unpaired-devices-list');
        if (list) {  // May be undefined in tests.
          var index = list.find(this.event_.device.address);
          if (index != undefined)
            list.dataModel.splice(index, 1, this.event_.device);
        }
      }

      // Update the pairing instructions.
      var instructionsEl = assert($('bluetooth-pairing-instructions'));
      this.clearElement_(instructionsEl);
      this.dismissible_ = opt_notDismissible !== true;
      var message = loadTimeData.getString(this.event_.pairing);
      assert(typeof this.event_.device.name == 'string');
      message = message.replace(
          '%1', /** @type {string} */(this.event_.device.name));
      instructionsEl.textContent = message;

      // Update visibility of dialog elements.
      if (this.event_.passkey) {
        this.updatePasskey_(String(this.event_.passkey));
        if (this.event_.pairing == BluetoothPairingEventType.CONFIRM_PASSKEY) {
          // Confirming a match between displayed passkeys.
          this.displayElements_(['bluetooth-pairing-passkey-display',
                                 'bluetooth-pair-device-accept-button',
                                 'bluetooth-pair-device-reject-button']);
          $('bluetooth-pair-device-accept-button').disabled = false;
        } else {
          // Remote entering a passkey.
          this.displayElements_(['bluetooth-pairing-passkey-display',
                                 'bluetooth-pair-device-cancel-button']);
        }
      } else if (this.event_.pincode) {
        this.updatePasskey_(String(this.event_.pincode));
        this.displayElements_(['bluetooth-pairing-passkey-display',
                               'bluetooth-pair-device-cancel-button']);
      } else if (this.event_.pairing ==
                 BluetoothPairingEventType.ENTER_PIN_CODE) {
        // Prompting the user to enter a PIN code.
        this.displayElements_(['bluetooth-pairing-pincode-entry',
                               'bluetooth-pair-device-connect-button',
                               'bluetooth-pair-device-cancel-button']);
        $('bluetooth-pincode').value = '';
      } else if (this.event_.pairing ==
                 BluetoothPairingEventType.ENTER_PASSKEY) {
        // Prompting the user to enter a passkey.
        this.displayElements_(['bluetooth-pairing-passkey-entry',
                               'bluetooth-pair-device-connect-button',
                               'bluetooth-pair-device-cancel-button']);
        $('bluetooth-passkey').value = '';
      } else if (this.event_.pairing == BluetoothPairingEventType.CONNECTING) {
        // Starting the pairing process.
        this.displayElements_(['bluetooth-pair-device-cancel-button']);
      } else {
        // Displaying an error message.
        this.displayElements_(['bluetooth-pair-device-dismiss-button']);
      }
      // User is required to enter a passkey or pincode before the connect
      // button can be enabled.  The 'oninput' methods for the input fields
      // determine when the connect button becomes active.
      $('bluetooth-pair-device-connect-button').disabled = true;
    },

    /**
     * Handles the ENTER key for the passkey or pincode entry field.
     * @param {Event} event A keydown event.
     * @private
     */
    keyDownEventHandler_: function(event) {
      /** @const */ var ENTER_KEY_CODE = 13;
      if (event.keyCode == ENTER_KEY_CODE) {
        var button = $('bluetooth-pair-device-connect-button');
        if (!button.hidden)
          button.click();
      }
    },

    /**
     * Handles the click event on the close button.
     * @param {Event} event A click down event.
     * @private
     */
    onClose_: function(event) {
      event.preventDefault();
      chrome.send('dialogClose');
    },

    /**
     * Updates the visibility of elements in the dialog.
     * @param {Array<string>} list List of conditionally visible elements that
     *     are to be made visible.
     * @private
     */
    displayElements_: function(list) {
      var enabled = {};
      for (var i = 0; i < list.length; i++) {
        var key = list[i];
        enabled[key] = true;
      }
      for (var i = 0; i < ELEMENTS.length; i++) {
        var key = ELEMENTS[i];
        $(key).hidden = !enabled[key];
      }
    },

    /**
     * Removes all children from an element.
     * @param {!Element} element Target element to clear.
     */
    clearElement_: function(element) {
      var child = element.firstChild;
      while (child) {
        element.removeChild(child);
        child = element.firstChild;
      }
    },

    /**
     * Formats an element for displaying the passkey or PIN code.
     * @param {string} key Passkey or PIN to display.
     */
    updatePasskey_: function(key) {
      var passkeyEl = assert($('bluetooth-pairing-passkey-display'));
      var keyClass =
          (this.event_.pairing == BluetoothPairingEventType.REMOTE_PASSKEY ||
           this.event_.pairing == BluetoothPairingEventType.REMOTE_PIN_CODE) ?
              'bluetooth-keyboard-button' :
              'bluetooth-passkey-char';
      this.clearElement_(passkeyEl);
      // Passkey should always have 6 digits.
      key = '000000'.substring(0, 6 - key.length) + key;
      var progress = this.event_.enteredKey;
      for (var i = 0; i < key.length; i++) {
        var keyEl = document.createElement('span');
        keyEl.textContent = key.charAt(i);
        keyEl.className = keyClass;
        if (progress != undefined) {
          if (i < progress)
            keyEl.classList.add('key-typed');
          else if (i == progress)
            keyEl.classList.add('key-next');
          else
            keyEl.classList.add('key-untyped');
        }
        passkeyEl.appendChild(keyEl);
      }
      if (this.event_.pairing == BluetoothPairingEventType.REMOTE_PASSKEY ||
          this.event_.pairing == BluetoothPairingEventType.REMOTE_PIN_CODE) {
        // Add enter key.
        var label = loadTimeData.getString('bluetoothEnterKey');
        var keyEl = document.createElement('span');
        keyEl.textContent = label;
        keyEl.className = keyClass;
        keyEl.id = 'bluetooth-enter-key';
        if (progress != undefined) {
          if (progress > key.length)
            keyEl.classList.add('key-typed');
          else if (progress == key.length)
            keyEl.classList.add('key-next');
          else
            keyEl.classList.add('key-untyped');
        }
        passkeyEl.appendChild(keyEl);
      }
      passkeyEl.hidden = false;
    },
  };

  /**
   * Configures the device pairing instructions and displays the pairing
   * overlay.
   * @param {!BluetoothPairingEvent} event
   * @param {boolean=} opt_notDismissible If set to true, the dialog can not
   *     be dismissed.
   */
  BluetoothPairing.showDialog = function(event, opt_notDismissible) {
    BluetoothPairing.getInstance().update(event, opt_notDismissible);
    PageManager.showPageByName('bluetoothPairing', false);
  };


  /**
   * Handles bluetoothPrivate onPairing events.
   * @param {!chrome.bluetoothPrivate.PairingEvent} event
   */
  BluetoothPairing.onBluetoothPairingEvent = function(event) {
    var dialog = BluetoothPairing.getInstance();
    if (!dialog.event_ || dialog.event_.device.address != event.device.address)
      return;  // Ignore events not associated with an active connect or pair.
    var pairingEvent = /** @type {!BluetoothPairingEvent} */ ({
      pairing: GetBluetoothPairingEvent(event.pairing),
      device: event.device,
      pincode: event.pincode,
      passkey: event.passkey,
      enteredKey: event.enteredKey
    });
    dialog.update(pairingEvent);
    PageManager.showPageByName('bluetoothPairing', false);
  };

  /**
   * Displays a message from the Bluetooth adapter.
   * @param {{message: string, address: string}} data Data for constructing the
   *     message. |data.message| is the name of message to show. |data.address|
   *     is the device address.
   */
  BluetoothPairing.showMessage = function(data) {
    /** @type {string} */ var name = data.address;
    if (name.length == 0)
      return;
    var dialog = BluetoothPairing.getInstance();
    if (dialog.event_ && name == dialog.event_.device.address &&
        dialog.event_.pairing == BluetoothPairingEventType.CANCELED) {
      // Do not show any error message after cancelation of the pairing.
      return;
    }

    var list = $('bluetooth-paired-devices-list');
    if (list) {
      var index = list.find(name);
      if (index == undefined) {
        list = $('bluetooth-unpaired-devices-list');
        index = list.find(name);
      }
      if (index != undefined) {
        var entry = list.dataModel.item(index);
        if (entry && entry.name)
          name = /** @type {string} */ (entry.name);
      }
    }
    var event = /** @type {!BluetoothPairingEvent} */ ({
      pairing: /** @type {BluetoothPairingEventType} */ (data.message),
      device: /** @type {!chrome.bluetooth.Device} */ ({
        name: name,
        address: data.address,
      })
    });
    BluetoothPairing.showDialog(event, true /* not dismissible */);
  };

  /**
   * Sends a connect request to the bluetoothPrivate API. If there is an error
   * the pairing dialog will be shown with the error message.
   * @param {!chrome.bluetooth.Device} device
   * @param {boolean=} opt_showConnecting If true, show 'connecting' message in
   *     the pairing dialog.
   */
  BluetoothPairing.connect = function(device, opt_showConnecting) {
    if (opt_showConnecting) {
      var event = /** @type {!BluetoothPairingEvent} */ (
          {pairing: BluetoothPairingEventType.CONNECTING, device: device});
      BluetoothPairing.showDialog(event);
    }
    var address = device.address;
    chrome.bluetoothPrivate.connect(address, function(result) {
      BluetoothPairing.connectCompleted_(address, result);
    });
  };

  /**
   * Connect request completion callback.
   * @param {string} address
   * @param {chrome.bluetoothPrivate.ConnectResultType} result
   */
  BluetoothPairing.connectCompleted_ = function(address, result) {
    var message;
    if (chrome.runtime.lastError) {
      var errorMessage = chrome.runtime.lastError.message;
      if (errorMessage != 'Connect failed') {
        console.error('bluetoothPrivate.connect: Unexpected error for: ' +
                      address + ': ' + errorMessage);
      }
    }
    switch (result) {
      case chrome.bluetoothPrivate.ConnectResultType.SUCCESS:
      case chrome.bluetoothPrivate.ConnectResultType.ALREADY_CONNECTED:
        BluetoothPairing.dismissDialog();
        return;
      case chrome.bluetoothPrivate.ConnectResultType.UNKNOWN_ERROR:
        message = 'bluetoothConnectUnknownError';
        break;
      case chrome.bluetoothPrivate.ConnectResultType.IN_PROGRESS:
        message = 'bluetoothConnectInProgress';
        break;
      case chrome.bluetoothPrivate.ConnectResultType.FAILED:
        message = 'bluetoothConnectFailed';
        break;
      case chrome.bluetoothPrivate.ConnectResultType.AUTH_FAILED:
        message = 'bluetoothConnectAuthFailed';
        break;
      case chrome.bluetoothPrivate.ConnectResultType.AUTH_CANCELED:
        message = 'bluetoothConnectAuthCanceled';
        break;
      case chrome.bluetoothPrivate.ConnectResultType.AUTH_REJECTED:
        message = 'bluetoothConnectAuthRejected';
        break;
      case chrome.bluetoothPrivate.ConnectResultType.AUTH_TIMEOUT:
        message = 'bluetoothConnectAuthTimeout';
        break;
      case chrome.bluetoothPrivate.ConnectResultType.UNSUPPORTED_DEVICE:
        message = 'bluetoothConnectUnsupportedDevice';
        break;
      case chrome.bluetoothPrivate.ConnectResultType.ATTRIBUTE_LENGTH_INVALID:
        message = 'bluetoothConnectAttributeLengthInvalid';
        break;
      case chrome.bluetoothPrivate.ConnectResultType.CONNECTION_CONGESTED:
        message = 'bluetoothConnectConnectionCongested';
        break;
      case chrome.bluetoothPrivate.ConnectResultType.INSUFFICIENT_ENCRYPTION:
        message = 'bluetoothConnectInsufficientEncryption';
        break;
      case chrome.bluetoothPrivate.ConnectResultType.OFFSET_INVALID:
        message = 'bluetoothConnectOffsetInvalid';
        break;
      case chrome.bluetoothPrivate.ConnectResultType.READ_NOT_PERMITTED:
        message = 'bluetoothConnectReadNotPermitted';
        break;
      case chrome.bluetoothPrivate.ConnectResultType.REQUEST_NOT_SUPPORTED:
        message = 'bluetoothConnectRequestNotSupported';
        break;
      case chrome.bluetoothPrivate.ConnectResultType.WRITE_NOT_PERMITTED:
        message = 'bluetoothConnectWriteNotPermitted';
        break;
    }
    if (message)
      BluetoothPairing.showMessage({message: message, address: address});
  };

  /**
   * Closes the Bluetooth pairing dialog.
   */
  BluetoothPairing.dismissDialog = function() {
    var overlay = PageManager.getTopmostVisiblePage();
    var dialog = BluetoothPairing.getInstance();
    if (overlay == dialog && dialog.dismissible_) {
      if (dialog.event_)
        dialog.event_.pairing = BluetoothPairingEventType.DISMISSED;
      PageManager.closeOverlay();
    }
  };

  // Export
  return {
    BluetoothPairing: BluetoothPairing
  };
});
