// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple virtual keyboard implementation.
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
 * The repeat delay in milliseconds before a key starts repeating.
 * @type {number}
 */
var REPEAT_DELAY_MSEC = 250;

/**
 * The repeat interval or number of milliseconds between subsequent keypresses.
 * @type {number}
 */
var REPEAT_INTERVAL_MSEC = 92;

/**
 * A structure to track the currently repeating key on the keyboard.
 */
var repeatKey = {
    /**
     * The timer for the delay before repeating behaviour begins.
     * @type {number|undefined}
     */
    timer: undefined,

    /**
     * The interval timer for issuing keypresses of a repeating key.
     * @type {number|undefined}
     */
    interval: undefined,

    /**
     * The key which is currently repeating.
     * @type {BaseKey|undefined}
     */
    key: undefined,

    /**
     * Cancel the repeat timers of the currently active key.
     */
    cancel: function() {
      clearTimeout(this.timer);
      clearInterval(this.interval);
      this.timer = undefined;
      this.interval = undefined;
      this.key = undefined;
    }
};

/**
 * Transition the mode according to the given transition.
 * @param {string} transition The transition to take.
 * @return {void}
 */
function transitionMode(transition) {
  currentMode = MODE_TRANSITIONS[currentMode + transition];
  setMode(currentMode);
}

/**
 * Send the given key to chrome, via the experimental extension API.
 * @param {string} key The key to send.
 * @param {string=} opt_type The type of event to send (keydown or keyup). If
 *     omitted send both keydown and keyup events.
 * @return {void}
 */
function sendKey(key, type) {
  var keyEvent = {'keyIdentifier': key};
  if (!type || type == 'keydown') {
    // A keypress event is automatically generated for printable characters
    // immediately following the keydown event.
    keyEvent.type = 'keydown';
    if (chrome.experimental) {
      chrome.experimental.input.sendKeyboardEvent(keyEvent);
    }
  }
  if (!type || type == 'keyup') {
    keyEvent.type = 'keyup';
    if (chrome.experimental) {
      chrome.experimental.input.sendKeyboardEvent(keyEvent);
    }
    // Exit shift mode after completing any shifted keystroke.
    if (currentMode == SHIFT_MODE) {
      transitionMode(SHIFT_MODE);
    }
  }
}

/**
 * Set up the event handlers necessary to respond to mouse and touch events on
 * the virtual keyboard.
 * @param {BaseKey} key The BaseKey object corresponding to this key.
 * @param {Element} element The top-level DOM Element to set event handlers on.
 * @param {function()} keyDownHandler The event handler called when the key is
 *     pressed. This will be called repeatedly when holding a repeating key.
 * @param {function()=} opt_keyUpHandler The event handler called when the key
 *     is released. This is only called once per actual key press.
 */
function setupKeyEventHandlers(key, element, keyDownHandler, opt_keyUpHandler) {
  /**
   * Handle a key down event on the virtual key.
   * @param {UIEvent} evt The UI event which triggered the key down.
   */
  var downHandler = function(evt) {
    // Don't process a key down if the key is already down.
    if (key.pressed) {
      return;
    }
    key.pressed = true;
    if (keyDownHandler) {
      keyDownHandler();
    }
    evt.preventDefault();
    repeatKey.cancel();

    // Start a repeating timer if there is a repeat interval and a function to
    // process key down events.
    if (key.repeat && keyDownHandler) {
      repeatKey.key = key;
      // The timeout for the repeating timer occurs at
      // REPEAT_DELAY_MSEC - REPEAT_INTERVAL_MSEC so that the interval
      // function can handle all repeat keypresses and will get the first one
      // at the correct time.
      repeatKey.timer = setTimeout(function() {
            repeatKey.timer = undefined;
            repeatKey.interval = setInterval(function() {
                  keyDownHandler();
                }, REPEAT_INTERVAL_MSEC);
          }, Math.max(0, REPEAT_DELAY_MSEC - REPEAT_INTERVAL_MSEC));
    }
  };

  /**
   * Handle a key up event on the virtual key.
   * @param {UIEvent} evt The UI event which triggered the key up.
   */
  var upHandler = function(evt) {
    // If they key was not actually pressed do not send a key up event.
    if (!key.pressed) {
      return;
    }
    key.pressed = false;

    // Cancel running repeat timer for the released key only.
    if (repeatKey.key == key) {
      repeatKey.cancel();
    }

    if (opt_keyUpHandler) {
      opt_keyUpHandler();
    }
    evt.preventDefault();
  };

  // Setup mouse event handlers.
  element.addEventListener('mousedown', downHandler);
  element.addEventListener('mouseup', upHandler);
  element.addEventListener('mouseout', upHandler);

  // Setup touch handlers.
  element.addEventListener('touchstart', downHandler);
  element.addEventListener('touchend', upHandler);
}

