// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple, English virtual keyboard implementation.
 */

var KEY_MODE = 'key';
var SHIFT_MODE = 'shift';
var NUMBER_MODE = 'number';
var SYMBOL_MODE = 'symbol';
var MODES = [ KEY_MODE, SHIFT_MODE, NUMBER_MODE, SYMBOL_MODE ];
var currentMode = KEY_MODE;
var MODE_TRANSITIONS = {};

MODE_TRANSITIONS[KEY_MODE + SHIFT_MODE] = SHIFT_MODE;
MODE_TRANSITIONS[KEY_MODE + NUMBER_MODE] = NUMBER_MODE;
MODE_TRANSITIONS[SHIFT_MODE + SHIFT_MODE] = KEY_MODE;
MODE_TRANSITIONS[SHIFT_MODE + NUMBER_MODE] = NUMBER_MODE;
MODE_TRANSITIONS[NUMBER_MODE + SHIFT_MODE] = SYMBOL_MODE;
MODE_TRANSITIONS[NUMBER_MODE + NUMBER_MODE] = KEY_MODE;
MODE_TRANSITIONS[SYMBOL_MODE + SHIFT_MODE] = NUMBER_MODE;
MODE_TRANSITIONS[SYMBOL_MODE + NUMBER_MODE] = KEY_MODE;

/**
 * Transition the mode according to the given transition.
 * @param {string} transition The transition to take.
 * @return {void}
 */
function transitionMode(transition) {
  currentMode = MODE_TRANSITIONS[currentMode + transition];
}

/**
 * Plain-old-data class to represent a character.
 * @param {string} display The HTML to be displayed.
 * @param {string} id The key identifier for this Character.
 * @constructor
 */
function Character(display, id) {
  this.display = display;
  this.keyIdentifier = id;
}

/**
 * Convenience function to make the keyboard data more readable.
 * @param {string} display Both the display and id for the created Character.
 */
function C(display) {
  return new Character(display, display);
}

/**
 * An abstract base-class for all keys on the keyboard.
 * @constructor
 */
function BaseKey() {}

BaseKey.prototype = {
  /**
   * The aspect ratio of this key.
   * @type {number}
   */
  aspect_: 1,

  /**
   * The cell type of this key.  Determines the background colour.
   * @type {string}
   */
  cellType_: '',

  /**
   * @return {number} The aspect ratio of this key.
   */
  get aspect() {
    return this.aspect_;
  },

  /**
   * Set the position, a.k.a. row, of this key.
   * @param {string} position The position.
   * @return {void}
   */
  set position(position) {
    for (var i in this.modeElements_) {
      this.modeElements_[i].classList.add(this.cellType_ + 'r' + position);
    }
  },

  /**
   * Returns the amount of padding for the top of the key.
   * @param {string} mode The mode for the key.
   * @param {number} height The height of the key.
   * @return {number} Padding in pixels.
   */
  getPadding: function(mode, height) {
    return Math.floor(height / 3.5);
  },

  /**
   * Size the DOM elements of this key.
   * @param {string} mode The mode to be sized.
   * @param {number} height The height of the key.
   * @return {void}
   */
  sizeElement: function(mode, height) {
    var padding = this.getPadding(mode, height);
    var border = 1;
    var margin = 5;
    var width = Math.floor(height * this.aspect_);

    var extraHeight = margin + padding + 2 * border;
    var extraWidth = margin + 2 * border;

    this.modeElements_[mode].style.width = (width - extraWidth) + 'px';
    this.modeElements_[mode].style.height = (height - extraHeight) + 'px';
    this.modeElements_[mode].style.marginLeft = margin + 'px';
    this.modeElements_[mode].style.fontSize = (height / 3.5) + 'px';
    this.modeElements_[mode].style.paddingTop = padding + 'px';
  },

  /**
   * Resize all modes of this key based on the given height.
   * @param {number} height The height of the key.
   * @return {void}
   */
  resize: function(height) {
    for (var i in this.modeElements_) {
      this.sizeElement(i, height);
    }
  },

  /**
   * Create the DOM elements for the given keyboard mode.  Must be overridden.
   * @param {string} mode The keyboard mode to create elements for.
   * @param {number} height The height of the key.
   * @return {Element} The top-level DOM Element for the key.
   */
  makeDOM: function(mode, height) {
    throw new Error('makeDOM not implemented in BaseKey');
  },
};

