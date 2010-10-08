// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

/**
 * Sends 'activate' DOMUI message.
 */
function sendActivate(index) {
  chrome.send('activate', [String(index), 'close_and_activate']);
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
   *   will be added to.
   * @param {Object} attrs JSON object that represents this menu items
   *   properties.  This is created from menu model in C code.  See
   *   chromeos/views/native_menu_domui.cc.
   * @param {boolean} hasIcon True if the menu has left icon.
   */
  init: function(menu, attrs, hasIcon) {
    this.menu_ = menu;
    this.attrs = attrs;
    var attrs = this.attrs;
    if (attrs.type == 'separator') {
      this.className = 'separator';
    } else if (attrs.type == 'command' ||
               attrs.type == 'submenu' ||
               attrs.type == 'check' ||
               attrs.type == 'radio') {
      this.initMenuItem_(hasIcon);
    } else {
      this.classList.add('disabled');
      this.textContent = 'unknown';
    }
    this.classList.add(hasIcon ? 'has-icon' : 'noicon');

    if (attrs.visible) {
      menu.appendChild(this);
    }
  },

  /**
   * Changes the selection state of the menu item.
   * @param {boolean} b True to set the selection, or false otherwise.
   */
  set selected(b) {
    if (b) {
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
      sendActivate(this.menu_.getMenuItemIndexOf(this));
    }
  },

  /**
   * Sends open_submenu DOMUI message.
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
  initMenuItem_: function(hasIcon) {
    var attrs = this.attrs;
    this.className = 'menu-item ' + attrs.type;
    this.menu_.addHandlers(this);
    var mnemonic = MNEMONIC_REGEXP.exec(attrs.label);
    if (mnemonic) {
       var c = mnemonic[2];
       this.menu_.registerMnemonicKey(c, this);
    }
    if (hasIcon) {
      this.classList.add('left-icon');

      var url;
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
        this.style.backgroundImage = "url(" + url + ")";
      }
    }
    var label = document.createElement('div');
    label.className = 'menu-label';

    if (!mnemonic) {
      label.textContent = attrs.label;
    } else {
      label.appendChild(document.createTextNode(mnemonic[1]));
      label.appendChild(document.createElement('span'));
      label.appendChild(document.createTextNode(mnemonic[3]));
      label.childNodes[1].className = 'mnemonic';
      label.childNodes[1].textContent = mnemonic[2];
    }

    if (attrs.font) {
      label.style.font = attrs.font;
    }
    this.appendChild(label);

    if (attrs.type == 'submenu') {
      // This overrides left-icon's position, but it's OK as submenu
      // shoudln't have left-icon.
      this.classList.add('right-icon');
      this.style.backgroundImage = "url(" + this.menu_.config_.arrowUrl + ")";
    }
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
   * Decorates the menu element.
   */
  decorate: function() {
    this.id = 'viewport';
  },

  /**
   * Initialize the menu.
   * @param {Object} config Configuration parameters in JSON format.
   *  See chromeos/views/native_menu_domui.cc for details.
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
   * Returns the index of the {@code item}.
   */
  getMenuItemIndexOf: function(item) {
    return this.items_.indexOf(item);
  },

  /**
   * A template method to create MenuItem object.
   * Subclass class can override to return custom menu item.
   */
  createMenuItem: function() {
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
      item.init(this, attrs, model.hasIcon);
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
   * Registers mnemonic key.
   * @param {string} c A mnemonic key to activate item.
   * @param {MenuItem} item An item to be activated when {@code c} is pressed.
   */
  registerMnemonicKey: function(c, item) {
    this.mnemonics_[c.toLowerCase()] = item;
  },

  /**
   * Add event handlers for the item.
   */
  addHandlers: function(item) {
    var menu = this;
    item.addEventListener('mouseover', function(event) {
      menu.onMouseover_(event, item);
    });
    if (item.attrs.enabled) {
      item.addEventListener('mouseup', function(event) {
        menu.onClick_(event, item);
      });
    } else {
      item.classList.add('disabled');
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
   * @param {MenuItem} item the submenu item to open.
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
    if (item)
      item.activate();
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
    if (window.innerHeight == 0) {
      // menu window is not visible yet. just hide buttons.
      up.classList.add('hidden');
      down.classList.add('hidden');
      return;
    }
    if (this.scrollHeight > window.innerHeight) {
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
   * @param {number} incr specifies the direction to search, 1 to
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
      if (item.attrs.enabled && item.attrs.type != 'separator')
        return item;
    }
    return null;
  },

  /**
   * Cancels timers to open/close submenus.
   * @private
   */
  cancelSubmenuTimer_: function() {
    if (this.openSubmenuTimer_) {
      clearTimeout(this.openSubmenuTimer_);
      this.openSubmenuTimer_ = 0;
    }
    if (this.closeSubmenuTimer_) {
      clearTimeout(this.closeSubmenuTimer_);
      this.closeSubmenuTimer_ = 0;
    }
  },

  /**
   * Starts auto scroll.
   * @param {number} tick the number of pixels to scroll.
   * @private
   */
  autoScroll_: function(tick) {
    var previous = this.scrollTop;
    this.scrollTop += tick;
    if (this.scrollTop != previous) {
      var menu = this;
      this.scrollTimer_ = setTimeout(
          function() {
            menu.autoScroll_(tick);
          },
          SCROLL_INTERVAL_MS);
    }
  },

  /**
   * Stops auto scroll.
   * @private
   */
  stopScroll_: function () {
    if (this.scrollTimer_) {
      clearTimeout(this.scrollTimer_);
      this.scrollTimer_ = 0;
    }
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
