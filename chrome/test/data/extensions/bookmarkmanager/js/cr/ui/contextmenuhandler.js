// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {

  /**
   * Handles context menus.
   * @constructor
   */
  function ContextMenuHandler() {}

  ContextMenuHandler.prototype = {

    /**
     * The menu that we are currently showing.
     * @type {cr.ui.Menu}
     */
    menu_: null,
    get menu() {
      return this.menu_;
    },

    /**
     * Shows a menu as a context menu.
     * @param {!Event} e The event triggering the show (usally a contextmenu
     *     event).
     * @param {!cr.ui.Menu} menu The menu to show.
     */
    showMenu: function(e, menu) {
      this.menu_ = menu;

      menu.style.display = 'block';
      // when the menu is shown we steal all keyboard events.
      menu.ownerDocument.addEventListener('keydown', this, true);
      menu.ownerDocument.addEventListener('mousedown', this, true);
      menu.ownerDocument.addEventListener('blur', this, true);
      menu.addEventListener('activate', this);
      this.positionMenu_(e, menu);
    },

    /**
     * Hide the currently shown menu.
     */
    hideMenu: function() {
      var menu = this.menu;
      if (!menu)
        return;

      menu.style.display = 'none';
      menu.ownerDocument.removeEventListener('keydown', this, true);
      menu.ownerDocument.removeEventListener('mousedown', this, true);
      menu.ownerDocument.removeEventListener('blur', this, true);
      menu.removeEventListener('activate', this);
      menu.selectedIndex = -1;
      this.menu_ = null;

      // On windows we might hide the menu in a right mouse button up and if
      // that is the case we wait some short period before we allow the menu
      // to be shown again.
      this.hideTimestamp_ = Date.now();
    },

    /**
     * Positions the menu
     * @param {!Event} e The event object triggering the showing.
     * @param {!cr.ui.Menu} menu The menu to position.
     * @private
     */
    positionMenu_: function(e, menu) {
      // TODO(arv): Handle scrolled documents when needed.

      var x, y;
      // When the user presses the context menu key (on the keyboard) we need
      // to detect this.
      if (e.screenX == 0 && e.screenY == 0) {
        var rect = e.currentTarget.getBoundingClientRect();
        x = rect.left;
        y = rect.top;
      } else {
        x = e.clientX;
        y = e.clientY;
      }

      var menuRect = menu.getBoundingClientRect();
      var bodyRect = menu.ownerDocument.body.getBoundingClientRect();

      // Does menu fit below?
      if (y + menuRect.height > bodyRect.height) {
        // Does menu fit above?
        if (y - menuRect.height >= 0) {
          y -= menuRect.height;
        } else {
          // Menu did not fit above nor below.
          y = 0;
          // We could resize the menu here but lets not worry about that at this
          // point.
        }
      }

      // Does menu fit to the right?
      if (x + menuRect.width > bodyRect.width) {
        // Does menu fit to the left?
        if (x - menuRect.width >= 0) {
          x -= menuRect.width;
        } else {
          // Menu did not fit to the right nor to the left.
          x = 0;
          // We could resize the menu here but lets not worry about that at this
          // point.
        }
      }

      menu.style.left = x + 'px';
      menu.style.top = y + 'px';
    },

    /**
     * Handles event callbacks.
     * @param {!Event} e The event object.
     */
    handleEvent: function(e) {
      // Context menu is handled even when we have no menu.
      if (e.type != 'contextmenu' && !this.menu)
        return;

      switch (e.type) {
        case 'mousedown':
          if (!this.menu.contains(e.target))
            this.hideMenu();
          else
            e.preventDefault();
          break;
        case 'keydown':
          // keyIdentifier does not report 'Esc' correctly
          if (e.keyCode == 27 /* Esc */) {
            this.hideMenu();

          // If the menu is visible we let it handle all the keyboard events.
          } else if (this.menu) {
            this.menu.handleKeyDown(e);
            e.preventDefault();
            e.stopPropagation();
          }
          break;

        case 'activate':
        case 'blur':
          this.hideMenu();
          break;

        case 'contextmenu':
          if ((!this.menu || !this.menu.contains(e.target)) &&
              (!this.hideTimestamp_ || Date.now() - this.hideTimestamp_ > 50))
            this.showMenu(e, e.currentTarget.contextMenu);
          e.preventDefault();
          // Don't allow elements further up in the DOM to show their menus.
          e.stopPropagation();
          break;
      }
    },

    /**
     * Adds a contextMenu property to an element or element class.
     * @param {!Element|!Function} element The element or class to add the
     *     contextMenu property to.
     */
    addContextMenuProperty: function(element) {
      if (typeof element == 'function')
        element = element.prototype;

      element.__defineGetter__('contextMenu', function() {
        return this.contextMenu_;
      });
      element.__defineSetter__('contextMenu', function(menu) {
        var oldContextMenu = this.contextMenu;

        if (typeof menu == 'string' && menu[0] == '#') {
          menu = this.ownerDocument.getElementById(menu.slice(1));
          cr.ui.decorate(menu, Menu);
        }

        if (menu === oldContextMenu)
          return;

        if (oldContextMenu && !menu)
          this.removeEventListener('contextmenu', contextMenuHandler);
        if (menu && !oldContextMenu)
          this.addEventListener('contextmenu', contextMenuHandler);

        this.contextMenu_ = menu;

        if (menu && menu.id)
          this.setAttribute('contextmenu', '#' + menu.id);

        cr.dispatchPropertyChange(this, 'contextMenu', menu, oldContextMenu);
      });
    }
  };

  /**
   * The singleton context menu handler.
   * @type {!ContextMenuHandler}
   */
  var contextMenuHandler = new ContextMenuHandler;

  // Export
  return {
    contextMenuHandler: contextMenuHandler
  };
});