/**
 * A simple key which displays Characters.
 * @param {Character} key The Character for KEY_MODE.
 * @param {Character} shift The Character for SHIFT_MODE.
 * @param {Character} num The Character for NUMBER_MODE.
 * @param {Character} symbol The Character for SYMBOL_MODE.
 * @constructor
 * @extends {BaseKey}
 */
function Key(key, shift, num, symbol) {
  this.modeElements_ = {};
  this.aspect_ = 1;  // ratio width:height
  this.cellType_ = '';

  this.modes_ = {};
  this.modes_[KEY_MODE] = key;
  this.modes_[SHIFT_MODE] = shift;
  this.modes_[NUMBER_MODE] = num;
  this.modes_[SYMBOL_MODE] = symbol;
}

Key.prototype = {
  __proto__: BaseKey.prototype,

  /** @inheritDoc */
  makeDOM: function(mode, height) {
    this.modeElements_[mode] = document.createElement('div');
    this.modeElements_[mode].textContent = this.modes_[mode].display;
    this.modeElements_[mode].className = 'key';

    this.sizeElement(mode, height);

    this.modeElements_[mode].onclick =
        sendKeyFunction(this.modes_[mode].keyIdentifier);

    return this.modeElements_[mode];
  }
};

/**
 * A key which displays an SVG image.
 * @param {number} aspect The aspect ratio of the key.
 * @param {string} className The class that provides the image.
 * @param {string} keyId The key identifier for the key.
 * @constructor
 * @extends {BaseKey}
 */
function SvgKey(aspect, className, keyId) {
  this.modeElements_ = {};
  this.aspect_ = aspect;
  this.cellType_ = 'nc';
  this.className_ = className;
  this.keyId_ = keyId;
}

SvgKey.prototype = {
  __proto__: BaseKey.prototype,

  /** @inheritDoc */
  getPadding: function(mode, height) { return 0; },

  /** @inheritDoc */
  makeDOM: function(mode, height) {
    this.modeElements_[mode] = document.createElement('div');
    this.modeElements_[mode].className = 'key';

    var img = document.createElement('div');
    img.className = 'image-key ' + this.className_;
    this.modeElements_[mode].appendChild(img);

    this.modeElements_[mode].onclick = sendKeyFunction(this.keyId_);

    this.sizeElement(mode, height);

    return this.modeElements_[mode];
  }
};

/**
 * A Key that remains the same through all modes.
 * @param {number} aspect The aspect ratio of the key.
 * @param {string} content The display text for the key.
 * @param {string} keyId The key identifier for the key.
 * @constructor
 * @extends {BaseKey}
 */
function SpecialKey(aspect, content, keyId) {
  this.modeElements_ = {};
  this.aspect_ = aspect;
  this.cellType_ = 'nc';
  this.content_ = content;
  this.keyId_ = keyId;
}

SpecialKey.prototype = {
  __proto__: BaseKey.prototype,

  /** @inheritDoc */
  makeDOM: function(mode, height) {
    this.modeElements_[mode] = document.createElement('div');
    this.modeElements_[mode].textContent = this.content_;
    this.modeElements_[mode].className = 'key';

    this.modeElements_[mode].onclick = sendKeyFunction(this.keyId_);

    this.sizeElement(mode, height);

    return this.modeElements_[mode];
  }
};

/**
 * A shift key.
 * @param {number} aspect The aspect ratio of the key.
 * @constructor
 * @extends {BaseKey}
 */
function ShiftKey(aspect) {
  this.modeElements_ = {};
  this.aspect_ = aspect;
  this.cellType_ = 'nc';
}

