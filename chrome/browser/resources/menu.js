// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// How long to wait to open submenu when mouse hovers.
var SUBMENU_OPEN_DELAY_MS = 200;
// How long to wait to close submenu when mouse left.
var SUBMENU_CLOSE_DELAY_MS = 500;
// Regular expression to match/find mnemonic key.
var MNEMONIC_REGEXP = /&(.)/;

/**
 * Sends 'click' DOMUI message.
 */
function sendClick(id) {
  chrome.send('click', [ id + '' ]);
}

/**
 * MenuItem class.
 */
function MenuItem(menu, id, attrs) {
  this.menu_ = menu;
  this.id = id;
  this.attrs = attrs;
}

MenuItem.prototype = {
  /**
   * Initialize the MenuItem.
   * @param {boolean} has_icon True if the menu has left icon.
   * @public
   */
  init: function(has_icon) {
    this.div = document.createElement('div');
    var attrs = this.attrs;
    if (attrs.type == 'separator') {
      this.div.className = 'separator';
    } else if (attrs.type == 'command' ||
               attrs.type == 'submenu' ||
               attrs.type == 'check' ||
               attrs.type == 'radio') {
      this.initMenuItem(has_icon);
    } else {
      this.div.className = 'menu_item disabled';
      this.div.innerHTML = 'unknown';
    }
    this.div.classList.add(has_icon ? 'has_icon' : 'noicon');
  },

  /**
   * Select this item.
   * @public
   */
  select: function() {
    this.div.classList.add('selected');
    this.menu_.setSelection(this);
  },

  /**
   * Unselect this item.
   * @public
   */
  unselect: function() {
    this.div.classList.remove('selected');
  },

  /**
   * Activat the menu item.
   * @public
   */
  activate: function() {
    if (this.attrs.type == 'submenu') {
      this.menu_.openSubmenu(this);
    } else if (this.attrs.type != 'separator') {
      sendClick(this.id);
    }
  },

  /**
   * Sends open_submenu DOMUI message.
   * @public
   */
  sendOpenSubmenuCommand: function() {
    chrome.send('open_submenu', [ this.id + '', this.getYCoord_() + '']);
  },

  /**
   * Returns y coordinate of this menu item
   * in the menu window.
   * @private
   */
  getYCoord_: function() {
    var element = this.div;
    var y = 0;
    while (element != null) {
      y += element.offsetTop;
      element = element.offsetParent;
    }
    return y;
  },

  /**
   * Internal method to initiailze the MenuItem's div element.
   * @private
   */
  initMenuItem: function(has_icon) {
    var attrs = this.attrs;
    this.div.className = 'menu_item ' + attrs.type;
    this.menu_.addHandlers(this);
    var mnemonic = MNEMONIC_REGEXP.exec(attrs.label);
    if (mnemonic) {
       var c = mnemonic[1];
       this.menu_.registerMnemonicKey(c, this);
    }
    var text = attrs.label.replace(
        MNEMONIC_REGEXP, '<span class="mnemonic">$1</span>');
    if (has_icon) {
      var icon = document.createElement('image');
      icon.className = 'left_icon';

      if (attrs.type == 'radio') {
        icon.src = attrs.checked ?
          menu_.config_.radioOnUrl : menu_.config_.radioOffUrl;
      } else if (attrs.icon) {
        icon.src = attrs.icon;
      } else if (attrs.type == 'check' && attrs.checked) {
        icon.src = menu_.config_.checkUrl;
      } else {
        icon.style.width = '12px';
      }
      this.div.appendChild(icon);
    }
    var label = document.createElement('div');
    label.className = 'menu_label';
    label.innerHTML = text;

    if (attrs.font) {
      label.style.font = attrs.font;
    }
    this.div.appendChild(label);

    if (attrs.type == 'submenu') {
      var icon = document.createElement('image');
      icon.src = menu_.config_.arrowUrl;
      icon.className = 'right_icon';
      this.div.appendChild(icon);
    }
  },
};

/**
 * Menu class.
 */
function Menu() {
  /* configuration object */
  this.config_ = null;
  /* currently selected menu item */
  this.current_ = null;
  /* the id of last element */
  this.last_id_ = -1;
  /* timers for opening/closing submenu */
  this.open_submenu_timer_ = 0;
  this.close_submenu_timer_ = 0;
  /* pointer to a submenu currently shown, if any */
  this.submenu_shown_ = null;
  /* list of menu items */
  this.items_ = [];
  /* map from mnemonic character to item to activate */
  this.mnemonics_ = {};
  /* true if this menu is root */
  this.is_root_ = false;
}

