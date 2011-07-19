// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * ButtonCommand class for small buttons on menu items.
 */
var ButtonCommand = cr.ui.define('div');

ButtonCommand.prototype = {
  __proto__: HTMLDivElement.prototype,

  /**
   * Decorate Button item.
   */
  decorate: function() {
  },

  /**
   * Changes the selection state of the menu item.
   * @param {boolean} selected True to set the selection, or false otherwise.
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
    sendActivate(this.menu_.getMenuItemIndexOf(this),
                 'close_and_activate');
  },
};

/**
 * EditCommand implements Copy and Paste command.
 */
var EditCommand = cr.ui.define('div');

EditCommand.prototype = {
  __proto__: ButtonCommand.prototype,

  /**
   * Initialize the menu item.
   * @override
   */
  init: function(menu, attrs, model) {
    this.menu_ = menu;
    this.attrs = attrs;
    if (this.attrs.font) {
      this.style.font = attrs.font;
    }
    menu.addHandlers(this, this);
    if (attrs.command_id == menu.config_.IDC_COPY) {
      menu.addLabelTo(this, menu.config_.IDS_COPY, this,
                      false /* no mnemonic */);
    } else {
      menu.addLabelTo(this, menu.config_.IDS_PASTE, this,
                      false /* no mnemonic */);
    }
  },
};

/**
 * EditMenuItem which has Copy and Paste commands inside.
 */
var EditMenuItem = cr.ui.define('div');

EditMenuItem.prototype = {
  __proto__: MenuItem.prototype,

  /**
   * Initialize
   */
  decorate: function() {
    this.className = 'menu-item';
    this.label_ = document.createElement('div');
    this.label_.className = 'menu-label';
    this.cut_ = document.createElement('div');
    this.cut_.className = 'edit-button left-button';
    this.copy_ = new EditCommand();
    this.copy_.className = 'edit-button center-button';
    this.paste_ = new EditCommand();
    this.paste_.className = 'edit-button right-button';

    this.appendChild(this.label_);
    this.appendChild(this.cut_);
    this.appendChild(this.copy_);
    this.appendChild(this.paste_);
  },

  /**
   * Activates the command.
   * @override
   */
  activate: function() {
    sendActivate(this.menu_.getMenuItemIndexOf(this),
                 'close_and_activate');
  },

  /**
   * @override
   */
  set selected(selected) {
    if (selected) {
      this.cut_.classList.add('selected');
      this.menu_.selectedItem = this;
    } else {
      this.cut_.classList.remove('selected');
    }
  },

  /**
   * Initialize the edit items with configuration info.
   * @override
   */
  initMenuItem_: function() {
    this.label_.textContent =
        this.menu_.config_.IDS_EDIT2;
    if (this.attrs.font) {
      this.label_.style.font = this.attrs.font;
      this.cut_.style.font = this.attrs.font;
    }
    this.menu_.addLabelTo(
        this, this.menu_.config_.IDS_CUT, this.cut_,
        false /* no mnemonic */);
    this.menu_.addHandlers(this, this.cut_);
  },
};

/**
 * ZoomCommand class implements Zoom plus and fullscreen.
 */
var ZoomCommand = cr.ui.define('div');

ZoomCommand.prototype = {
  __proto__: ButtonCommand.prototype,

  /**
   * Initialize the menu item.
   * @override
   */
  init: function(menu, attrs, model) {
    this.menu_ = menu;
    this.attrs = attrs;
    menu.addHandlers(this, this);
    if (attrs.command_id == menu.config_.IDC_ZOOM_PLUS) {
      this.textContent = '+';
    }
    if (this.attrs.font) {
      this.style.font = attrs.font;
    }
  },

  /**
   * Activate zoom plus and full screen commands.
   * @override
   */
  activate: function() {
    sendActivate(this.menu_.getMenuItemIndexOf(this),
                 this.attrs.command_id == this.menu_.config_.IDC_ZOOM_PLUS ?
                 'activate_no_close' : 'close_and_activate');
  },
};

/**
 * ZoomMenuItem which has plus and fullscreen buttons inside.
 */
var ZoomMenuItem = cr.ui.define('div');

ZoomMenuItem.prototype = {
  __proto__: MenuItem.prototype,

  /**
   * Decorate Zoom button item.
   */
  decorate: function() {
    this.className = 'menu-item';

    this.label_ = document.createElement('div');
    this.label_.className = 'menu-label';
    this.minus_ = document.createElement('div');
    this.minus_.className = 'zoom-button left-button';
    this.minus_.textContent = '-';
    this.plus_ = new ZoomCommand();
    this.plus_.className = 'zoom-button right-button';
    this.percent_ = document.createElement('div');
    this.percent_.className = 'zoom-percent center-button';
    this.fullscreen_ = new ZoomCommand();
    this.fullscreen_.className = 'fullscreen';

    this.appendChild(this.label_);
    this.appendChild(this.minus_);
    this.appendChild(this.percent_);
    this.appendChild(this.plus_);
    this.appendChild(this.fullscreen_);
  },

  /**
   * Activates the cut command.
   * @override
   */
  activate: function() {
    sendActivate(this.menu_.getMenuItemIndexOf(this),
                 'activate_no_close');
  },

  /**
   * Updates zoom controls.
   * @params {JSON} params JSON object to configure zoom controls.
   */
  updateZoomControls: function(params) {
    this.attrs.enabled = params.plus;
    if (params.plus) {
      this.plus_.classList.remove('disabled');
    } else {
      this.plus_.classList.add('disabled');
    }
    this.attrs.enabled = params.minus;
    if (params.minus) {
      this.classList.remove('disabled');
    } else {
      this.classList.add('disabled');
    }
    this.percent_.textContent = params.percent;
  },

  /**
   * @override
   */
  set selected(selected) {
    if (selected) {
      this.minus_.classList.add('selected');
      this.menu_.selectedItem = this;
    } else {
      this.minus_.classList.remove('selected');
    }
  },

  /**
   * Initializes the zoom menu item with configuration info.
   * @override
   */
  initMenuItem_: function() {
    this.label_.textContent =
        this.menu_.config_.IDS_ZOOM_MENU2;
    this.menu_.addHandlers(this, this.minus_);

    if (this.attrs.font) {
      this.label_.style.font = this.attrs.font;
      this.minus_.style.font = this.attrs.font;
      this.percent_.style.font = this.attrs.font;
    }
  },
};

/**
 * WrenchMenu
 */
var WrenchMenu = cr.ui.define('div');

WrenchMenu.prototype = {
  __proto__: Menu.prototype,

  /**
   * Decorate Zoom button item.
   */
  decorate: function() {
    Menu.prototype.decorate.call(this);
    this.edit_ = new EditMenuItem();
    this.zoom_ = new ZoomMenuItem();
  },

  /**
   * Create a MenuItem for given {@code attrs}.
   * @override
   */
  createMenuItem: function(attrs) {
    switch(attrs.command_id) {
      case this.config_.IDC_CUT:
        return this.edit_;
      case this.config_.IDC_COPY:
        return this.edit_.copy_;
      case this.config_.IDC_PASTE:
        return this.edit_.paste_;
      case this.config_.IDC_ZOOM_MINUS:
        return this.zoom_;
      case this.config_.IDC_ZOOM_PLUS:
        return this.zoom_.plus_;
      case this.config_.IDC_FULLSCREEN:
        return this.zoom_.fullscreen_;
      default:
        return new MenuItem();
    }
  },

  updateZoomControls: function(params) {
    this.zoom_.updateZoomControls(params);
  },
};

function updateZoomControls(params) {
  var menu = document.getElementById('viewport');
  menu.updateZoomControls(params);
}
