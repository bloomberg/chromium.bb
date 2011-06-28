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
var kKeyboardAspect = 3.3;

/**
 * Calculate the height of the row based on the size of the page.
 * @return {number} The height of each row, in pixels.
 */
function getRowHeight() {
  var x = window.innerWidth;
  var y = window.innerHeight - ((imeui && imeui.visible) ? IME_HEIGHT : 0);
  return (x > kKeyboardAspect * y) ?
      (height = Math.floor(y / 4)) :
      (height = Math.floor(x / (kKeyboardAspect * 4)));
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

var oldHeight = 0;
var oldX = 0;
var oldAspect = 0;

/**
 * Resize the keyboard according to the new window size.
 * @return {void}
 */
window.onresize = function() {
  var height = getRowHeight();
  var newX = document.documentElement.clientWidth;
  // All rows of the current keyboard should have the same aspect, so just use
  // the first row.
  var newAspect = KEYBOARDS[currentKeyboardLayout]['rows'][0].aspect;

  if (newX != oldX || height != oldHeight || newAspect != oldAspect) {
    var totalWidth = Math.floor(height * newAspect);
    var leftPadding = Math.floor((newX - totalWidth) / 2);
    document.getElementById('b').style.paddingLeft = leftPadding + 'px';
  }

  if (height != oldHeight || newAspect != oldAspect) {
    var rows = KEYBOARDS[currentKeyboardLayout]['rows'];
    for (var i = 0; i < rows.length; ++i) {
      rows[i].resize(height);
    }
    var canvas = KEYBOARDS[currentKeyboardLayout]['canvas'];
    if (canvas !== undefined) {
      var canvasHeight = (height - 2 /* overlapped margin*/) * rows.length;
      canvas.resize(canvasHeight);
    }
  }

  updateIme();

  oldHeight = height;
  oldX = newX;
  oldAspect = newAspect;
}

/**
 * Init the keyboard.
 * @return {void}
 */
window.onload = function() {
  var body = document.getElementById('b');

  initIme(body);

  var mainDiv = document.createElement('div');
  mainDiv.className = 'main';
  body.appendChild(mainDiv);

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

  height = getRowHeight();

  // A div element which holds canvases for all keyboards layouts.
  var canvasesDiv = document.createElement('div');
  canvasesDiv.className = 'canvas';

  for (var layout in KEYBOARDS) {
    // A div element which holds rows for the layout.
    var rowsDiv = document.createElement('div');
    var rows = KEYBOARDS[layout]['rows'];
    for (var i = 0; i < rows.length; ++i) {
      rowsDiv.appendChild(rows[i].makeDOM(height));
      rows[i].showMode(currentMode);
    }
    rowsDiv.hidden = true;
    mainDiv.appendChild(rowsDiv);
    KEYBOARDS[layout]['rowsDiv'] = rowsDiv;

    var canvas = KEYBOARDS[layout]['canvas'];
    if (canvas !== undefined) {
      canvas.visible = false;
      var canvasHeight = (height - 2 /* overlapped margin*/) * rows.length;
      canvas.resize(canvasHeight);
      canvasesDiv.appendChild(canvas);
    }
  }
  mainDiv.appendChild(canvasesDiv);

  oldHeight = height;
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
      rowsDiv.hidden = !visible;
      if (canvas !== undefined) {
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
