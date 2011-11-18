// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An input method UI implementation
 */

/**
 * Const variables
 */
const IME_KEYBOARD_HEIGHT = 300;
const IME_HEIGHT = 48;
const IME_VMARGIN = 12;
const IME_HMARGIN = 16;
const IME_FONTSIZE = IME_HEIGHT - IME_VMARGIN * 2;
const IME_MAX_CANDIDATES = 20;

/**
 * Creates a new button element.
 * @param {Object=} opt_propertyBag Optional properties.
 * @constructor
 * @extends {HTMLButtonElement}
 */
var Button = cr.ui.define('button');

Button.prototype = {
  __proto__: HTMLButtonElement.prototype,
  /**
   * Initialized the button element.
   */
  decorate: function() {
    this.className = 'ime button';

    this.style.height = IME_HEIGHT + 'px';
    this.style.paddingTop =
      this.style.paddingBottom = IME_VMARGIN + 'px';
    this.style.paddingLeft =
      this.style.paddingRight = IME_HMARGIN + 'px';
    this.style.fontSize = IME_FONTSIZE + 'px';

    this.tabIndex = -1;
    this.text = '';
  },

  /**
   * Returns the text of the button
   * @return {string}
   */
  get text() {
    return this.innerHTML;
  },

  /**
   * Sets the HTML of the button
   * @param {string} text in button in HTML format. If text is empty, the
   * button will be hidden.
   * @return {void}
   */
  set text(text) {
    this.innerHTML = text;
    if (text) {
      this.style.display = 'inline-block';
    } else {
      this.style.display = 'none';
    }
  },
};

/**
 * An input method UI
 * @param {Object=} opt_propertyBag Optional properties.
 * @constructor
 * @extends {HTMLDivElement}
 */
var ImeUi = cr.ui.define('div');

ImeUi.prototype = {
  __proto__: HTMLDivElement.prototype,

  /**
   * Initialized the button element.
   */
  decorate: function() {
    this.className = 'ime panel';
    this.style.height = IME_HEIGHT + 'px';

    this.auxiliary_ = new Button();
    this.auxiliary_.className += ' auxiliary';
    this.appendChild(this.auxiliary_);

    var that = this;
    this.pageUp_ = new Button({'text': ' &laquo; '});
    this.pageUp_.onclick = function() {
      that.pageUp();
    };

    this.pageDown_ = new Button({'text': ' &raquo; '});
    this.pageDown_.onclick = function() {
      that.pageDown();
    };

    this.appendChild(this.pageUp_);

    // Create candidates
    this.candidates_ = [];
    /**
     * Creates a candidate element. We need this function to create a new scope
     * for each candidate buttons, so that they may have different value of
     * index.
     * @param {number} index
     */
    function createCandidateButton(index) {
      candidate = new Button();
      candidate.className += ' candidate';
      that.candidates_.push(candidate);
      that.appendChild(candidate);
      candidate.onclick = function() {
        that.candidateClicked(index);
      };
    }
    for (var i = 0; i < IME_MAX_CANDIDATES; i++) {
      createCandidateButton(i);
    }

    this.appendChild(this.pageDown_);
    this.update();
  },

  /**
   * Returns true if Ime Ui is visible.
   * @return {boolean}
   */
  get visible() {
    return this.style.display != 'none';
  },

  /**
   * Sets the input cursor location.
   * @param {number} x The x of the input cursor coordinates.
   * @param {number} y The y of the input cursor coordinates.
   * @param {number} w The width of the input cursor.
   * @param {number} h The height of the input cursor.
   * @return {void}
   */
  setCursorLocation: function(x, y, w, h) {
    // Currently we do nothing.
  },

  /**
   * Updates the auxiliary text.
   * @param {string} text The auxiliary text.
   * @return {void}
   */
  updateAuxiliaryText: function(text) {
    this.auxiliary_.text = text;
    this.update();
  },

  /**
   * Updates the lookup table.
   * @param {Object} table The lookup table.
   * @return {void}
   */
  updateLookupTable : function(table) {
    if (table.visible) {
      for (var i = 0;
          i < IME_MAX_CANDIDATES && i < table.candidates.length; i++) {
        this.candidates_[i].text = table.candidates[i];
      }
      for (; i < IME_MAX_CANDIDATES; i++) {
        this.candidates_[i].text = '';
      }
    } else {
      for (var i = 0; i < this.candidates_.length; i++) {
        this.candidates_[i].text = '';
      }
    }
    this.table_ = table;
    this.update();
  },

  update : function() {
    var visible =
      (this.table_ && this.table_.visible) || this.auxiliary_.text_;

    if (visible == this.visible) {
      return;
    }

    if (visible) {
      chrome.experimental.input.setKeyboardHeight(
        IME_KEYBOARD_HEIGHT + IME_HEIGHT);
      this.style.display = 'inline-block';
    } else {
      this.style.display = 'none';
      chrome.experimental.input.setKeyboardHeight(IME_KEYBOARD_HEIGHT);
    }
    // TODO(penghuang) Adjust the width of all items in ImeUi to fill the width
    // of keyboard.
  },

  /**
   * Candidate is clicked.
   * @param {number} index The index of the candidate.
   * @return {void}
   */
  candidateClicked: function(index) {
    chrome.experimental.input.ui.candidateClicked(index, 1);
  },

  /**
   * Go to the previous page of the lookup table.
   * @return {void}
   */
  pageUp: function() {
    chrome.experimental.input.ui.pageUp();
  },

  /**
   * Go to the next page of the lookup table.
   * @return {void}
   */
  pageDown: function() {
    chrome.experimental.input.ui.pageDown();
  },
};

var imeui = null;

/**
 * Initialize Ime Ui
 * @param {Element} element The html element which will contain the ImeUi.
 * @return {void}
 */
function initIme(element) {
  if (imeui) {
    // ime ui has been initialized.
    return;
  }

  try {
    // Register self to receive input method UI events.
    chrome.experimental.input.ui.register();
  } catch (e) {
    // The ime is not enabled in chromium.
    return;
  }

  imeui = new ImeUi();

  element.appendChild(imeui);

  // new row
  var clearingDiv = document.createElement('div');
  clearingDiv.style.clear = 'both';
  element.appendChild(clearingDiv);

  // Install events handlers.
  chrome.experimental.input.ui.onSetCursorLocation.addListener(
      function(x, y, w, h) {
        imeui.setCursorLocation(x, y, w, h);
      });
  chrome.experimental.input.ui.onUpdateAuxiliaryText.addListener(
      function(text) {
        imeui.updateAuxiliaryText(text);
      });
  chrome.experimental.input.ui.onUpdateLookupTable.addListener(
      function(table) {
        imeui.updateLookupTable(table);
      });
}

/**
 * Updates Ime Ui. It should be called when window is resized.
 * @return {void}
 */
function updateIme() {
  if (imeui) {
    imeui.update();
  }
}
