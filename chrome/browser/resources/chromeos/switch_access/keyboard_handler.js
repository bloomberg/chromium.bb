// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(elichtenberg): Move into custom logger class or somewhere else.
let debuggingEnabled = true;

/**
 * Class to handle keyboard input.
 *
 * @constructor
 * @param {SwitchAccessInterface} switchAccess
 */
function KeyboardHandler(switchAccess) {
  /**
   * SwitchAccess reference.
   * @private {SwitchAccessInterface}
   */
  this.switchAccess_ = switchAccess;

  this.init_();
}

KeyboardHandler.prototype = {
  /**
   * Set up key listener.
   *
   * @private
   */
  init_: function() {
    // Capture keycodes for keys 1 through 4, and 6 through 9.
    let keyCodes =
        ['1', '2', '3', '4', '6', '7', '8', '9'].map(key => key.charCodeAt(0));
    chrome.accessibilityPrivate.setSwitchAccessKeys(keyCodes);
    document.addEventListener('keyup', this.handleKeyEvent_.bind(this));
  },

  /**
   * Handle a keyboard event by calling the appropriate SwitchAccess functions.
   *
   * @param {!Event} event
   * @private
   */
  handleKeyEvent_: function(event) {
    switch (event.key) {
      case '1':
        console.log('1 = go to next element');
        this.switchAccess_.moveToNode(true);
        break;
      case '2':
        console.log('2 = go to previous element');
        this.switchAccess_.moveToNode(false);
        break;
      case '3':
        console.log('3 = select element');
        this.switchAccess_.selectCurrentNode();
        break;
      case '4':
        this.switchAccess_.showOptionsPage();
        break;
    }
    if (debuggingEnabled) {
      switch (event.key) {
        case '6':
          console.log('6 = go to next element (debug mode)');
          this.switchAccess_.debugMoveToNext();
          break;
        case '7':
          console.log('7 = go to previous element (debug mode)');
          this.switchAccess_.debugMoveToPrevious();
          break;
        case '8':
          console.log('8 = go to child element (debug mode)');
          this.switchAccess_.debugMoveToFirstChild();
          break;
        case '9':
          console.log('9 = go to parent element (debug mode)');
          this.switchAccess_.debugMoveToParent();
          break;
      }
    }
    this.switchAccess_.performedUserAction();
  }
};
