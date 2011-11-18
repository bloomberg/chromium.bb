// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A handwriting virtual keyboard implementation.
 */

/**
 * Const variables
 */
const HANDWRITING_CANVAS_LINE_COLOR = '#ffffff';
const HANDWRITING_CANVAS_LINE_WIDTH = 5;
const HANDWRITING_CANVAS_MOUSEMOVE_THRESHOLD_SQ = 15 * 15;  // 15px.
const HANDWRITING_CANVAS_ASPECT = 1.0;
const HANDWRITING_CANVAS_ELEMENT_ID = 'handwriting-canvas';

/**
 * The key that clears the canvas for handwriting.
 * @param {number} aspect The aspect ratio of the key.
 * @constructor
 * @extends {BaseKey}
 */
function ClearHandwritingKey(content) {
  this.modeElements_ = {}
  this.cellType_ = 'nc';
  this.content_ = content;
}

ClearHandwritingKey.prototype = {
  __proto__: BaseKey.prototype,

  /** @inheritDoc */
  makeDOM: function(mode, height) {
    this.modeElements_[mode] = document.createElement('div');
    this.modeElements_[mode].className = 'key handwriting-clear';
    addContent(this.modeElements_[mode], this.content_);

    this.modeElements_[mode].onclick = function() {
      var canvas = document.getElementById(HANDWRITING_CANVAS_ELEMENT_ID);
      canvas.clear();
    };

    return this.modeElements_[mode];
  }
};

/**
 * The canvas element for handwriting.
 * @param {Object=} opt_propertyBag Optional properties.
 * @constructor
 * @extends {HTMLCanvasElement}
 */
var HandwritingCanvas = cr.ui.define('canvas');

HandwritingCanvas.prototype = {
  __proto__: HTMLCanvasElement.prototype,

  /**
   * Creates the DOM element for the canvas.
   * @return {Element} The DOM Element.
   */
  decorate: function() {
    this.id = HANDWRITING_CANVAS_ELEMENT_ID;
    this.stroke_ = [];

    var canvas = this;
    var context = canvas.getContext('2d');

    canvas.className = 'handwriting';
    canvas.onmousedown = function(e) {
      var coords = canvas.getEventCoordinates(e);
      canvas.stroke_ = [];
      context.strokeStyle = HANDWRITING_CANVAS_LINE_COLOR;
      context.lineWidth = HANDWRITING_CANVAS_LINE_WIDTH;
      context.beginPath();
      context.moveTo(coords.x, coords.y);
      canvas.addStroke(coords.x, coords.y);
    };

    canvas.onmousemove = function(e) {
      var coords = canvas.getEventCoordinates(e);
      if (canvas.stroke_.length == 0) {
        return;
      }
      context.lineTo(coords.x, coords.y);
      context.stroke();
      canvas.addStroke(coords.x, coords.y);
    };
    canvas.ontouchmove = canvas.onmousemove;

    canvas.onmouseup = function(e) {
      var coords = canvas.getEventCoordinates(e);
      if (canvas.stroke_.length > 0) {
        context.lineTo(coords.x, coords.y);
        context.stroke();
        canvas.addStroke(coords.x, coords.y);
        if (chrome.experimental) {
          chrome.experimental.input.virtualKeyboard.sendHandwritingStroke(
              canvas.stroke_);
        }
        canvas.stroke_ = [];
      }
    };
    canvas.onmouseout = canvas.onmouseup;

    canvas.ontouchstart = canvas.onmousedown;
    canvas.ontouchenter = canvas.onmousedown;
    canvas.ontouchend = canvas.onmouseup;
    canvas.ontouchleave = canvas.onmouseup;
    canvas.ontouchcancel = canvas.onmouseup;

    // Clear the canvas when an IME hides the lookup table.
    if (chrome.experimental && chrome.experimental.input.ui) {
      chrome.experimental.input.ui.onUpdateLookupTable.addListener(
          function(table) {
            if (!table.visible) {
              canvas.clear();
            }
          });
    }
    return canvas;
  },

  /**
   * Updates this.stroke_ unless the posision clicked/touched is too close to
   * the previous one.
   * @param {number} offsetX x-coordinate of the mouse/touch event.
   * @param {number} offsetY y-coordinate of the mouse/touch event.
   * @return {void}
   */
  addStroke: function(offsetX, offsetY) {
    var x = offsetX / this.width;
    var y = offsetY / this.height;

    if (this.stroke_.length == 0) {
      this.stroke_.push({ x: x, y: y });
    } else {
      var delta_x =
          (this.stroke_[this.stroke_.length - 1].x - x) * this.width;
      var delta_y =
          (this.stroke_[this.stroke_.length - 1].y - y) * this.height;
      if (delta_x * delta_x + delta_y * delta_y >=
          HANDWRITING_CANVAS_MOUSEMOVE_THRESHOLD_SQ) {
        // Do not update the array if the distance is less than threshold not
        // to send excessive amount of data to an handwriting IME.
        this.stroke_.push({ x: x, y: y });
      }
    }
  },

  /**
   * Gets xy-coordinates of a mouse/touch event.
   * @param {Object} event An event object for a mouse/touch event.
   * @return {Object}
   */
  getEventCoordinates: function(event) {
    if (event.changedTouches && event.changedTouches[0]) {
      var touch = event.changedTouches[0];
      return { x: touch.clientX - event.currentTarget.offsetLeft,
            y: touch.clientY - event.currentTarget.offsetTop };
    } else if (event.offsetX) {
      return { x: event.offsetX, y: event.offsetY };
    }
    // Unexpected event. Not reached.
    return { x: 0, y: 0 };
  },

  /**
   * Resizes the canvas element.
   * @param {number} height The height of the canvas.
   * @return {void}
   */
  resize: function(height) {
    var width = height * HANDWRITING_CANVAS_ASPECT;
    this.style.height = height + 'px';
    this.style.width = width + 'px';
    this.height = height;
    this.width = width;
  },

  /**
   * Clears the canvas.
   * @return {void}
   */
  clear: function() {
    var context = this.getContext('2d');
    context.clearRect(0, 0, this.width, this.height);
    if (chrome.experimental) {
      chrome.experimental.input.virtualKeyboard.cancelHandwritingStrokes();
    }
  },

  /**
   * Set the visibility of the canvas.
   * @param {boolean} visible True if the canvas should be visible.
   * @return {void}
   */
  set visible(visible) {
    this.hidden = !visible;
  }
};

/**
 * All keys for the rows of the handwriting keyboard.
 * NOTE: every row below should have an aspect of 5.
 * @type {Array.<Array.<BaseKey>>}
 */
var KEYS_HANDWRITING_VK = [
  [
    new ClearHandwritingKey('CLEAR')
  ],
  [
    new SvgKey('handwriting-return', 'Enter')
  ],
  [
    new SpecialKey('handwriting-space', 'SPACE', 'Spacebar'),
    new SvgKey('handwriting-backspace', 'Backspace', true /* repeat */)
  ],
  [
    new SvgKey('handwriting-mic', ''),
    new HideKeyboardKey()
  ]
];

// Add the layout to KEYBOARDS, which is defined in common.js
KEYBOARDS['handwriting-vk'] = {
  'definition': KEYS_HANDWRITING_VK,
  'aspect': 2.1,
  // TODO(yusukes): Stop special-casing canvas when mazda's i18n keyboard
  // code is ready.
  'canvas': null
};
