// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// How long to wait to open submenu when mouse hovers.
var SUBMENU_OPEN_DELAY_MS = 200;
// How long to wait to close submenu when mouse left.
var SUBMENU_CLOSE_DELAY_MS = 500;
// Scroll repeat interval.
var SCROLL_INTERVAL_MS = 20;
// Scrolling amount in pixel.
var SCROLL_TICK_PX = 4;
// Regular expression to match/find mnemonic key.
var MNEMONIC_REGEXP = /([^&]*)&(.)(.*)/;

var localStrings = new LocalStrings();

/**
 * Sends 'activate' WebUI message.
 * @param {number} index The index of menu item to activate in menu model.
 * @param {string} mode The activation mode, one of 'close_and_activate', or
 *    'activate_no_close'.
 * TODO(oshima): change these string to enum numbers once it becomes possible
 * to pass number to C++.
 */
function sendActivate(index, mode) {
  chrome.send('activate', [String(index), mode]);
}

/**
 * MenuItem class.
 */
var MenuItem = cr.ui.define('div');

MenuItem.prototype = {
  __proto__ : HTMLDivElement.prototype,

  /**
   * Decorates the menu item element.
   */
  decorate: function() {
    this.className = 'menu-item';
  },

  /**
   * Initialize the MenuItem.
   * @param {Menu} menu A {@code Menu} object to which this menu item
   *    will be added to.
   * @param {Object} attrs JSON object that represents this menu items
   *    properties.  This is created from menu model in C code.  See
   *    chromeos/views/native_menu_webui.cc.
   * @param {Object} model The model object.
   */
  init: function(menu, attrs, model) {
    // The left icon's width. 0 if no icon.
    var leftIconWidth = model.maxIconWidth;
    this.menu_ = menu;
    this.attrs = attrs;
    var attrs = this.attrs;
    if (attrs.type == 'separator') {
      this.className = 'separator';
    } else if (attrs.type == 'command' ||
               attrs.type == 'submenu' ||
               attrs.type == 'check' ||
               attrs.type == 'radio') {
      this.initMenuItem_();
      this.initPadding_(leftIconWidth);
    } else {
      // This should not happend.
      this.classList.add('disabled');
      this.textContent = 'unknown';
    }

    menu.appendChild(this);
    if (!attrs.visible) {
      this.classList.add('hidden');
    }
  },

  /**
   * Changes the selection state of the menu item.
   * @param {boolean} selected True to set the selection, or false
   *     otherwise.
   */
  set selected(selected) {
    if (selected) {
      this.classList.add('selected');
      this.menu_.selectedItem = this;
    } else {
      this.classList.remove('selected');
    }
  },

  /**
   * Activate the menu item.
   */
  activate: function() {
    if (this.attrs.type == 'submenu') {
      this.menu_.openSubmenu(this);
    } else if (this.attrs.type != 'separator' &&
               this.className.indexOf('selected') >= 0) {
      sendActivate(this.menu_.getMenuItemIndexOf(this),
                   'close_and_activate');
    }
  },

  /**
   * Sends open_submenu WebUI message.
   */
  sendOpenSubmenuCommand: function() {
    chrome.send('open_submenu',
                [String(this.menu_.getMenuItemIndexOf(this)),
                 String(this.getBoundingClientRect().top)]);
  },

  /**
   * Internal method to initiailze the MenuItem.
   * @private
   */
  initMenuItem_: function() {
    var attrs = this.attrs;
    this.className = 'menu-item ' + attrs.type;
    this.menu_.addHandlers(this, this);
    var label = document.createElement('div');

    label.className = 'menu-label';
    this.menu_.addLabelTo(this, attrs.label, label,
                          true /* enable mnemonic */);

    if (attrs.font) {
      label.style.font = attrs.font;
    }
    this.appendChild(label);


    if (attrs.accel) {
      var accel = document.createElement('div');
      accel.className = 'accelerator';
      accel.textContent = attrs.accel;
      accel.style.font = attrs.font;
      this.appendChild(accel);
    }

    if (attrs.type == 'submenu') {
      // This overrides left-icon's position, but it's OK as submenu
      // shoudln't have left-icon.
      this.classList.add('right-icon');
      this.style.backgroundImage = 'url(' + this.menu_.config_.arrowUrl + ')';
    }
  },

  initPadding_: function(leftIconWidth) {
    if (leftIconWidth <= 0) {
      this.classList.add('no-icon');
      return;
    }
    this.classList.add('left-icon');

    var url;
    var attrs = this.attrs;
    if (attrs.type == 'radio') {
      url = attrs.checked ?
          this.menu_.config_.radioOnUrl :
          this.menu_.config_.radioOffUrl;
    } else if (attrs.icon) {
      url = attrs.icon;
    } else if (attrs.type == 'check' && attrs.checked) {
      url = this.menu_.config_.checkUrl;
    }
    if (url) {
      this.style.backgroundImage = 'url(' + url + ')';
    }
    // TODO(oshima): figure out how to update left padding in rule.
    // 4 is the padding on left side of icon.
    var padding =
        4 + leftIconWidth + this.menu_.config_.icon_to_label_padding;
    this.style.WebkitPaddingStart = padding + 'px';
  },
};

