// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  /**
   * Enumeration of possible states during pairing.  The value associated with
   * each state maps to a localized string in the global variable
   * 'templateData'.
   * @enum {string}
   */
  var PAIRING = {
    ENTER_PIN_CODE: 'bluetoothEnterPinCode',
    ENTER_PASSKEY: 'bluetoothEnterPasskey',
    REMOTE_PIN_CODE: 'bluetoothRemotePinCode',
    REMOTE_PASSKEY: 'bluetoothRemotePasskey',
    CONFIRM_PASSKEY: 'bluetoothConfirmPasskey',
    ERROR_NO_DEVICE: 'bluetoothErrorNoDevice',
    ERROR_INCORRECT_PIN: 'bluetoothErrorIncorrectPin',
    ERROR_CONNECTION_TIMEOUT: 'bluetoothErrorTimeout',
    ERROR_CONNECTION_FAILED: 'bluetoothErrorConnectionFailed'
  };

  /**
   * List of IDs for conditionally visible elements in the dialog.
   * @type {Array.<String>}
   * @const
   */
  var ELEMENTS = ['bluetooth-pairing-passkey-display',
                  'bluetooth-pairing-passkey-entry',
                  'bluetooth-pair-device-connect-button',
                  'bluetooth-pair-device-cancel-button',
                  'bluetooth-pair-device-accept-button',
                  'bluetooth-pair-device-reject-button',
                  'bluetooth-pair-device-dismiss-button'];


  /**
   * Encapsulated handling of the Bluetooth device pairing page.
   * @constructor
   */
  function BluetoothPairing() {
    OptionsPage.call(this,
                     'bluetoothPairing',
                     templateData.bluetoothOptionsPageTabTitle,
                     'bluetooth-pairing');
  }

  cr.addSingletonGetter(BluetoothPairing);

  BluetoothPairing.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Description of the bluetooth device.
     * @type {{name: string,
     *         address: string,
     *         icon: Constants.DEVICE_TYPE,
     *         paired: boolean,
     *         connected: boolean,
     *         pairing: string|undefined,
     *         passkey: number|undefined,
     *         pincode: string|undefined,
     *         entered: number|undefined}}
     * @private.
     */
    device_: null,

     /** @inheritDoc */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);
      var self = this;
      var cancel = function() {
        chrome.send('updateBluetoothDevice',
                    [self.device_.address, 'cancel']);
        OptionsPage.closeOverlay();
      };
      var connect = function() {
        var args = [self.device_.address, 'connect'];
        var passkey = self.device_.passkey;
        if (!passkey && !$('bluetooth-pairing-passkey-entry').hidden)
          passkey = $('bluetooth-passkey').value;
        if (passkey)
          args.push(String(passkey));
        chrome.send('updateBluetoothDevice', args);
        OptionsPage.closeOverlay();
      };
      $('bluetooth-pair-device-cancel-button').onclick = cancel;
      $('bluetooth-pair-device-reject-button').onclick = cancel;
      $('bluetooth-pair-device-connect-button').onclick = connect;
      $('bluetooth-pair-device-accept-button').onclick = connect;
      $('bluetooth-pair-device-dismiss-button').onclick = function() {
        OptionsPage.closeOverlay();
      };
      $('bluetooth-passkey').oninput = function() {
        $('bluetooth-pair-device-connect-button').disabled =
            $('bluetooth-passkey').value.length == 0;
      }
    },

    /**
     * Override to prevent showing the overlay if the Bluetooth device details
     * have not been specified.  Prevents showing an empty dialog if the user
     * quits and restarts Chrome while in the process of pairing with a device.
     * @return {boolean} True if the overlay can be displayed.
     */
    canShowPage: function() {
      return this.device_ && this.device_.address && this.device_.pairing;
    },

    /**
     * Configures the overlay for pairing a device.
     * @param {Object} device Description of the bluetooth device.
     */
    update: function(device) {
      this.device_ = {};
      for (key in device)
        this.device_[key] = device[key];
      // Update the pairing instructions.
      var instructionsEl = $('bluetooth-pairing-instructions');
      this.clearElement_(instructionsEl);

      var message = templateData[device.pairing];
      message = message.replace('%1', this.device_.name);
      instructionsEl.textContent = message;

      // Update visibility of dialog elements.
      if (this.device_.passkey) {
        this.updatePasskey_();
        if (this.device_.pairing == PAIRING.CONFIRM_PASSKEY) {
          // Confirming a match between displayed passkeys.
          this.displayElements_(['bluetooth-pairing-passkey-display',
                                 'bluetooth-pair-device-accept-button',
                                 'bluetooth-pair-device-reject-button']);
        } else {
          // Remote entering a passkey.
          this.displayElements_(['bluetooth-pairing-passkey-display',
                                 'bluetooth-pair-device-cancel-button']);
        }
      } else if (this.device_.pincode) {
        this.updatePinCode_();
        this.displayElements_(['bluetooth-pairing-passkey-display',
                               'bluetooth-pair-device-cancel-button']);
      } else if (this.device_.pairing == PAIRING.ENTER_PIN_CODE) {
        // Prompting the user to enter a PIN code.
        this.displayElements_(['bluetooth-pairing-passkey-entry',
                               'bluetooth-pair-device-connect-button',
                               'bluetooth-pair-device-cancel-button']);
      } else if (this.device_.pairing == PAIRING.ENTER_PASSKEY) {
        // Prompting the user to enter a passkey.
        this.displayElements_(['bluetooth-pairing-passkey-entry',
                               'bluetooth-pair-device-connect-button',
                               'bluetooth-pair-device-cancel-button']);
      } else {
        // Displaying an error message.
        this.displayElements_(['bluetooth-pair-device-dismiss-button']);
      }
      $('bluetooth-pair-device-connect-button').disabled =
          $('bluetooth-passkey').value.length == 0;
    },

    /**
     * Updates the visibility of elements in the dialog.
     * @param {Array.<string>} list List of conditionally visible elements that
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
     * Formats an element for displaying the passkey.
     */
    updatePasskey_: function() {
      var passkeyEl = $('bluetooth-pairing-passkey-display');
      var keyClass = this.device_.pairing == PAIRING.REMOTE_PASSKEY ?
          'bluetooth-keyboard-button' : 'bluetooth-passkey-char';
      this.clearElement_(passkeyEl);
      var key = String(this.device_.passkey);
      var progress = this.device_.entered | 0;
      for (var i = 0; i < key.length; i++) {
        var keyEl = document.createElement('span');
        keyEl.textContent = key.charAt(i);
        keyEl.className = keyClass;
        if (i < progress)
          keyEl.classList.add('key-typed');
        passkeyEl.appendChild(keyEl);
      }
      if (this.device_.pairing == PAIRING.REMOTE_PASSKEY) {
        // Add enter key.
        var label = templateData['bluetoothEnterKey'];
        var keyEl = document.createElement('span');
        keyEl.textContent = label;
        keyEl.className = keyClass;
        keyEl.id = 'bluetooth-enter-key';
        passkeyEl.appendChild(keyEl);
      }
      passkeyEl.hidden = false;
    },

    /**
     * Formats an element for displaying the PIN code.
     */
    updatePinCode_: function() {
      var passkeyEl = $('bluetooth-pairing-passkey-display');
      var keyClass = this.device_.pairing == PAIRING.REMOTE_PIN_CODE ?
          'bluetooth-keyboard-button' : 'bluetooth-passkey-char';
      this.clearElement_(passkeyEl);
      var key = String(this.device_.pincode);
      for (var i = 0; i < key.length; i++) {
        var keyEl = document.createElement('span');
        keyEl.textContent = key.charAt(i);
        keyEl.className = keyClass;
        keyEl.classList.add('key-pin');
        passkeyEl.appendChild(keyEl);
      }
      if (this.device_.pairing == PAIRING.REMOTE_PIN_CODE) {
        // Add enter key.
        var label = templateData['bluetoothEnterKey'];
        var keyEl = document.createElement('span');
        keyEl.textContent = label;
        keyEl.className = keyClass;
        keyEl.classList.add('key-pin');
        keyEl.id = 'bluetooth-enter-key';
        passkeyEl.appendChild(keyEl);
      }
      passkeyEl.hidden = false;
    },
  };

  /**
   * Configures the device pairing instructions and displays the pairing
   * overlay.
   * @param {Object} device Description of the Bluetooth device.
   */
  BluetoothPairing.showDialog = function(device) {
    BluetoothPairing.getInstance().update(device);
    OptionsPage.navigateToPage('bluetoothPairing');
  };

  // Export
  return {
    BluetoothPairing: BluetoothPairing
  };
});
