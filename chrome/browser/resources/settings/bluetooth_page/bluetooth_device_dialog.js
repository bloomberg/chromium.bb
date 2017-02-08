// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

var PairingEventType = chrome.bluetoothPrivate.PairingEventType;

// NOTE(dbeam): even though this behavior is only used privately, it must
// be globally accessible for Closure's --polymer_pass to compile happily.

/** @polymerBehavior */
settings.BluetoothPairDeviceBehavior = {
  properties: {
    /**
     * Current Pairing device.
     * @type {!chrome.bluetooth.Device|undefined}
     */
    pairingDevice: Object,

    /**
     * Current Pairing event.
     * @type {?chrome.bluetoothPrivate.PairingEvent}
     */
    pairingEvent_: {
      type: Object,
      value: null,
    },

    /** Pincode or passkey value, used to trigger connect enabled changes. */
    pinOrPass: String,

    /**
     * Interface for bluetoothPrivate calls. Set in bluetooth-page.
     * @type {BluetoothPrivate}
     * @private
     */
    bluetoothPrivate: {
      type: Object,
      value: chrome.bluetoothPrivate,
    },

    /**
     * @const
     * @type {!Array<number>}
     */
    digits: {
      type: Array,
      readOnly: true,
      value: [0, 1, 2, 3, 4, 5],
    },
  },

  observers: [
    'pairingChanged_(pairingDevice, pairingEvent_)',
  ],

  /**
   * Listener for chrome.bluetoothPrivate.onPairing events.
   * @type {?function(!chrome.bluetoothPrivate.PairingEvent)}
   * @private
   */
  bluetoothPrivateOnPairingListener_: null,

  /** Called when the dialog is opened. Starts listening for pairing events. */
  startPairing: function() {
    if (!this.bluetoothPrivateOnPairingListener_) {
      this.bluetoothPrivateOnPairingListener_ =
          this.onBluetoothPrivateOnPairing_.bind(this);
      this.bluetoothPrivate.onPairing.addListener(
          this.bluetoothPrivateOnPairingListener_);
    }
  },

  /** Called when the dialog is closed. */
  endPairing: function() {
    if (this.bluetoothPrivateOnPairingListener_) {
      this.bluetoothPrivate.onPairing.removeListener(
          this.bluetoothPrivateOnPairingListener_);
      this.bluetoothPrivateOnPairingListener_ = null;
    }
    this.pairingEvent_ = null;
  },

  /**
   * Process bluetoothPrivate.onPairing events.
   * @param {!chrome.bluetoothPrivate.PairingEvent} event
   * @private
   */
  onBluetoothPrivateOnPairing_: function(event) {
    if (!this.pairingDevice ||
        event.device.address != this.pairingDevice.address) {
      return;
    }
    if (event.pairing == PairingEventType.KEYS_ENTERED &&
        event.passkey === undefined && this.pairingEvent_) {
      // 'keysEntered' event might not include the updated passkey so preserve
      // the current one.
      event.passkey = this.pairingEvent_.passkey;
    }
    this.pairingEvent_ = event;
  },

  /** @private */
  pairingChanged_: function() {
    // Auto-close the dialog when pairing completes.
    if (this.pairingDevice.paired && !this.pairingDevice.connecting &&
        this.pairingDevice.connected) {
      this.close();
      return;
    }
    this.pinOrPass = '';
  },

  /**
   * @return {string}
   * @private
   */
  getMessage_: function() {
    var message;
    if (!this.pairingEvent_)
      message = 'bluetoothStartConnecting';
    else
      message = this.getEventDesc_(this.pairingEvent_.pairing);
    return this.i18n(message, this.pairingDevice.name);
  },

  /**
   * @return {boolean}
   * @private
   */
  showEnterPincode_: function() {
    return !!this.pairingEvent_ &&
        this.pairingEvent_.pairing == PairingEventType.REQUEST_PINCODE;
  },

  /**
   * @return {boolean}
   * @private
   */
  showEnterPasskey_: function() {
    return !!this.pairingEvent_ &&
        this.pairingEvent_.pairing == PairingEventType.REQUEST_PASSKEY;
  },

  /**
   * @return {boolean}
   * @private
   */
  showDisplayPassOrPin_: function() {
    if (!this.pairingEvent_)
      return false;
    var pairing = this.pairingEvent_.pairing;
    return (
        pairing == PairingEventType.DISPLAY_PINCODE ||
        pairing == PairingEventType.DISPLAY_PASSKEY ||
        pairing == PairingEventType.CONFIRM_PASSKEY ||
        pairing == PairingEventType.KEYS_ENTERED);
  },

  /**
   * @return {boolean}
   * @private
   */
  showAcceptReject_: function() {
    return !!this.pairingEvent_ &&
        this.pairingEvent_.pairing == PairingEventType.CONFIRM_PASSKEY;
  },

  /**
   * @return {boolean}
   * @private
   */
  showConnect_: function() {
    if (!this.pairingEvent_)
      return false;
    var pairing = this.pairingEvent_.pairing;
    return pairing == PairingEventType.REQUEST_PINCODE ||
        pairing == PairingEventType.REQUEST_PASSKEY;
  },

  /**
   * @return {boolean}
   * @private
   */
  enableConnect_: function() {
    if (!this.showConnect_())
      return false;
    var inputId =
        (this.pairingEvent_.pairing == PairingEventType.REQUEST_PINCODE) ?
        '#pincode' :
        '#passkey';
    var paperInput = /** @type {!PaperInputElement} */ (this.$$(inputId));
    assert(paperInput);
    /** @type {string} */ var value = paperInput.value;
    return !!value && paperInput.validate();
  },

  /**
   * @return {boolean}
   * @private
   */
  showDismiss_: function() {
    return this.pairingDevice.paired ||
        (!!this.pairingEvent_ &&
         this.pairingEvent_.pairing == PairingEventType.COMPLETE);
  },

  /** @private */
  onAcceptTap_: function() {
    this.sendResponse_(chrome.bluetoothPrivate.PairingResponse.CONFIRM);
  },

  /** @private */
  onConnectTap_: function() {
    this.sendResponse_(chrome.bluetoothPrivate.PairingResponse.CONFIRM);
  },

  /** @private */
  onRejectTap_: function() {
    this.sendResponse_(chrome.bluetoothPrivate.PairingResponse.REJECT);
  },

  /**
   * @param {!chrome.bluetoothPrivate.PairingResponse} response
   * @private
   */
  sendResponse_: function(response) {
    if (!this.pairingDevice)
      return;
    var options =
        /** @type {!chrome.bluetoothPrivate.SetPairingResponseOptions} */ {
          device: this.pairingDevice,
          response: response
        };
    if (response == chrome.bluetoothPrivate.PairingResponse.CONFIRM) {
      var pairing = this.pairingEvent_.pairing;
      if (pairing == PairingEventType.REQUEST_PINCODE)
        options.pincode = this.$$('#pincode').value;
      else if (pairing == PairingEventType.REQUEST_PASSKEY)
        options.passkey = parseInt(this.$$('#passkey').value, 10);
    }
    this.bluetoothPrivate.setPairingResponse(options, function() {
      if (chrome.runtime.lastError) {
        // TODO(stevenjb): Show error.
        console.error(
            'Error setting pairing response: ' + options.device.name +
            ': Response: ' + options.response +
            ': Error: ' + chrome.runtime.lastError.message);
      }
      this.close();
    }.bind(this));

    this.fire('response', options);
  },

  /**
   * @param {!PairingEventType} eventType
   * @return {string}
   * @private
   */
  getEventDesc_: function(eventType) {
    assert(eventType);
    if (eventType == PairingEventType.COMPLETE ||
        eventType == PairingEventType.KEYS_ENTERED ||
        eventType == PairingEventType.REQUEST_AUTHORIZATION) {
      return 'bluetoothStartConnecting';
    }
    return 'bluetooth_' + /** @type {string} */ (eventType);
  },

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getPinDigit_: function(index) {
    if (!this.pairingEvent_)
      return '';
    var digit = '0';
    var pairing = this.pairingEvent_.pairing;
    if (pairing == PairingEventType.DISPLAY_PINCODE &&
        this.pairingEvent_.pincode &&
        index < this.pairingEvent_.pincode.length) {
      digit = this.pairingEvent_.pincode[index];
    } else if (
        this.pairingEvent_.passkey &&
        (pairing == PairingEventType.DISPLAY_PASSKEY ||
         pairing == PairingEventType.KEYS_ENTERED ||
         pairing == PairingEventType.CONFIRM_PASSKEY)) {
      var passkeyString = String(this.pairingEvent_.passkey);
      if (index < passkeyString.length)
        digit = passkeyString[index];
    }
    return digit;
  },

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getPinClass_: function(index) {
    if (!this.pairingEvent_)
      return '';
    if (this.pairingEvent_.pairing == PairingEventType.CONFIRM_PASSKEY)
      return 'confirm';
    var cssClass = 'display';
    if (this.pairingEvent_.pairing == PairingEventType.DISPLAY_PASSKEY) {
      if (index == 0)
        cssClass += ' next';
      else
        cssClass += ' untyped';
    } else if (
        this.pairingEvent_.pairing == PairingEventType.KEYS_ENTERED &&
        this.pairingEvent_.enteredKey) {
      var enteredKey = this.pairingEvent_.enteredKey;  // 1-7
      var lastKey = this.digits.length;                // 6
      if ((index == -1 && enteredKey > lastKey) || (index + 1 == enteredKey))
        cssClass += ' next';
      else if (index > enteredKey)
        cssClass += ' untyped';
    }
    return cssClass;
  },
};