ShiftKey.prototype = {
  __proto__: BaseKey.prototype,

  /** @inheritDoc */
  getPadding: function(mode, height) {
    if (mode == NUMBER_MODE || mode == SYMBOL_MODE) {
      return BaseKey.prototype.getPadding.call(this, mode, height);
    }
    return 0;
  },

  /** @inheritDoc */
  makeDOM: function(mode, height) {
    this.modeElements_[mode] = document.createElement('div');

    if (mode == KEY_MODE || mode == SHIFT_MODE) {
      var shift = document.createElement('div');
      shift.className = 'image-key shift';
      this.modeElements_[mode].appendChild(shift);
    } else if (mode == NUMBER_MODE) {
      this.modeElements_[mode].textContent = 'more';
    } else if (mode == SYMBOL_MODE) {
      this.modeElements_[mode].textContent = '#123';
    }

    if (mode == SHIFT_MODE || mode == SYMBOL_MODE) {
      this.modeElements_[mode].className = 'moddown key';
    } else {
      this.modeElements_[mode].className = 'key';
    }

    this.sizeElement(mode, height);

    this.modeElements_[mode].onclick = function() {
      transitionMode(SHIFT_MODE);
      setMode(currentMode);
    };
    return this.modeElements_[mode];
  },
};

/**
 * The symbol key: switches the keyboard into symbol mode.
 * @constructor
 * @extends {BaseKey}
 */
function SymbolKey() {
  this.modeElements_ = {}
  this.aspect_ = 1.3;
  this.cellType_ = 'nc';
}

SymbolKey.prototype = {
  __proto__: BaseKey.prototype,

  /** @inheritDoc */
  makeDOM: function(mode, height) {
    this.modeElements_[mode] = document.createElement('div');

    if (mode == KEY_MODE || mode == SHIFT_MODE) {
      this.modeElements_[mode].textContent = '#123';
    } else if (mode == NUMBER_MODE || mode == SYMBOL_MODE) {
      this.modeElements_[mode].textContent = 'abc';
    }

    if (mode == NUMBER_MODE || mode == SYMBOL_MODE) {
      this.modeElements_[mode].className = 'moddown key';
    } else {
      this.modeElements_[mode].className = 'key';
    }

    this.sizeElement(mode, height);

    this.modeElements_[mode].onclick = function() {
      transitionMode(NUMBER_MODE);
      setMode(currentMode);
    };

    return this.modeElements_[mode];
  }
};

/**
 * The ".com" key.
 * @constructor
 * @extends {BaseKey}
 */
function DotComKey() {
  this.modeElements_ = {}
  this.aspect_ = 1.3;
  this.cellType_ = 'nc';
}

DotComKey.prototype = {
  __proto__: BaseKey.prototype,

  /** @inheritDoc */
  makeDOM: function(mode, height) {
    this.modeElements_[mode] = document.createElement('div');
    this.modeElements_[mode].textContent = '.com';
    this.modeElements_[mode].className = 'key';

    this.sizeElement(mode, height);

    this.modeElements_[mode].onclick = function() {
      sendKey('.');
      sendKey('c');
      sendKey('o');
      sendKey('m');
    };

    return this.modeElements_[mode];
  }
};

/**
 * The key that hides the keyboard.
 * @constructor
 * @extends {BaseKey}
 */
function HideKeyboardKey() {
  this.modeElements_ = {}
  this.aspect_ = 1.3;
  this.cellType_ = 'nc';
}

HideKeyboardKey.prototype = {
  __proto__: BaseKey.prototype,

  /** @inheritDoc */
  getPadding: function(mode, height) { return 0; },

  /** @inheritDoc */
  makeDOM: function(mode, height) {
    this.modeElements_[mode] = document.createElement('div');
    this.modeElements_[mode].className = 'key';

    var hide = document.createElement('div');
    hide.className = 'image-key hide';
    this.modeElements_[mode].appendChild(hide);

    this.sizeElement(mode, height);

    this.modeElements_[mode].onclick = function() {
      // TODO(bryeung): need a way to cancel the keyboard
    };

    return this.modeElements_[mode];
  }
};

