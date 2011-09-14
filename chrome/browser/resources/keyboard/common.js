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
var currentMode = SHIFT_MODE;
var enterShiftModeOnSpace = false;
var MODE_TRANSITIONS = {};

MODE_TRANSITIONS[KEY_MODE + SHIFT_MODE] = SHIFT_MODE;
MODE_TRANSITIONS[KEY_MODE + NUMBER_MODE] = NUMBER_MODE;
MODE_TRANSITIONS[SHIFT_MODE + SHIFT_MODE] = KEY_MODE;
MODE_TRANSITIONS[SHIFT_MODE + NUMBER_MODE] = NUMBER_MODE;
MODE_TRANSITIONS[NUMBER_MODE + SHIFT_MODE] = SYMBOL_MODE;
MODE_TRANSITIONS[NUMBER_MODE + NUMBER_MODE] = KEY_MODE;
MODE_TRANSITIONS[SYMBOL_MODE + SHIFT_MODE] = NUMBER_MODE;
MODE_TRANSITIONS[SYMBOL_MODE + NUMBER_MODE] = KEY_MODE;

var KEYBOARDS = {};

/**
 * The long-press delay in milliseconds before long-press handler is invoked.
 * @type {number}
 */
var LONGPRESS_DELAY_MSEC = 500;

/**
 * The repeat delay in milliseconds before a key starts repeating. Use the same
 * rate as Chromebook. (See chrome/browser/chromeos/language_preferences.cc)
 * @type {number}
 */
var REPEAT_DELAY_MSEC = 500;

/**
 * The repeat interval or number of milliseconds between subsequent keypresses.
 * Use the same rate as Chromebook.
 * @type {number}
 */
var REPEAT_INTERVAL_MSEC = 50;

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
 * @return {void}
 */
function sendKey(key) {
  var keyEvent = {'keyIdentifier': key};
  // A keypress event is automatically generated for printable characters
  // immediately following the keydown event.
  if (chrome.experimental) {
    keyEvent.type = 'keydown';
    chrome.experimental.input.sendKeyboardEvent(keyEvent);
    keyEvent.type = 'keyup';
    chrome.experimental.input.sendKeyboardEvent(keyEvent);
  }
  // Exit shift mode after pressing any key but space.
  if (currentMode == SHIFT_MODE && key != 'Spacebar') {
    transitionMode(SHIFT_MODE);
  }
  // Enter shift mode after typing a closing punctuation and then a space for a
  // new sentence.
  if (enterShiftModeOnSpace) {
    enterShiftModeOnSpace = false;
    if (currentMode != SHIFT_MODE && key == 'Spacebar') {
      transitionMode(SHIFT_MODE);
    }
  }
  if (currentMode != SHIFT_MODE && (key == '.' || key == '?' || key == '!')) {
    enterShiftModeOnSpace = true;
  }
}

/**
 * Add a child div element that represents the content of the given element.
 * A child div element that represents a text content is added if
 * opt_textContent is given. Otherwise a child element that represents an image
 * content is added. If the given element already has a child, the child element
 * is modified.
 * @param {Element} element The DOM Element to which the content is added.
 * @param {string} opt_textContent The text to be inserted.
 * @return {void}
 */
function addContent(element, opt_textContent) {
  if (element.childNodes.length > 0) {
    var content = element.childNodes[0];
    if (opt_textContent) {
      content.textContent = opt_textContent;
    }
    return;
  }

  var content = document.createElement('div');
  if (opt_textContent) {
    content.textContent = opt_textContent;
    content.className = 'text-key';
  } else {
    content.className = 'image-key';
  }
  element.appendChild(content);
}

/**
 * Set up the event handlers necessary to respond to mouse and touch events on
 * the virtual keyboard.
 * @param {BaseKey} key The BaseKey object corresponding to this key.
 * @param {Element} element The top-level DOM Element to set event handlers on.
 * @param {Object.<string, function()>} handlers The object that contains key
 *     event handlers in the following form.
 *
 *     { 'up': keyUpHandler,
 *       'down': keyDownHandler,
 *       'long': keyLongHandler }
 *
 *     keyUpHandler: Called when the key is pressed. This will be called
 *         repeatedly when holding a repeating key.
 *     keyDownHandler: Called when the keyis released. This is only called
 *         once per actual key press.
 *     keyLongHandler: Called when the key is long-pressed for
 *         |LONGPRESS_DELAY_MSEC| milliseconds.
 *
 *     The object does not necessarily contain all the handlers above, but
 *     needs to contain at least one of them.
 */