/**
 * Menu class.
 */
var Menu = cr.ui.define('div');

Menu.prototype = {
  __proto__: HTMLDivElement.prototype,

  /**
   * Configuration object.
   * @type {Object}
   */
  config_ : null,

  /**
   * Currently selected menu item.
   * @type {MenuItem}
   */
  current_ : null,

  /**
   * Timers for opening/closing submenu.
   * @type {number}
   */
  openSubmenuTimer_ : 0,
  closeSubmenuTimer_ : 0,

  /**
   * Auto scroll timer.
   * @type {number}
   */
  scrollTimer_ : 0,

  /**
   * Pointer to a submenu currently shown, if any.
   * @type {MenuItem}
   */
  submenuShown_ : null,

  /**
   * True if this menu is root.
   * @type {boolean}
   */
  isRoot_ : false,

  /**
   * Scrollable Viewport.
   * @type {HTMLElement}
   */
  viewpotr_ : null,

  /**
   * Total hight of scroll buttons. Used to adjust the height of
   * viewport in order to show scroll bottons without scrollbar.
   * @type {number}
   */
  buttonHeight_ : 0,

  /**
   * True to enable scroll button.
   * @type {boolean}
   */
  scrollEnabled : false,

  /**
   * Decorates the menu element.
   */
  decorate: function() {
    this.id = 'viewport';
  },

  /**
   * Initialize the menu.
   * @param {Object} config Configuration parameters in JSON format.
   *  See chromeos/views/native_menu_webui.cc for details.
   */
  init: function(config) {
    // List of menu items
    this.items_ = [];
    // Map from mnemonic character to item to activate
    this.mnemonics_ = {};

    this.config_ = config;
    this.addEventListener('mouseout', this.onMouseout_.bind(this));

    document.addEventListener('keydown', this.onKeydown_.bind(this));
    document.addEventListener('keypress', this.onKeypress_.bind(this));
    document.addEventListener('mousewheel', this.onMouseWheel_.bind(this));
    window.addEventListener('resize', this.onResize_.bind(this));

    // Setup scroll events.
    var up = document.getElementById('scroll-up');
    var down = document.getElementById('scroll-down');
    up.addEventListener('mouseout', this.stopScroll_.bind(this));
    down.addEventListener('mouseout', this.stopScroll_.bind(this));
    var menu = this;
    up.addEventListener('mouseover',
                        function() {
                          menu.autoScroll_(-SCROLL_TICK_PX);
                        });
    down.addEventListener('mouseover',
                          function() {
                            menu.autoScroll_(SCROLL_TICK_PX);
                          });

    this.buttonHeight_ =
        up.getBoundingClientRect().height +
        down.getBoundingClientRect().height;
  },

  /**
   * Adds a label to {@code targetDiv}. A label may contain
   * mnemonic key, preceded by '&'.
   * @param {MenuItem} item The menu item to be activated by mnemonic
   *    key.
   * @param {string} label The label string to be added to
   *    {@code targetDiv}.
   * @param {HTMLElement} div The div element the label is added to.
   * @param {boolean} enableMnemonic True to enable mnemonic, or false
   *    to not to interprete mnemonic key. The function removes '&'
   *    from the label in both cases.
   */
  addLabelTo: function(item, label, targetDiv, enableMnemonic) {
    var mnemonic = MNEMONIC_REGEXP.exec(label);
    if (mnemonic && enableMnemonic) {
      var c = mnemonic[2].toLowerCase();
      this.mnemonics_[c] = item;
    }
    if (!mnemonic) {
      targetDiv.textContent = label;
    } else if (enableMnemonic) {
      targetDiv.appendChild(document.createTextNode(mnemonic[1]));
      targetDiv.appendChild(document.createElement('span'));
      targetDiv.appendChild(document.createTextNode(mnemonic[3]));
      targetDiv.childNodes[1].className = 'mnemonic';
      targetDiv.childNodes[1].textContent = mnemonic[2];
    } else {
      targetDiv.textContent = mnemonic.splice(1, 3).join('');
    }
  },

  /**
   * Returns the index of the {@code item}.
   */
  getMenuItemIndexOf: function(item) {
    return this.items_.indexOf(item);
  },

  /**
   * A template method to create an item object. It can be a subclass
   * of MenuItem, or any HTMLElement that implements {@code init},
   * {@code activate} methods as well as {@code selected} attribute.
   * @param {Object} attrs The menu item's properties passed from C++.
   */
  createMenuItem: function(attrs) {
    return new MenuItem();
  },

  /**
   * Update and display the new model.
   */
  updateModel: function(model) {
    this.isRoot = model.isRoot;
    this.current_ = null;
    this.items_ = [];
    this.mnemonics_ = {};
    this.innerHTML = '';  // remove menu items

    for (var i = 0; i < model.items.length; i++) {
      var attrs = model.items[i];
      var item = this.createMenuItem(attrs);
      item.init(this, attrs, model);
      this.items_.push(item);
    }
    this.onResize_();
  },

  /**
   * Highlights the currently selected item, or
   * select the 1st selectable item if none is selected.
   */
  showSelection: function() {
    if (this.current_) {
      this.current_.selected = true;
    } else  {
      this.findNextEnabled_(1).selected = true;
    }
  },

  /**
   * Add event handlers for the item.
   */
  addHandlers: function(item, target) {
    var menu = this;
    target.addEventListener('mouseover', function(event) {
      menu.onMouseover_(event, item);
    });
    if (item.attrs.enabled) {
      target.addEventListener('mouseup', function(event) {
        menu.onClick_(event, item);
      });
    } else {
      target.classList.add('disabled');
    }
  },

  /**
   * Set the selected item. This controls timers to open/close submenus.
   * 1) If the selected menu is submenu, and that submenu is not yet opeend,
   *    start timer to open. This will not cancel close timer, so
   *    if there is a submenu opened, it will be closed before new submenu is
   *    open.
   * 2) If the selected menu is submenu, and that submenu is already opened,
   *    cancel both open/close timer.
   * 3) If the selected menu is not submenu, cancel all timers and start
   *    timer to close submenu.
   * This prevents from opening/closing menus while you're actively
   * navigating menus. To open submenu, you need to wait a bit, or click
   * submenu.
   *
   * @param {MenuItem} item The selected item.
   */
  set selectedItem(item) {
    if (this.current_ != item) {
      if (this.current_ != null)
        this.current_.selected = false;
      this.current_ = item;
      this.makeSelectedItemVisible_();
    }

    var menu = this;
    if (item.attrs.type == 'submenu') {
      if (this.submenuShown_ != item) {
        this.openSubmenuTimer_ =
            setTimeout(
                function() {
                  menu.openSubmenu(item);
                },
                SUBMENU_OPEN_DELAY_MS);
      } else {
        this.cancelSubmenuTimer_();
      }
    } else if (this.submenuShown_) {
      this.cancelSubmenuTimer_();
      this.closeSubmenuTimer_ =
          setTimeout(
              function() {
                menu.closeSubmenu_(item);
              },
              SUBMENU_CLOSE_DELAY_MS);
    }
  },

  /**
   * Open submenu {@code item}. It does nothing if the submenu is
   * already opened.
   * @param {MenuItem} item The submenu item to open.
   */
  openSubmenu: function(item) {
    this.cancelSubmenuTimer_();
    if (this.submenuShown_ != item) {
      this.submenuShown_ = item;
      item.sendOpenSubmenuCommand();
    }
  },

  /**
   * Handle keyboard navigation and activation.
   * @private
   */
  onKeydown_: function(event) {
    switch (event.keyIdentifier) {
      case 'Left':
        this.moveToParent_();
        break;
      case 'Right':
        this.moveToSubmenu_();
        break;
      case 'Up':
        this.classList.add('mnemonic-enabled');
        this.findNextEnabled_(-1).selected = true;
      break;
      case 'Down':
        this.classList.add('mnemonic-enabled');
        this.findNextEnabled_(1).selected = true;
        break;
      case 'U+0009':  // tab
         break;
      case 'U+001B':  // escape
        chrome.send('close_all', []);
        break;
      case 'Enter':
      case 'U+0020':  // space
        if (this.current_) {
          this.current_.activate();
        }
        break;
    }
  },

  /**
   * Handle mnemonic keys.
   * @private
   */
  onKeypress_: function(event) {
    // Handles mnemonic.
    var c = String.fromCharCode(event.keyCode);
    var item = this.mnemonics_[c.toLowerCase()];
    if (item) {
      item.selected = true;
      item.activate();
    }
  },

  // Mouse Event handlers
  onClick_: function(event, item) {
    item.activate();
  },

  onMouseover_: function(event, item) {
    this.cancelSubmenuTimer_();
    // Ignore false mouseover event at (0,0) which is
    // emitted when opening submenu.
    if (item.attrs.enabled && event.clientX != 0 && event.clientY != 0) {
      item.selected = true;
    }
  },

  onMouseout_: function(event) {
    if (this.current_) {
      this.current_.selected = false;
    }
  },

  onResize_: function() {
    var up = document.getElementById('scroll-up');
    var down = document.getElementById('scroll-down');
    // this needs to be < 2 as empty page has height of 1.
    if (window.innerHeight < 2) {
      // menu window is not visible yet. just hide buttons.
      up.classList.add('hidden');
      down.classList.add('hidden');
      return;
    }
    // Do not use screen width to determin if we need scroll buttons
    // as the max renderer hight can be shorter than actual screen size.
    // TODO(oshima): Fix this when we implement transparent renderer.
    if (this.scrollHeight > window.innerHeight && this.scrollEnabled) {
      this.style.height = (window.innerHeight - this.buttonHeight_) + 'px';
      up.classList.remove('hidden');
      down.classList.remove('hidden');
    } else {
      this.style.height = '';
      up.classList.add('hidden');
      down.classList.add('hidden');
    }
  },

  onMouseWheel_: function(event) {
    var delta = event.wheelDelta / 5;
    this.scrollTop -= delta;
  },

  /**
   * Closes the submenu.
   * a submenu.
   * @private
   */
  closeSubmenu_: function(item) {
    this.submenuShown_ = null;
    this.cancelSubmenuTimer_();
    chrome.send('close_submenu', []);
  },

  /**
   * Move the selection to parent menu if the current menu is
   * a submenu.
   * @private
   */
  moveToParent_: function() {
    if (!this.isRoot) {
      if (this.current_) {
        this.current_.selected = false;
      }
      chrome.send('move_to_parent', []);
    }
  },

  /**
   * Move the selection to submenu if the currently selected
   * menu is a submenu.
   * @private
   */
  moveToSubmenu_: function () {
    var current = this.current_;
    if (current && current.attrs.type == 'submenu') {
      this.openSubmenu(current);
      chrome.send('move_to_submenu', []);
    }
  },

  /**
   * Find a next selectable item. If nothing is selected, the 1st
   * selectable item will be chosen. Returns null if nothing is
   * selectable.
   * @param {number} incr Specifies the direction to search, 1 to
   * downwards and -1 for upwards.
   * @private
   */
  findNextEnabled_: function(incr) {
    var len = this.items_.length;
    var index;
    if (this.current_) {
      index = this.getMenuItemIndexOf(this.current_);
    } else {
      index = incr > 0 ? -1 : len;
    }
    for (var i = 0; i < len; i++) {
      index = (index + incr + len) % len;
      var item = this.items_[index];
      if (item.attrs.enabled && item.attrs.type != 'separator' &&
          !item.classList.contains('hidden'))
        return item;
    }
    return null;
  },

  /**
   * Cancels timers to open/close submenus.
   * @private
   */
  cancelSubmenuTimer_: function() {
    clearTimeout(this.openSubmenuTimer_);
    this.openSubmenuTimer_ = 0;
    clearTimeout(this.closeSubmenuTimer_);
    this.closeSubmenuTimer_ = 0;
  },

  /**
   * Starts auto scroll.
   * @param {number} tick The number of pixels to scroll.
   * @private
   */
  autoScroll_: function(tick) {
    var previous = this.scrollTop;
    this.scrollTop += tick;
    var menu = this;
    this.scrollTimer_ = setTimeout(
        function() {
          menu.autoScroll_(tick);
        },
        SCROLL_INTERVAL_MS);
  },

  /**
   * Stops auto scroll.
   * @private
   */
  stopScroll_: function () {
    clearTimeout(this.scrollTimer_);
    this.scrollTimer_ = 0;
  },

  /**
   * Scrolls the viewport to make the selected item visible.
   * @private
   */
  makeSelectedItemVisible_: function(){
    this.current_.scrollIntoViewIfNeeded(false);
  },
};

/**
 * functions to be called from C++.
 */
function init(config) {
  document.getElementById('viewport').init(config);
}

function selectItem() {
  document.getElementById('viewport').showSelection();
}

function updateModel(model) {
  document.getElementById('viewport').updateModel(model);
}

function modelUpdated() {
  chrome.send('model_updated', []);
}

function enableScroll(enabled) {
  document.getElementById('viewport').scrollEnabled = enabled;
}