Polymer({
  is: 'bluetooth-device-dialog',

  behaviors: [
    I18nBehavior,
    CrScrollableBehavior,
    settings.BluetoothPairDeviceBehavior,
  ],

  properties: {
    /**
     * The version of this dialog to show: 'pairDevice', or 'connectError'.
     * Must be set before the dialog is opened.
     */
    dialogId: String,
  },

  observers: [
    'dialogUpdated_(dialogId, pairingEvent_)',
  ],

  open: function() {
    this.startPairing();
    this.pinOrPass = '';
    this.getDialog_().showModal();
    this.itemWasFocused_ = false;
  },

  close: function() {
    this.endPairing();
    var dialog = this.getDialog_();
    if (dialog.open)
      dialog.close();
  },

  /** @private */
  dialogUpdated_: function() {
    if (this.showEnterPincode_())
      this.$$('#pincode').focus();
    else if (this.showEnterPasskey_())
      this.$$('#passkey').focus();
  },

  /**
   * @return {!CrDialogElement}
   * @private
   */
  getDialog_: function() {
    return /** @type {!CrDialogElement} */ (this.$.dialog);
  },

  /**
   * @param {string} desiredDialogType
   * @return {boolean}
   * @private
   */
  isDialogType_: function(desiredDialogType, currentDialogType) {
    return currentDialogType == desiredDialogType;
  },

  /** @private */
  onCancelTap_: function() {
    this.getDialog_().cancel();
  },

  /** @private */
  onDialogCanceled_: function() {
    if (this.dialogId == 'pairDevice')
      this.sendResponse_(chrome.bluetoothPrivate.PairingResponse.CANCEL);
    this.endPairing();
  },
});
