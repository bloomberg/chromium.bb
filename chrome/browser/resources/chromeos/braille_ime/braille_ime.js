// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview Braille hardware keyboard input method.
 *
 * This method is automatically enabled when a braille display is connected
 * and ChromeVox is turned on.  Most of the braille input and editing logic
 * is located in ChromeVox where the braille translation library is available.
 * This IME connects to ChromeVox and communicates using messages as follows:
 *
 * Sent from this IME to ChromeVox:
 * {type: 'activeState', active: boolean}
 * {type: 'inputContext', context: InputContext}
 *   Sent on focus/blur to inform ChromeVox of the type of the current field.
 *   In the latter case (blur), context is null.
 * {type: 'reset'}
 *   Sent when the {code onReset} IME event fires.
 * {type: 'brailleDots', dots: number}
 *   Sent when the user typed a braille cell using the standard keyboard.
 *   ChromeVox treats this similarly to entering braille input using the
 *   braille display.
 *
 * Sent from ChromeVox to this IME:
 * {type: 'replaceText', contextID: number, deleteBefore: number,
 *  newText: string}
 *   Deletes {@code deleteBefore} characters before the cursor (or selection)
 *   and inserts {@code newText}.  {@code contextID} identifies the text field
 *   to apply the update to (no change will happen if focus has moved to a
 *   different field).
 */

/**
 * @constructor
 */
var BrailleIme = function() {};