/**
 * A container for keys.
 * @param {number} position The position of the row (0-3).
 * @param {Array.<BaseKey>} keys The keys in the row.
 * @constructor
 */
function Row(position, keys) {
  this.position_ = position;
  this.keys_ = keys;
  this.element_ = null;
  this.modeElements_ = {};
}

Row.prototype = {
  /**
   * Get the total aspect ratio of the row.
   * @return {number} The aspect ratio relative to a height of 1 unit.
   */
  get aspect() {
    var total = 0;
    for (var i = 0; i < this.keys_.length; ++i) {
      total += this.keys_[i].aspect;
    }
    return total;
  },

  /**
   * Create the DOM elements for the row.
   * @return {Element} The top-level DOM Element for the row.
   */
  makeDOM: function(height) {
    this.element_ = document.createElement('div');
    this.element_.className = 'row';
    for (var i = 0; i < MODES.length; ++i) {
      var mode = MODES[i];
      this.modeElements_[mode] = document.createElement('div');
      this.modeElements_[mode].style.display = 'none';
      this.element_.appendChild(this.modeElements_[mode]);
    }

    for (var j = 0; j < this.keys_.length; ++j) {
      var key = this.keys_[j];
      for (var i = 0; i < MODES.length; ++i) {
        this.modeElements_[MODES[i]].appendChild(key.makeDOM(MODES[i]), height);
      }
    }

    for (var i = 0; i < MODES.length; ++i) {
      var clearingDiv = document.createElement('div');
      clearingDiv.style.clear = 'both';
      this.modeElements_[MODES[i]].appendChild(clearingDiv);
    }

    for (var i = 0; i < this.keys_.length; ++i) {
      this.keys_[i].position = this.position_;
    }

    return this.element_;
  },

  /**
   * Shows the given mode.
   * @param {string} mode The mode to show.
   * @return {void}
   */
  showMode: function(mode) {
    for (var i = 0; i < MODES.length; ++i) {
      this.modeElements_[MODES[i]].style.display = 'none';
    }
    this.modeElements_[mode].style.display = 'block';
  },

  /**
   * Resizes all keys in the row according to the global size.
   * @param {number} height The height of the key.
   * @return {void}
   */
  resize: function(height) {
    for (var i = 0; i < this.keys_.length; ++i) {
      this.keys_[i].resize(height);
    }
  },
};

/**
 * All keys for the rows of the keyboard.
 * NOTE: every row below should have an aspect of 12.6.
 * @type {Array.<Array.<BaseKey>>}
 */
var KEYS = [
  [
    new SvgKey(1, 'tab', 'Tab'),
    new Key(C('q'), C('Q'), C('1'), C('`')),
    new Key(C('w'), C('W'), C('2'), C('~')),
    new Key(C('e'), C('E'), C('3'), new Character('<', 'LessThan')),
    new Key(C('r'), C('R'), C('4'), new Character('>', 'GreaterThan')),
    new Key(C('t'), C('T'), C('5'), C('[')),
    new Key(C('y'), C('Y'), C('6'), C(']')),
    new Key(C('u'), C('U'), C('7'), C('{')),
    new Key(C('i'), C('I'), C('8'), C('}')),
    new Key(C('o'), C('O'), C('9'), C('\'')),
    new Key(C('p'), C('P'), C('0'), C('|')),
    new SvgKey(1.6, 'backspace', 'Backspace')
  ],
  [
    new SymbolKey(),
    new Key(C('a'), C('A'), C('!'), C('+')),
    new Key(C('s'), C('S'), C('@'), C('=')),
    new Key(C('d'), C('D'), C('#'), C(' ')),
    new Key(C('f'), C('F'), C('$'), C(' ')),
    new Key(C('g'), C('G'), C('%'), C(' ')),
    new Key(C('h'), C('H'), C('^'), C(' ')),
    new Key(C('j'), C('J'), new Character('&', 'Ampersand'), C(' ')),
    new Key(C('k'), C('K'), C('*'), C('#')),
    new Key(C('l'), C('L'), C('('), C(' ')),
    new Key(C('\''), C('\''), C(')'), C(' ')),
    new SvgKey(1.3, 'return', 'Enter')
  ],
  [
    new ShiftKey(1.6),
    new Key(C('z'), C('Z'), C('/'), C(' ')),
    new Key(C('x'), C('X'), C('-'), C(' ')),
    new Key(C('c'), C('C'), C('\''), C(' ')),
    new Key(C('v'), C('V'), C('"'), C(' ')),
    new Key(C('b'), C('B'), C(':'), C('.')),
    new Key(C('n'), C('N'), C(';'), C(' ')),
    new Key(C('m'), C('M'), C('_'), C(' ')),
    new Key(C('!'), C('!'), C('{'), C(' ')),
    new Key(C('?'), C('?'), C('}'), C(' ')),
    new Key(C('/'), C('/'), C('\\'), C(' ')),
    new ShiftKey(1)
  ],
  [
    new SvgKey(1.3, 'mic', ''),
    new DotComKey(),
    new SpecialKey(1.3, '@', '@'),
    // TODO(bryeung): the spacebar needs to be a little bit more stretchy,
    // since this row has only 7 keys (as opposed to 12), the truncation
    // can cause it to not be wide enough.
    new SpecialKey(4.8, ' ', 'Spacebar'),
    new SpecialKey(1.3, ',', ','),
    new SpecialKey(1.3, '.', '.'),
    new HideKeyboardKey()
  ]
];

