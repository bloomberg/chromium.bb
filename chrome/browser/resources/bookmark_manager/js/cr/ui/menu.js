// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {

  const MenuItem = cr.ui.MenuItem;

  /**
   * Creates a new menu element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLMenuElement}
   */
  var Menu = cr.ui.define('menu');

  Menu.prototype = {
    __proto__: HTMLMenuElement.prototype,

    /**
     * Initializes the menu element.
     */
    decorate: function() {
      this.addEventListener('mouseover', this.handleMouseOver_);
      this.addEventListener('mouseout', this.handleMouseOut_);

      // Decorate the children as menu items.
      var children = this.children;
      for (var i = 0, child; child = children[i]; i++) {
        cr.ui.decorate(child, MenuItem);
      }
    },

    /**
     * Walks up the ancestors until a menu item belonging to this menu is found.
     * @param {Element} el
     * @return {cr.ui.MenuItem} The found menu item or null.
     * @private
     */
    findMenuItem_: function(el) {
      while (el && el.parentNode != this) {
        el = el.parentNode;
      }
      return el;
    },

    /**
     * Handles mouseover events and selects the hovered item.
     * @param {Event} e The mouseover event.
     * @private
     */
    handleMouseOver_: function(e) {
      var overItem = this.findMenuItem_(e.target);
      this.selectedItem = overItem;
    },

    /**
     * Handles mouseout events and deselects any selected item.
     * @param {Event} e The mouseout event.
     * @private
     */
    handleMouseOut_: function(e) {
      this.selectedItem = null;
    },

    /**
     * The index of the selected item.
     * @type {boolean}
     */
    // getter and default value is defined using cr.defineProperty.
    set selectedIndex(selectedIndex) {
      if (this.selectedIndex_ != selectedIndex) {
        var oldSelectedItem = this.selectedItem;
        this.selectedIndex_ = selectedIndex;
        if (oldSelectedItem)
          oldSelectedItem.selected = false;
        var item = this.selectedItem;
        if (item)
          item.selected = true;

        cr.dispatchSimpleEvent(this, 'change');
      }
    },

    /**
     * The selected menu item or null if none.
     * @type {cr.ui.MenuItem}
     */
    get selectedItem() {
      return this.children[this.selectedIndex];
    },
    set selectedItem(item) {
      var index = Array.prototype.indexOf.call(this.children, item);
      this.selectedIndex = index;
    },

    /**
     * This is the function that handles keyboard navigation. This is usually
     * called by the element responsible for managing the menu.
     * @param {Event} e The keydown event object.
     * @return {boolean} Whether the event was handled be the menu.
     */
    handleKeyDown: function(e) {
      var item = this.selectedItem;

      var self = this;
      function selectNextVisible(m) {
        var children = self.children;
        var len = children.length;
        var i = self.selectedIndex;
        if (i == -1 && m == -1) {
          // Edge case when we need to go the last item fisrt.
          i = 0;
        }
        while (true) {
          i = (i + m + len) % len;
          item = children[i];
          if (item && !item.isSeparator() && !item.hidden)
            break;
        }
        if (item)
          self.selectedIndex = i;
      }

      switch (e.keyIdentifier) {
        case 'Down':
          selectNextVisible(1);
          return true;
        case 'Up':
          selectNextVisible(-1);
          return true;
        case 'Enter':
        case 'U+0020': // Space
          if (item) {
            if (cr.dispatchSimpleEvent(item, 'activate', true, true)) {
              if (item.command)
                item.command.execute();
            }
          }
          return true;
      }

      return false;
    }
  };

  /**
   * The selected menu item.
   * @type {number}
   */
  cr.defineProperty(Menu, 'selectedIndex', cr.PropertyKind.JS, -1);

  // Export
  return {
    Menu: Menu
  };
});