BrailleIme.prototype = {
  /**
   * Whether to enable extra debug logging for the IME.
   * @const {boolean}
   * @private
   */
  DEBUG: false,

  /**
   * ChromeVox extension ID.
   * @const {string}
   * @private
   */
  CHROMEVOX_EXTENSION_ID_: 'mndnfokpggljbaajbnioimlmbfngpief',

  /**
   * Name of the port used for communication with ChromeVox.
   * @const {string}
   * @private
   */
  PORT_NAME: 'cvox.BrailleIme.Port',

  /**
   * Identifier for the use standard keyboard option used in the menu and
   * {@code localStorage}.  This can be switched on to type braille using the
   * standard keyboard, or off (default) for the usual keyboard behaviour.
   * @const {string}
   */
  USE_STANDARD_KEYBOARD_ID: 'useStandardKeyboard',

  // State related to the support for typing braille using a standrad
  // (querty) keyboard.

  /** @private {boolean} */
  useStandardKeyboard_: false,

  /**
   * Braille dots for keys that are currently pressed.
   * @private {number}
   */
  pressed_: 0,

  /**
   * Dots that have been pressed at some point since {@code pressed_} was last
   * {@code 0}.
   * @private {number}
   */
  accumulated_: 0,

  /**
   * Maps key codes on a standard keyboard to the correspodning dots.
   * Keys on the 'home row' correspond to the keys on a Perkins-style keyboard.
   * Note that the mapping below is arranged like the dots in a braille cell.
   * Only 6 dot input is supported.
   * @private
   * @const {Object.<string, string>}
   */
  CODE_TO_DOT_: {'KeyF': 0x01, 'KeyJ': 0x08,
                 'KeyD': 0x02, 'KeyK': 0x10,
                 'KeyS': 0x04, 'KeyL': 0x20 },

  /**
   * The current engine ID as set by {@code onActivate}, or the empty string if
   * the IME is not active.
   * @type {string}
   * @private
   */
  engineID_: '',

  /**
   * The port used to communicate with ChromeVox.
   * @type {Port} port_
   * @private
   */
  port_: null,

  /**
   * Registers event listeners in the chrome IME API.
   */
  init: function() {
    chrome.input.ime.onActivate.addListener(this.onActivate_.bind(this));
    chrome.input.ime.onDeactivated.addListener(this.onDeactivated_.bind(this));
    chrome.input.ime.onFocus.addListener(this.onFocus_.bind(this));
    chrome.input.ime.onBlur.addListener(this.onBlur_.bind(this));
    chrome.input.ime.onInputContextUpdate.addListener(
        this.onInputContextUpdate_.bind(this));
    chrome.input.ime.onKeyEvent.addListener(this.onKeyEvent_.bind(this));
    chrome.input.ime.onReset.addListener(this.onReset_.bind(this));
    chrome.input.ime.onMenuItemActivated.addListener(
        this.onMenuItemActivated_.bind(this));
    this.connectChromeVox_();
  },

  /**
   * Called by the IME framework when this IME is activated.
   * @param {string} engineID Engine ID, should be 'braille'.
   * @private
   */
  onActivate_: function(engineID) {
    this.log_('onActivate', engineID);
    this.engineID_ = engineID;
    if (!this.port_) {
      this.connectChromeVox_();
    }
    this.useStandardKeyboard_ =
        localStorage[this.USE_STANDARD_KEYBOARD_ID] === String(true);
    this.accumulated_ = 0;
    this.pressed_ = 0;
    this.updateMenuItems_();
    this.sendActiveState_();
  },

  /**
   * Called by the IME framework when this IME is deactivated.
   * @param {string} engineID Engine ID, should be 'braille'.
   * @private
   */
  onDeactivated_: function(engineID) {
    this.log_('onDectivated', engineID);
    this.engineID_ = '';
    this.sendActiveState_();
  },

  /**
   * Called by the IME framework when a text field receives focus.
   * @param {InputContext} context Input field context.
   * @private
   */
  onFocus_: function(context) {
    this.log_('onFocus', JSON.stringify(context));
    this.sendInputContext_(context);
  },

  /**
   * Called by the IME framework when a text field looses focus.
   * @param {number} contextID Input field context ID.
   * @private
   */
  onBlur_: function(contextID) {
    this.log_('onBlur', contextID + '');
    this.sendInputContext_(null);
  },

  /**
   * Called by the IME framework when the current input context is updated.
   * @param {InputContext} context Input field context.
   * @private
   */
  onInputContextUpdate_: function(context) {
    this.log_('onInputContextUpdate', JSON.stringify(context));
    this.sendInputContext_(context);
  },

  /**
   * Called by the system when this IME is active and a key event is generated.
   * @param {string} engineID Engine ID, should be 'braille'.
   * @param {!ChromeKeyboardEvent} event The keyboard event.
   * @return {boolean} Whether the event was handled by this IME (true) or
   *     should be allowed to propagate.
   * @private
   */
  onKeyEvent_: function(engineID, event) {
    this.log_('onKeyEvent', engineID + ', ' + JSON.stringify(event));
    return this.processKey_(event.code, event.type);
  },

  /**
   * Called when chrome ends the current text input session.
   * @param {string} engineID Engine ID, should be 'braille'.
   * @private
   */
  onReset_: function(engineID) {
    this.log_('onReset', engineID);
    this.engineID_ = engineID;
    this.sendToChromeVox_({type: 'reset'});
  },

  /**
   * Called by the IME framework when a menu item is activated.
   * @param {string} engineID Engine ID, should be 'braille'.
   * @param {string} itemID Identifies the menu item.
   * @private
   */
  onMenuItemActivated_: function(engineID, itemID) {
    if (engineID === this.engineID_ &&
        itemID === this.USE_STANDARD_KEYBOARD_ID) {
      this.useStandardKeyboard_ = !this.useStandardKeyboard_;
      localStorage[this.USE_STANDARD_KEYBOARD_ID] =
          String(this.useStandardKeyboard_);
      if (!this.useStandardKeyboard_) {
        this.accumulated_ = 0;
        this.pressed_ = 0;
      }
      this.updateMenuItems_();
    }
  },

  /**
   * Outputs a log message to the console, only if {@link BrailleIme.DEBUG}
   * is set to true.
   * @param {string} func Name of the caller.
   * @param {string} message Message to output.
   * @private
   */
  log_: function(func, message) {
    if (func === 'onKeyEvent') {
      return;
    }
    if (this.DEBUG) {
      console.log('BrailleIme.' + func + ': ' + message);
    }
  },

  /**
   * Handles a querty key on the home row as a braille key.
   * @param {string} code Key code.
   * @param {string} type Type of key event.
   * @return {boolean} Whether the key event was handled or not.
   * @private
   */
  processKey_: function(code, type) {
    if (!this.useStandardKeyboard_) {
      return false;
    }
    var dot = this.CODE_TO_DOT_[code];
    if (!dot) {
      this.pressed_ = 0;
      this.accumulated_ = 0;
      return false;
    }
    if (type === 'keydown') {
      this.pressed_ |= dot;
      this.accumulated_ |= this.pressed_;
      return true;
    } else if (type == 'keyup') {
      this.pressed_ &= ~dot;
      if (this.pressed_ == 0 && this.accumulated_ != 0) {
        this.sendToChromeVox_({type: 'brailleDots', dots: this.accumulated_});
        this.accumulated_ = 0;
      }
      return true;
    }
    return false;
  },

  /**
   * Connects to the ChromeVox extension for message passing.
   * @private
   */
  connectChromeVox_: function() {
    if (this.port_) {
      this.port_.disconnect();
      this.port_ = null;
    }
    this.port_ = chrome.runtime.connect(
        this.CHROMEVOX_EXTENSION_ID_, {name: this.PORT_NAME});
    this.port_.onMessage.addListener(
        this.onChromeVoxMessage_.bind(this));
    this.port_.onDisconnect.addListener(
        this.onChromeVoxDisconnect_.bind(this));
  },

  /**
   * Handles a message from the ChromeVox extension.
   * @param {*} message The message from the extension.
   * @private
   */
  onChromeVoxMessage_: function(message) {
    this.log_('onChromeVoxMessage', JSON.stringify(message));
    message = /** @type {{type: string}} */ (message);
    switch (message.type) {
      case 'replaceText':
        message =
            /**
             * @type {{contextID: number, deleteBefore: number,
             *         newText: string}}
             */
            (message);
        this.replaceText_(message.contextID, message.deleteBefore,
                          message.newText);
        break;
    }
  },

  /**
   * Handles a disconnect event from the ChromeVox side.
   * @private
   */
  onChromeVoxDisconnect_: function() {
    this.port_ = null;
    this.log_('onChromeVoxDisconnect',
              JSON.stringify(chrome.runtime.lastError));
  },

  /**
   * Sends a message to the ChromeVox extension.
   * @param {Object} message The message to send.
   * @private
   */
  sendToChromeVox_: function(message) {
    if (this.port_) {
      this.port_.postMessage(message);
    }
  },

  /**
   * Sends the given input context to ChromeVox.
   * @param {InputContext} context Input context, or null when there's no input
   *    context.
   * @private
   */
  sendInputContext_: function(context) {
    this.sendToChromeVox_({type: 'inputContext', context: context});
  },

  /**
   * Sends the active state to ChromeVox.
   * @private
   */
  sendActiveState_: function() {
    this.sendToChromeVox_({type: 'activeState',
                           active: this.engineID_.length > 0});
  },

  /**
   * Replaces text in the current text field.
   * @param {number} contextID Context for the input field to replace the
   *     text in.
   * @param {number} deleteBefore How many characters to delete before the
   *     cursor.
   * @param {string} toInsert Text to insert at the cursor.
   */
  replaceText_: function(contextID, deleteBefore, toInsert) {
    var addText = function() {
      chrome.input.ime.commitText(
          {contextID: contextID, text: toInsert});
    }.bind(this);
    if (deleteBefore > 0) {
      var deleteText = function() {
        chrome.input.ime.deleteSurroundingText(
            {engineID: this.engineID_, contextID: contextID,
             offset: -deleteBefore, length: deleteBefore}, addText);
      }.bind(this);
      // Make sure there's no non-zero length selection so that
      // deleteSurroundingText works correctly.
      chrome.input.ime.deleteSurroundingText(
          {engineID: this.engineID_, contextID: contextID,
           offset: 0, length: 0}, deleteText);
    } else {
      addText();
    }
  },

  /**
   * Updates the menu items for this IME.
   */
  updateMenuItems_: function() {
    // TODO(plundblad): Localize when translations available.
    chrome.input.ime.setMenuItems(
        {engineID: this.engineID_,
         items: [
           {
             id: this.USE_STANDARD_KEYBOARD_ID,
             label: 'Use standard keyboard for braille',
             style: 'check',
             visible: true,
             checked: this.useStandardKeyboard_,
             enabled: true
             }
         ]
        });
  }
};