function setupKeyEventHandlers(key, element, handlers) {
  var keyDownHandler = handlers['down'];
  var keyUpHandler = handlers['up'];
  var keyLongHandler = handlers['long'];
  if (!(keyDownHandler || keyUpHandler || keyLongPressHandler)) {
    throw new Error('Invalid handlers passed to setupKeyEventHandlers');
  }

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

    if (keyLongHandler) {
      key.longPressTimer = setTimeout(function() {
          keyLongHandler(evt),
          clearTimeout(key.longPressTimer);
          delete key.longPressTimer;
          key.pressed = false;
        }, LONGPRESS_DELAY_MSEC);
    }
  };

  /**
   * Handle a key up event on the virtual key.
   * @param {UIEvent} evt The UI event which triggered the key up.
   */
  var upHandler = function(evt) {
    // Reset long-press timer.
    if (key.longPressTimer) {
      clearTimeout(key.longPressTimer);
      delete key.longPressTimer
    }

    // If they key was not actually pressed do not send a key up event.
    if (!key.pressed) {
      return;
    }
    key.pressed = false;

    // Cancel running repeat timer for the released key only.
    if (repeatKey.key == key) {
      repeatKey.cancel();
    }

    if (keyUpHandler) {
      keyUpHandler();
    }
    evt.preventDefault();
  };

  var outHandler = function(evt) {
    // Reset key press state if the point goes out of the element.
    key.pressed = false;
    // Reset long-press timer.
    if (key.longPressTimer) {
      clearTimeout(key.longPressTimer);
      delete key.longPressTimer
    }
  }

  // Setup mouse event handlers.
  element.addEventListener('mousedown', downHandler);
  element.addEventListener('mouseup', upHandler);
  element.addEventListener('mouseout', outHandler);

  // Setup touch handlers.
  element.addEventListener('touchstart', downHandler);
  element.addEventListener('touchend', upHandler);
  // TODO(mazda): Add a handler for touchleave once Webkit supports it.
  // element.addEventListener('touchleave', outHandler);
}

/**
 * Create closure for the sendKey function.
 * @param {string} key The key paramater to sendKey.
 * @return {function()} A function which calls sendKey(key).
 */
function sendKeyFunction(key) {
  return function() {
    sendKey(key);
  };
}

/**
 * Show the popup keyboard.
 * @param {string} name The name of the popup keyboard.
 */
function showPopupKeyboard(name) {
  // TODO(mazda): Implement this function.
  console.warn('Popup keyboard is not implemented yet.');
}

/**
 * Create closure for the showPopupKeyboard function.
 * @param {string} name The name paramater to showPopupKeyboard.
 * @return {function()} A function which calls showPopupKeyboard(name).
 */