Menu.prototype = {
  /**
   * Initialize the menu.
   * @public
   */
  init: function(config) {
    this.config_ = config;
    document.getElementById('menu').onmouseout = this.onMouseout_.bind(this);
    document.addEventListener('keydown', this.onKeydown_.bind(this));
    /* disable text select */
    document.onselectstart = function() { return false; }
  },

  /**
   * A template method to create MenuItem object.
   * Subclass class can override to return custom menu item.
   * @public
   */
  createMenuItem: function(id, attrs) {
    return new MenuItem(this, id, attrs);
  },

  /**
   * Update and display the new model.
   * @public
   */
  updateModel: function(model) {
    this.is_root = model.is_root;
    this.current_ = null;
    this.items_ = [];
    this.mnemonics_ = {};

    var menu = document.getElementById('menu');
    menu.innerHTML = '';  // remove menu items

    var id = 0;

    for (i in model.items) {
      var attrs = model.items[i];
      var item = this.createMenuItem(id++, attrs);
      this.items_[item.id] = item;
      this.last_id_ = item.id;
      if (!item.attrs.visible) {
        continue;
      }

      item.init(model.has_icon);
      menu.appendChild(item.div);
    }
  },

  /**
   * Highlights the currently selected item, or
   * select the 1st selectable item if none is selected.
   * @public
   */
  showSelection: function() {
    if (this.current_) {
      this.current_.select();
    } else  {
      this.findNextEnabled_(1).select();
    }
  },

  /**
   * Registers mnemonic key.
   * @param {c} a mnemonic key to activate item.
   * @param {item} an item to be activated when {c} is pressed.
   * @public
   */
  registerMnemonicKey: function(c, item) {
    this.mnemonics_[c.toUpperCase()] = item;
    this.mnemonics_[c.toLowerCase()] = item;
  },

  /**
   * Add event handlers for the item.
   * @public
   */
  addHandlers: function(item) {
    var menu = this;
    item.div.addEventListener('mouseover', function(event) {
        menu.onMouseover_(event, item);
      });
    if (item.attrs.enabled) {
      item.div.addEventListener('mouseup', function(event) {
          menu.onClick_(event, item);
        });
    } else {
      item.div.classList.add('disabled');
    }
  },

  /**
   * Set the selected item. This also start or cancel
   * @public
   */
  setSelection: function(item) {
    if (this.current_ == item)
      return;

    if (this.current_ != null)
      this.current_.unselect();

    this.current_ = item;

    var menu = this;
    if (item.attrs.type == 'submenu') {
      if (this.submenu_shown_ != item) {
        this.open_submenu_timer_ =
            setTimeout(function() { menu.openSubmenu(item); },
                       SUBMENU_OPEN_DELAY_MS);
      } else {
        this.cancelSubmenuTimer_();
      }
    } else if (this.submenu_shown_) {
      this.cancelSubmenuTimer_();
      this.close_submenu_timer_ =
          setTimeout(function() { menu.closeSubmenu_(item); },
                     SUBMENU_CLOSE_DELAY_MS);
    }
  },

  /**
   * Open submenu {item}. It does nothing if the submenu is
   * already opened.
   * @param {item} the submenu item to open.
   * @public
   */
  openSubmenu: function(item) {
    this.cancelSubmenuTimer_();
    if (this.submenu_shown_ != item) {
      this.submenu_shown_ = item;
      item.sendOpenSubmenuCommand();
    }
  },

  /**
   * Handle keyboard navigatio and mnemonic keys.
   * @private
   */
  onKeydown_: function(event) {
    switch (event.keyCode) {
      case 27: /* escape */
        sendClick(-1); // -1 closes the menu.
        break;
      case 37: /* left */
        this.moveToParent_();
        break;
      case 39: /* right */
        this.moveToSubmenu_();
        break;
      case 38: /* up */
        document.getElementById('menu').className = 'mnemonic_enabled';
        this.findNextEnabled_(-1).select();
        break;
      case 40: /* down */
        document.getElementById('menu').className = 'mnemonic_enabled';
        this.findNextEnabled_(1).select();
        break;
      case 9:  /* tab */
        // TBD.
         break;
      case 13: /* return */
      case 32: /* space */
        if (this.current_) {
          this.current_.activate();
        }
        break;
      default:
        // Handles mnemonic.
        var c = String.fromCharCode(event.keyCode);
        var item = this.mnemonics_[c];
        if (item) item.activate();
    }
  },

  // Mouse Event handlers
  onClick_: function(event, item) {
    item.activate();
  },

  onMouseover_: function(event, item) {
    this.cancelSubmenuTimer_();
    if (this.current_ != item && item.attrs.enabled) {
      item.select();
    }
  },

  onMouseout_: function(event) {
    if (this.current_) {
      this.current_.unselect();
      this.current_ = null;
    }
  },

  /**
   * Closes the submenu.
   * a submenu.
   * @private
   */
  closeSubmenu_: function(item) {
    this.submenu_shown_ = null;
    this.cancelSubmenuTimer_();
    chrome.send('close_submenu', []);
  },

  /**
   * Move the selection to parent menu if the current menu is
   * a submenu.
   * @private
   */
  moveToParent_: function() {
    if (!this.is_root) {
      if (this.current_) {
        this.current_.unselect();
        this.current_ = null;
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
    if(current && current.attrs.type == 'submenu') {
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
     if (this.current_) {
       var id = parseInt(this.current_.id);
     } else {
       var id = incr > 0 ? -1 : this.last_id_ + 1;
     }
     for (var i = 0; i <= this.last_id_; i++) {
       if (id == 0 && incr < 0) {
         id = this.last_id_;
       } else if (id == this.last_id_ && incr > 0) {
         id = 0;
       } else {
         id += incr;
       }
       var item = this.items_[id];
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
    if (this.open_submenu_timer_) {
      clearTimeout(this.open_submenu_timer_);
      this.open_submenu_timer_ = 0;
    }
    if (this.close_submenu_timer_) {
      clearTimeout(this.close_submenu_timer_);
      this.close_submenu_timer_ = 0;
    }
  },
};

/**
 * functions to be called from C++.
 */
function init(config) {
  menu_.init(config);
}

function selectItem() {
  menu_.showSelection();
}

function updateModel(model) {
  menu_.updateModel(model);
}

var menu_;

function setMenu(menu) {
  menu_ = menu;
}
