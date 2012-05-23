// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a combobutton control.
 */

cr.define('cr.ui', function() {
  /**
   * Creates a new combobutton element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLUListElement}
   */
  var ComboButton = cr.ui.define(cr.ui.MenuButton);


  ComboButton.prototype = {
    __proto__: cr.ui.MenuButton.prototype,

    defaultItem_: null,

    /**
     * Truncates drop-down list.
     */
    clear: function() {
      this.menu.clear();
      this.multiple = false;
    },

    addDropDownItem: function(item) {
      this.multiple = true;
      this.menu.addMenuItem(item).data = item;
    },

    /**
     * Adds separator to drop-down list.
     */
    addSeparator: function() {
      this.menu.addSeparator();
    },

    /**
     * Default item to fire on combobox click
     */
    get defaultItem() {
      return this.defaultItem_;
    },
    set defaultItem(defaultItem) {
      this.defaultItem_ = defaultItem;
      if (defaultItem.label) {
        this.labelNode_.textContent = defaultItem.label;
      } else {
        this.labelNode_.textContent = '';
      }

      if (defaultItem.iconUrl) {
        this.iconNode_.src = defaultItem.iconUrl;
      } else {
        this.iconNode_.src = '';
      }
    },

    /**
     * Initializes the element.
     */
    decorate: function() {
      cr.ui.MenuButton.prototype.decorate.call(this);

      this.classList.add('combobutton');

      this.iconNode_ = this.ownerDocument.createElement('img');
      this.appendChild(this.iconNode_);

      this.labelNode_ = this.ownerDocument.createElement('span');
      this.appendChild(this.labelNode_);

      var triggerIcon = this.ownerDocument.createElement('span');
      triggerIcon.className = 'disclosureindicator';
      this.trigger_ = this.ownerDocument.createElement('div');
      this.trigger_.appendChild(triggerIcon);

      this.appendChild(this.trigger_);

      this.addEventListener('click', this.handleButtonClick_.bind(this));

      this.trigger_.addEventListener('click',
          this.handleTriggerClicked_.bind(this));

      this.menu.addEventListener('activate',
          this.handleMenuActivate_.bind(this));

      // Remove mousedown event listener created by MenuButton::decorate,
      // and move it down to trigger_.
      this.removeEventListener('mousedown', this);
      this.trigger_.addEventListener('mousedown', this);
    },

    /**
     * Handles the keydown event for the menu button.
     */
    handleKeyDown: function(e) {
      switch (e.keyIdentifier) {
        case 'Down':
        case 'Up':
          if (!this.isMenuShown())
            this.showMenu();
          e.preventDefault();
          break;
        case 'Esc':
        case 'U+001B': // Maybe this is remote desktop playing a prank?
          this.hideMenu();
          break;
      }
    },

    handleTriggerClicked_: function(event) {
      event.stopPropagation();
    },

    handleMenuActivate_: function(event) {
      this.dispatchSelectEvent(event.target.data);
    },

    handleButtonClick_: function() {
      this.dispatchSelectEvent(this.defaultItem_);
    },

    dispatchSelectEvent: function(item) {
      var selectEvent = new Event('select');
      selectEvent.item = item;
      this.dispatchEvent(selectEvent);
    }
  };

  cr.defineProperty(ComboButton, 'disabled', cr.PropertyKind.BOOL_ATTR);
  cr.defineProperty(ComboButton, 'multiple', cr.PropertyKind.BOOL_ATTR);

  return {
    ComboButton: ComboButton
  };
});