function showPopupKeyboardFunction(name) {
  return function () {
    showPopupKeyboard(name);
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
   * Create the DOM elements for the given keyboard mode.  Must be overridden.
   * @param {string} mode The keyboard mode to create elements for.
   * @return {Element} The top-level DOM Element for the key.
   */
  makeDOM: function(mode) {
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
  makeDOM: function(mode) {
    if (!this.modes_[mode]) {
      return null;
    }

    this.modeElements_[mode] = document.createElement('div');
    this.modeElements_[mode].className = 'key';
    addContent(this.modeElements_[mode], this.modes_[mode].display);

    // TODO(mazda): Set the long-press handler only if the key has characters
    // to show on the popup keyboard.
    setupKeyEventHandlers(this, this.modeElements_[mode],
        { 'up': sendKeyFunction(this.modes_[mode].keyIdentifier),
          'long': showPopupKeyboardFunction('') });

    return this.modeElements_[mode];
  }
};

/**
 * A key which displays an SVG image.
 * @param {string} className The class that provides the image.
 * @param {string} keyId The key identifier for the key.
 * @param {boolean} opt_repeat True if the key should repeat.
 * @constructor
 * @extends {BaseKey}
 */
function SvgKey(className, keyId, opt_repeat) {
  this.modeElements_ = {};
  this.cellType_ = 'nc';
  this.className_ = className;
  this.keyId_ = keyId;
  this.repeat_ = opt_repeat || false;
}

SvgKey.prototype = {
  __proto__: BaseKey.prototype,

  /** @inheritDoc */
  makeDOM: function(mode) {
    this.modeElements_[mode] = document.createElement('div');
    this.modeElements_[mode].className = 'key';
    this.modeElements_[mode].classList.add(this.className_);
    addContent(this.modeElements_[mode]);

    // send the key event on key down if key repeat is enabled
    var handler = this.repeat_ ? { 'down' : sendKeyFunction(this.keyId_) } :
                                 { 'up' : sendKeyFunction(this.keyId_) };
    setupKeyEventHandlers(this, this.modeElements_[mode], handler);

    return this.modeElements_[mode];
  }
};

/**
 * A Key that remains the same through all modes.
 * @param {string} content The display text for the key.
 * @param {string} keyId The key identifier for the key.
 * @constructor
 * @extends {BaseKey}
 */
function SpecialKey(className, content, keyId) {
  this.modeElements_ = {};
  this.cellType_ = 'nc';
  this.content_ = content;
  this.keyId_ = keyId;
  this.className_ = className;
}

SpecialKey.prototype = {
  __proto__: BaseKey.prototype,

  /** @inheritDoc */
  makeDOM: function(mode) {
    this.modeElements_[mode] = document.createElement('div');
    this.modeElements_[mode].className = 'key';
    this.modeElements_[mode].classList.add(this.className_);
    addContent(this.modeElements_[mode], this.content_);

    setupKeyEventHandlers(this, this.modeElements_[mode],
        { 'up': sendKeyFunction(this.keyId_) });

    return this.modeElements_[mode];
  }
};

/**
 * A shift key.
 * @constructor
 * @extends {BaseKey}
 */
function ShiftKey(className) {
  this.modeElements_ = {};
  this.cellType_ = 'nc';
  this.className_ = className;
}

ShiftKey.prototype = {
  __proto__: BaseKey.prototype,

  /** @inheritDoc */
  makeDOM: function(mode) {
    this.modeElements_[mode] = document.createElement('div');
    this.modeElements_[mode].className = 'key shift';
    this.modeElements_[mode].classList.add(this.className_);

    if (mode == KEY_MODE || mode == SHIFT_MODE) {
      addContent(this.modeElements_[mode]);
    } else if (mode == NUMBER_MODE) {
      addContent(this.modeElements_[mode], 'more');
    } else if (mode == SYMBOL_MODE) {
      addContent(this.modeElements_[mode], '#123');
    }

    if (mode == SHIFT_MODE || mode == SYMBOL_MODE) {
      this.modeElements_[mode].classList.add('moddown');
    } else {
      this.modeElements_[mode].classList.remove('moddown');
    }

    setupKeyEventHandlers(this, this.modeElements_[mode],
        { 'down': function() {
          transitionMode(SHIFT_MODE);
        }});

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
  this.cellType_ = 'nc';
}

SymbolKey.prototype = {
  __proto__: BaseKey.prototype,

  /** @inheritDoc */
  makeDOM: function(mode, height) {
    this.modeElements_[mode] = document.createElement('div');
    this.modeElements_[mode].className = 'key symbol';

    if (mode == KEY_MODE || mode == SHIFT_MODE) {
      addContent(this.modeElements_[mode], '#123');
    } else if (mode == NUMBER_MODE || mode == SYMBOL_MODE) {
      addContent(this.modeElements_[mode], 'abc');
    }

    if (mode == NUMBER_MODE || mode == SYMBOL_MODE) {
      this.modeElements_[mode].classList.add('moddown');
    } else {
      this.modeElements_[mode].classList.remove('moddown');
    }

    setupKeyEventHandlers(this, this.modeElements_[mode],
        { 'down': function() {
          transitionMode(NUMBER_MODE);
        }});

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
  this.cellType_ = 'nc';
}

DotComKey.prototype = {
  __proto__: BaseKey.prototype,

  /** @inheritDoc */
  makeDOM: function(mode) {
    this.modeElements_[mode] = document.createElement('div');
    this.modeElements_[mode].className = 'key com';
    addContent(this.modeElements_[mode], '.com');

    setupKeyEventHandlers(this, this.modeElements_[mode],
        { 'up': function() {
          sendKey('.');
          sendKey('c');
          sendKey('o');
          sendKey('m');
        }});

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
  this.cellType_ = 'nc';
}

HideKeyboardKey.prototype = {
  __proto__: BaseKey.prototype,

  /** @inheritDoc */
  makeDOM: function(mode) {
    this.modeElements_[mode] = document.createElement('div');
    this.modeElements_[mode].className = 'key hide';
    addContent(this.modeElements_[mode]);

    setupKeyEventHandlers(this, this.modeElements_[mode],
        { 'down': function() {
          if (chrome.experimental) {
            chrome.experimental.input.hideKeyboard();
          }
        }});

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
   * Create the DOM elements for the row.
   * @return {Element} The top-level DOM Element for the row.
   */
  makeDOM: function() {
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
        var keyDom = key.makeDOM(MODES[i]);
        if (keyDom) {
          this.modeElements_[MODES[i]].appendChild(keyDom);
        }
      }
    }

    for (var i = 0; i < MODES.length; ++i) {
      var clearingDiv = document.createElement('div');
      clearingDiv.style.clear = 'both';
      this.modeElements_[MODES[i]].appendChild(clearingDiv);
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
    this.modeElements_[mode].style.display = '-webkit-box';
  },
};
