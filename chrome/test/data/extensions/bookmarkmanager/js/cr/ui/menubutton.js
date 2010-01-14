// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  const Menu = cr.ui.Menu;

  /**
   * Creates a new menu button element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLButtonElement}
   */
  var MenuButton = cr.ui.define('button');

  MenuButton.prototype = {
    __proto__: HTMLButtonElement.prototype,

    /**
     * Initializes the menu button.
     */
    decorate: function() {
      this.addEventListener('mousedown', this);
      this.addEventListener('keydown', this);

      var menu;
      if ((menu = this.getAttribute('menu')))
        this.menu = menu;
    },

    /**
     * The menu associated with the menu button.
     * @type {cr.ui.Menu}
     */
    get menu() {
      return this.menu_;
    },
    set menu(menu) {
      if (typeof menu == 'string' && menu[0] == '#') {
        menu = this.ownerDocument.getElementById(menu.slice(1));
        cr.ui.decorate(menu, Menu);
      }

      this.menu_ = menu;
      if (menu) {
        if (menu.id)
          this.setAttribute('menu', '#' + menu.id);
      }
    },

    /**
     * Handles event callbacks.
     * @param {Event} e The event object.
     */
    handleEvent: function(e) {
      if (!this.menu)
        return;

      switch (e.type) {
        case 'mousedown':
          if (e.currentTarget == this.ownerDocument) {
            if (!this.contains(e.target) && !this.menu.contains(e.target))
              this.hideMenu();
            else
              e.preventDefault();
          } else {
            if (this.isMenuShown()) {
              this.hideMenu();
            } else {
              this.showMenu();
              // Prevent the button from stealing focus on mousedown.
              e.preventDefault();
            }
          }
          break;
        case 'keydown':
          this.handleKeyDown(e);
          // If the menu is visible we let it handle all the keyboard events.
          if (this.isMenuShown() && e.currentTarget == this.ownerDocument) {
            this.menu.handleKeyDown(e);
            e.preventDefault();
            e.stopPropagation();
          }
          break;

        case 'activate':
        case 'blur':
          this.hideMenu();
          break;
      }
    },

    /**
     * Shows the menu.
     */
    showMenu: function() {
      this.menu.style.display = 'block';
      // when the menu is shown we steal all keyboard events.
      this.ownerDocument.addEventListener('keydown', this, true);
      this.ownerDocument.addEventListener('mousedown', this, true);
      this.ownerDocument.addEventListener('blur', this, true);
      this.menu.addEventListener('activate', this);
      this.positionMenu_();
    },

    /**
     * Hides the menu.
     */
    hideMenu: function() {
      this.menu.style.display = 'none';
      this.ownerDocument.removeEventListener('keydown', this, true);
      this.ownerDocument.removeEventListener('mousedown', this, true);
      this.ownerDocument.removeEventListener('blur', this, true);
      this.menu.removeEventListener('activate', this);
      this.menu.selectedIndex = -1;
    },

    /**
     * Whether the menu is shown.
     */
    isMenuShown: function() {
      return window.getComputedStyle(this.menu).display != 'none';
    },

    /**
     * Positions the menu below the menu button. At this point we do not use any
     * advanced positioning logic to ensure the menu fits in the viewport.
     * @private
     */
    positionMenu_: function() {
      var buttonRect = this.getBoundingClientRect();
      this.menu.style.top = buttonRect.bottom + 'px';
      if (getComputedStyle(this).direction == 'rtl') {
        var menuRect = this.menu.getBoundingClientRect();
        this.menu.style.left = buttonRect.right - menuRect.width + 'px';
      } else {
        this.menu.style.left = buttonRect.left + 'px';
      }
    },

    /**
     * Handles the keydown event for the menu button.
     */
    handleKeyDown: function(e) {
      switch (e.keyIdentifier) {
        case 'Down':
        case 'Up':
        case 'Enter':
        case 'U+0020': // Space
          if (!this.isMenuShown())
            this.showMenu();
          e.preventDefault();
          break;
        case 'Esc':
        case 'U+001B': // Maybe this is remote desktop playing a prank?
          this.hideMenu();
          break;
      }
    }
  };

  // Export
  return {
    MenuButton: MenuButton
  };
});