/**
 * Create closure for the sendKey function.
 * @param {string} key The key paramater to sendKey.
 * @param {string=} type The type parameter to sendKey.
 * @return {function()} A function which calls sendKey(key, type).
 */
function sendKeyFunction(key, type) {
  return function() {
    sendKey(key, type);
  };
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
   * The column.
   * @type {number}
   */
  column_: 0.,

  /**
   * The cell type of this key.  Determines the background colour.
   * @type {string}
   */
  cellType_: '',

  /**
   * If true, holding this key will issue repeat keypresses.
   * @type {boolean}
   */
  repeat_: false,

  /**
   * Track the pressed state of the key. This is true if currently pressed.
   * @type {boolean}
   */
  pressed_: false,

  /**
   * Get the repeat behaviour of the key.
   * @return {boolean} True if the key will repeat.
   */
  get repeat() {
    return this.repeat_;
  },

  /**
   * Set the repeat behaviour of the key
   * @param {boolean} repeat True if the key should repeat.
   */
  set repeat(repeat) {
    this.repeat_ = repeat;
  },

  /**
   * Get the pressed state of the key.
   * @return {boolean} True if the key is currently pressed.
   */
  get pressed() {
    return this.pressed_;
  },

  /**
   * Set the pressed state of the key.
   * @param {boolean} pressed True if the key is currently pressed.
   */
  set pressed(pressed) {
    this.pressed_ = pressed;
  },

  /**
   * @return {number} The aspect ratio of this key.
   */
  get aspect() {
    return this.aspect_;
  },

  /**
   * Set the horizontal position for this key.
   * @param {number} column Horizonal position (in multiples of row height).
   */
  set column(column) {
    this.column_ = column;
  },

  /**
   * Set the vertical position, aka row, of this key.
   * @param {number} row The row.
   */
  set row(row) {
    for (var i in this.modeElements_) {
      this.modeElements_[i].classList.add(this.cellType_ + 'r' + row);
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
    var width = Math.floor(height * (this.column_ + this.aspect_)) -
                Math.floor(height *  this.column_);

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
    setupKeyEventHandlers(this, this.modeElements_[mode],
        sendKeyFunction(this.modes_[mode].keyIdentifier, 'keydown'),
        sendKeyFunction(this.modes_[mode].keyIdentifier, 'keyup'));

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

    setupKeyEventHandlers(this, this.modeElements_[mode],
        sendKeyFunction(this.keyId_, 'keydown'),
        sendKeyFunction(this.keyId_, 'keyup'));
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

    this.sizeElement(mode, height);
    setupKeyEventHandlers(this, this.modeElements_[mode],
        sendKeyFunction(this.keyId_, 'keydown'),
        sendKeyFunction(this.keyId_, 'keyup'));

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
    setupKeyEventHandlers(this, this.modeElements_[mode],
        function() {
          transitionMode(SHIFT_MODE);
        });

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
    setupKeyEventHandlers(this, this.modeElements_[mode],
        function() {
          transitionMode(NUMBER_MODE);
        });

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
    setupKeyEventHandlers(this, this.modeElements_[mode],
        function() {
          sendKey('.');
          sendKey('c');
          sendKey('o');
          sendKey('m');
        });

    return this.modeElements_[mode];
  }
};

/**
 * The key that hides the keyboard.
 * @param {number} aspect The aspect ratio of the key.
 * @constructor
 * @extends {BaseKey}
 */
function HideKeyboardKey(aspect) {
  this.modeElements_ = {}
  this.aspect_ = aspect;
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
    setupKeyEventHandlers(this, this.modeElements_[mode],
        function() {
          if (chrome.experimental) {
            chrome.experimental.input.hideKeyboard();
          }
        });

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
        this.modeElements_[MODES[i]].appendChild(key.makeDOM(MODES[i], height));
      }
    }

    for (var i = 0; i < MODES.length; ++i) {
      var clearingDiv = document.createElement('div');
      clearingDiv.style.clear = 'both';
      this.modeElements_[MODES[i]].appendChild(clearingDiv);
    }

    var column = 0.;
    for (var i = 0; i < this.keys_.length; ++i) {
      this.keys_[i].row = this.position_;
      this.keys_[i].column = column;
      column += this.keys_[i].aspect;
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
