// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe HID detection screen implementation.
 */

login.createScreen('HIDDetectionScreen', 'hid-detection', function() {
  return {
    EXTERNAL_API: [
      'setPointingDeviceState',
      'setKeyboardDeviceState',
    ],

  /**
   * Enumeration of possible states during pairing.  The value associated with
   * each state maps to a localized string in the global variable
   * |loadTimeData|.
   * @enum {string}
   */
   PAIRING: {
     STARTUP: 'bluetoothStartConnecting',
     REMOTE_PIN_CODE: 'bluetoothRemotePinCode',
     REMOTE_PASSKEY: 'bluetoothRemotePasskey',
     CONNECT_FAILED: 'bluetoothConnectFailed',
     CANCELED: 'bluetoothPairingCanceled',
     // Pairing dismissed (succeeded or canceled).
     DISMISSED: 'bluetoothPairingDismissed'
   },

   // Enumeration of possible connection states of a device.
   CONNECTION: {
     SEARCHING: 'searching',
     CONNECTED: 'connected',
     PAIRING: 'pairing',
     PAIRED: 'paired',
     // Special info state.
     UPDATE: 'update'
   },

   // Possible ids of device blocks.
   BLOCK: {
     MOUSE: 'hid-mouse-block',
     KEYBOARD: 'hid-keyboard-block'
   },

    /**
     * Button to move to usual OOBE flow after detection.
     * @private
     */
    continueButton_: null,

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];
      var continueButton = this.ownerDocument.createElement('button');
      continueButton.id = 'hid-continue-button';
      continueButton.textContent = loadTimeData.getString(
          'hidDetectionContinue');
      continueButton.addEventListener('click', function(e) {
        chrome.send('HIDDetectionOnContinue');
        e.stopPropagation();
      });
      buttons.push(continueButton);
      this.continueButton_ = continueButton;

      return buttons;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return this.continueButton_;
    },

    /**
     * Sets a device-block css class to reflect device state of searching,
     * connected, pairing or paired (for BT devices).
     * @param {blockId} id one of keys of this.BLOCK dict.
     * @param {state} one of keys of this.CONNECTION dict.
     * @private
     */
    setDeviceBlockState_: function(blockId, state) {
      if (state == 'update')
        return;
      var deviceBlock = $(blockId);
      for (var stateCase in this.CONNECTION)
        deviceBlock.classList.toggle(stateCase, stateCase == state);

      // 'Continue' button available iff at least one device is connected,
      if ((blockId in this.BLOCK) &&
          (state == this.CONNECTION.CONNECTED ||
           state == this.CONNECTION.PAIRED)) {
        $('hid-continue-button').disabled = false;
      } else {
        $('hid-continue-button').disabled = true;
      }
    },

    /**
     * Sets state for mouse-block.
     * @param {state} one of keys of this.CONNECTION dict.
     */
    setPointingDeviceState: function(state) {
      if (state === undefined)
        return;
      this.setDeviceBlockState_(this.BLOCK.MOUSE, state);
    },

    /**
     * Sets state for pincode key elements.
     * @param {entered} int, number of typed keys of pincode, -1 if keys press
     * detection is not supported by device.
     */
    setPincodeKeysState_: function(entered) {
      var pincodeLength = 7; // including enter-key
      for (var i = 0; i < pincodeLength; i++) {
        var pincodeSymbol = $('hid-keyboard-pincode-sym-' + (i + 1));
        pincodeSymbol.classList.remove('key-typed');
        pincodeSymbol.classList.remove('key-untyped');
        pincodeSymbol.classList.remove('key-next');
        if (i < entered)
          pincodeSymbol.classList.add('key-typed');
        else if (i == entered)
          pincodeSymbol.classList.add('key-next');
        else if (entered != -1)
          pincodeSymbol.classList.add('key-untyped');
      }
    },

    /**
     * Sets state for keyboard-block.
     * @param {data} dict with parameters.
     */
    setKeyboardDeviceState: function(data) {
      if (data === undefined || !('state' in data))
        return;
      var state = data['state'];
      this.setDeviceBlockState_(this.BLOCK.KEYBOARD, state);
      if (state == this.CONNECTION.PAIRED)
        $('hid-keyboard-label-paired').textContent = data['keyboard-label'];
      else if (state == this.CONNECTION.PAIRING) {
        $('hid-keyboard-label-pairing').textContent = data['keyboard-label'];
        if (data['pairing-state'] == this.PAIRING.REMOTE_PIN_CODE ||
            data['pairing-state'] == this.PAIRING.REMOTE_PASSKEY) {
          this.setPincodeKeysState_(-1);
          for (var i = 0, len = data['pincode'].length; i < len; i++) {
            var pincodeSymbol = $('hid-keyboard-pincode-sym-' + (i + 1));
            pincodeSymbol.textContent = data['pincode'][i];
          }
          announceAccessibleMessage(
              data['keyboard-label'] + ' ' + data['pincode'] + ' ' +
              loadTimeData.getString('hidDetectionBTEnterKey'));
        }
      } else if (state == this.CONNECTION.UPDATE) {
        if ('keysEntered' in data) {
          this.setPincodeKeysState_(data['keysEntered']);
        }
      }
    },
  };
});