/**
 * All of the rows in the keyboard.
 * @type {Array.<Row>}
 */
var allRows = [];  // Populated during start()

/**
 * Calculate the height of the row based on the size of the page.
 * @return {number} The height of each row, in pixels.
 */
function getRowHeight() {
  var x = window.innerWidth;
  var y = window.innerHeight;
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
  for (var i = 0; i < allRows.length; ++i) {
    allRows[i].showMode(mode);
  }
}

/**
 * The keyboard's aspect ratio.
 * @type {number}
 */
var kKeyboardAspect = 3.3;

/**
 * Send the given key to chrome, via the experimental extension API.
 * @param {string} key The key to send.
 * @return {void}
 */
function sendKey(key) {
  if (!chrome.experimental) {
    console.log(key);
    return;
  }

  var keyEvent = {'type': 'keydown', 'keyIdentifier': key};
  if (currentMode == SHIFT_MODE)
    keyEvent['shiftKey'] = true;

  chrome.experimental.input.sendKeyboardEvent(keyEvent);
  keyEvent['type'] = 'keyup';
  chrome.experimental.input.sendKeyboardEvent(keyEvent);

  // TODO(bryeung): deactivate shift after a successful keypress
}

/**
 * Create a closure for the sendKey function.
 * @param {string} key The parameter to sendKey.
 * @return {void}
 */
function sendKeyFunction(key) {
  return function() { sendKey(key); }
}

/**
 * Resize the keyboard according to the new window size.
 * @return {void}
 */
window.onresize = function() {
  var height = getRowHeight();
  var newX = document.documentElement.clientWidth;

  // All rows should have the same aspect, so just use the first one
  var totalWidth = Math.floor(height * allRows[0].aspect);
  var leftPadding = Math.floor((newX - totalWidth) / 2);
  document.getElementById('b').style.paddingLeft = leftPadding + 'px';

  for (var i = 0; i < allRows.length; ++i) {
    allRows[i].resize(height);
  }
}

/**
 * Init the keyboard.
 * @return {void}
 */
window.onload = function() {
  var body = document.getElementById('b');
  for (var i = 0; i < KEYS.length; ++i) {
    allRows.push(new Row(i, KEYS[i]));
  }

  for (var i = 0; i < allRows.length; ++i) {
    body.appendChild(allRows[i].makeDOM(getRowHeight()));
    allRows[i].showMode(KEY_MODE);
  }

  window.onresize();
}

// TODO(bryeung): would be nice to leave less gutter (without causing
// rendering issues with floated divs wrapping at some sizes).
