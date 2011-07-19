// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple virtual keyboard implementation.
 */

// TODO(mazda): Support more virtual keyboards like French VK, German VK, etc.
var KEYBOARDS = {
  'us': {
    'definition': KEYS_US,
    'rows': []
    // No canvas.
  },
  'handwriting-vk': {
    'definition': KEYS_HANDWRITING_VK,
    'rows': [],
    // TODO(yusukes): Stop special-casing canvas when mazda's i18n keyboard
    // code is ready.
    'canvas': new HandwritingCanvas
  }
};

/**
 * The keyboard layout name currently in use.
 * @type {string}
 */
var currentKeyboardLayout = 'us';

/**
 * The keyboard's aspect ratio.
 * @type {number}
 */
const kKeyboardAspect = 3.15;

/**
 * The ratio of the row height to the font size.
 * @type {number}
 */
const kFontSizeRatio = 3.5;

/**
 * Calculate the height of the keyboard based on the size of the page.
 * @return {number} The height of the keyboard in pixels.
 */
function getKeyboardHeight() {
  var x = window.innerWidth;
  var y = window.innerHeight - ((imeui && imeui.visible) ? IME_HEIGHT : 0);
  return (x > kKeyboardAspect * y) ? y : Math.floor(x / kKeyboardAspect);
}

/**
 * Set the keyboard mode.
 * @param {string} mode The new mode.
 * @return {void}
 */
function setMode(mode) {
  var rows = KEYBOARDS[currentKeyboardLayout]['rows'];
  for (var i = 0; i < rows.length; ++i) {
    rows[i].showMode(mode);
  }
}

/**
 * Resize the keyboard according to the new window size.
 * @return {void}
 */
window.onresize = function() {
  var keyboardDiv = document.getElementById('keyboard');
  var height = getKeyboardHeight();
  keyboardDiv.style.height = height + 'px';
  var mainDiv = document.getElementById('main');
  mainDiv.style.width = Math.floor(kKeyboardAspect * height) + 'px';
  var rowsLength = KEYBOARDS[currentKeyboardLayout]['rows'].length;
  keyboardDiv.style.fontSize = (height / kFontSizeRatio / rowsLength) + 'px';
  updateIme();
}

/**
 * Init the keyboard.
 * @return {void}
 */
window.onload = function() {
  var body = document.getElementById('b');

  var mainDiv = document.createElement('div');
  mainDiv.className = 'main';
  mainDiv.id = 'main';
  body.appendChild(mainDiv);

  initIme(mainDiv);

  // TODO(mazda,yusukes): If we support 40+ virtual keyboards, it's not a good
  // idea to create rows (and DOM) for all keyboards inside onload(). We should
  // do it on an on-demand basis instead.
  for (var layout in KEYBOARDS) {
    var keyboard = KEYBOARDS[layout];
    var def = keyboard['definition'];
    var rows = keyboard['rows'];
    for (var i = 0; i < def.length; ++i) {
      rows.push(new Row(i, def[i]));
    }
  }

  // A div element which holds rows and canvases
  var keyboardDiv = document.createElement('div');
  keyboardDiv.className = 'keyboard';
  keyboardDiv.id = 'keyboard';

  // A div element which holds canvases for all keyboards layouts.
  var canvasesDiv = document.createElement('div');
  canvasesDiv.className = 'nodisplay';

  for (var layout in KEYBOARDS) {
    // A div element which holds rows for the layout.
    var rowsDiv = document.createElement('div');
    var rows = KEYBOARDS[layout]['rows'];
    for (var i = 0; i < rows.length; ++i) {
      rowsDiv.appendChild(rows[i].makeDOM());
      rows[i].showMode(currentMode);
    }
    rowsDiv.className = 'nodisplay';
    keyboardDiv.appendChild(rowsDiv);
    KEYBOARDS[layout]['rowsDiv'] = rowsDiv;

    var canvas = KEYBOARDS[layout]['canvas'];
    if (canvas !== undefined) {
      canvas.visible = false;
      var border = 1;
      var marginTop = 5;
      var canvasHeight = getKeyboardHeight() - 2 * border - marginTop;
      canvas.resize(canvasHeight);
      canvasesDiv.appendChild(canvas);
    }
    KEYBOARDS[layout]['canvasesDiv'] = canvasesDiv;
  }
  keyboardDiv.appendChild(canvasesDiv);
  mainDiv.appendChild(keyboardDiv);

  window.onhashchange();

  // Restore the keyboard to the default state when it is hidden.
  // Ref: dvcs.w3.org/hg/webperf/raw-file/tip/specs/PageVisibility/Overview.html
  document.addEventListener("webkitvisibilitychange", function() {
    if (document.webkitHidden) {
      currentMode = SHIFT_MODE;
      setMode(currentMode);
    }
  }, false);
}
// TODO(bryeung): would be nice to leave less gutter (without causing
// rendering issues with floated divs wrapping at some sizes).

/**
 * Switch the keyboard layout based on the current URL hash.
 * @return {void}
 */
window.onhashchange = function() {
  var old_layout = currentKeyboardLayout;
  var new_layout = location.hash.replace(/^#/, "");

  if (KEYBOARDS[new_layout] === undefined) {
    // Unsupported layout.
    new_layout = "us";
  }
  currentKeyboardLayout = new_layout;

  [new_layout, old_layout].forEach(function(layout) {
      var visible = (layout == new_layout);
      var canvas = KEYBOARDS[layout]['canvas'];
      var rowsDiv = KEYBOARDS[layout]['rowsDiv'];
      var canvasesDiv = KEYBOARDS[layout]['canvasesDiv'];
      rowsDiv.className = visible ? 'rows' : 'nodisplay';
      if (canvas !== undefined) {
        canvasesDiv.className = visible ? 'canvas' : 'nodisplay';
        canvas.visible = visible;
        if (!visible) {
          canvas.clear();
        }
      }
      if (visible) {
        window.onresize();
      }
    });
}
