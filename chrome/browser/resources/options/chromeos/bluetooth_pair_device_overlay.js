// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

  /**
   * Enumeration of possible states during pairing.  The value associated with
   * each state maps to a loalized string in the global variable
   * 'templateData'.
   * @enum {string}
   */
  const PAIRING = {
    CONFIRM_PASSKEY: 'bluetoothConfirmPasskey',
    ENTER_PASSKEY: 'bluetoothEnterPasskey',
    FAILED_CONNECT: 'bluetoothFailedPairingInstructions',
    REMOTE_PASSKEY: 'bluetoothRemotePasskey'
  };

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
      $('bluetooth-passkey').oninput = function() {
        $('bluetooth-pair-device-connect-button').disabled =
            $('bluetooth-passkey').value.length == 0;
      }
    },

    /**
     * Override to prevent showing the overlay if the Bluetooth device details
     * have not been specified.  Prevents showing an empty dialog if the user
     * quits and restarts Chrome while in the process of pairing with a device.
     " @return {boolean} True if the overlay can be displayed.
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
      if (this.device_.passkey) {
        this.updatePasskey_();
        $('bluetooth-pairing-passkey-display').hidden = false;
        $('bluetooth-pairing-passkey-entry').hidden = true;
        $('bluetooth-pair-device-connect-button').hidden = true;
        if (this.device_.pairing == PAIRING.CONFIRM_PASSKEY) {
          // Display 'Accept' and 'Reject' buttons when confirming a match
          // between displayed passkeys.
          $('bluetooth-pair-device-accept-button').hidden = false;
          $('bluetooth-pair-device-reject-button').hidden = false;
          $('bluetooth-pair-device-cancel-button').hidden = true;
        } else {
          // Only display the 'Cancel' button for when remote entering a
          // passkey.
          $('bluetooth-pair-device-accept-button').hidden = true;
          $('bluetooth-pair-device-reject-button').hidden = true;
          $('bluetooth-pair-device-cancel-button').hidden = false;
        }
      } else if (this.device_.pairing == PAIRING.ENTER_PASSKEY) {
        // Display 'Connect' and 'Cancel' buttons when prompted to enter a
        // passkey.
        $('bluetooth-pairing-passkey-display').hidden = true;
        $('bluetooth-pairing-passkey-entry').hidden = false;
        $('bluetooth-pair-device-connect-button').hidden = false;
        $('bluetooth-pair-device-cancel-button').hidden = false;
      }
      $('bluetooth-pair-device-connect-button').disabled =
          $('bluetooth-passkey').value.length == 0;
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
     * @return {Element} Element containing the passkey.
     */
    updatePasskey_: function() {
      var passkeyEl = $('bluetooth-pairing-passkey-display');
      this.clearElement_(passkeyEl);
      var key = String(this.device_.passkey);
      var progress = this.device_.entered | 0;
      for (var i = 0; i < key.length; i++) {
        var keyEl = document.createElement('span');
        keyEl.textContent = key.charAt(i);
        keyEl.className = 'bluetooth-passkey-char';
        if (i < progress)
          keyEl.classList.add('key-typed');
        if (i == progress - 1)
          keyEl.classList.add('last-key-typed');
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
