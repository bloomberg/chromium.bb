// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui', function() {
  /** @const */ var Command = cr.ui.Command;

  /**
   * Creates a new menu item element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var MenuItem = cr.ui.define('div');

  /**
   * Creates a new menu separator element.
   * @return {cr.ui.MenuItem} The new separator element.
   */
  MenuItem.createSeparator = function() {
    var el = cr.doc.createElement('hr');
    MenuItem.decorate(el);
    return el;
  };

  MenuItem.prototype = {
    __proto__: HTMLButtonElement.prototype,

    /**
     * Initializes the menu item.
     */
    decorate: function() {
      var commandId;
      if ((commandId = this.getAttribute('command')))
        this.command = commandId;

      this.addEventListener('mouseup', this.handleMouseUp_);

      // Adding the 'custom-appearance' class prevents widgets.css from changing
      // the appearance of this element.
      this.classList.add('custom-appearance');

      var iconUrl;
      if ((iconUrl = this.getAttribute('icon')))
        this.iconUrl = iconUrl;
    },

    /**
     * The command associated with this menu item. If this is set to a string
     * of the form "#element-id" then the element is looked up in the document
     * of the command.
     * @type {cr.ui.Command}
     */
    command_: null,
    get command() {
      return this.command_;
    },
    set command(command) {
      if (this.command_) {
        this.command_.removeEventListener('labelChange', this);
        this.command_.removeEventListener('disabledChange', this);
        this.command_.removeEventListener('hiddenChange', this);
        this.command_.removeEventListener('checkedChange', this);
      }

      if (typeof command == 'string' && command[0] == '#') {
        command = this.ownerDocument.getElementById(command.slice(1));
        cr.ui.decorate(command, Command);
      }

      this.command_ = command;
      if (command) {
        if (command.id)
          this.setAttribute('command', '#' + command.id);

        this.label = command.label;
        this.disabled = command.disabled;
        this.hidden = command.hidden;

        this.command_.addEventListener('labelChange', this);
        this.command_.addEventListener('disabledChange', this);
        this.command_.addEventListener('hiddenChange', this);
        this.command_.addEventListener('checkedChange', this);
      }
    },

    /**
     * The text label.
     * @type {string}
     */
    get label() {
      return this.textContent;
    },
    set label(label) {
      this.textContent = label;
    },

    /**
     * Menu icon.
     * @type {string}
     */
    get iconUrl() {
      return this.style.backgroundImage;
    },
    set iconUrl(url) {
      this.style.backgroundImage = 'url(' + url + ')';
    },

    /**
     * @return {boolean} Whether the menu item is a separator.
     */
    isSeparator: function() {
      return this.tagName == 'HR';
    },

    /**
     * Handles mouseup events. This dispatches an active event and if there
     * is an assiciated command then that is executed.
     * @param {Event} The mouseup event object.
     * @private
     */
    handleMouseUp_: function(e) {
      if (!this.disabled && !this.isSeparator() && this.selected) {
        // Dispatch command event followed by executing the command object.
        if (cr.dispatchSimpleEvent(this, 'activate', true, true)) {
          var command = this.command;
          if (command)
            command.execute();
        }
      }
    },

    /**
     * Handles changes to the associated command.
     * @param {Event} e The event object.
     */
    handleEvent: function(e) {
      switch (e.type) {
        case 'disabledChange':
          this.disabled = this.command.disabled;
          break;
        case 'hiddenChange':
          this.hidden = this.command.hidden;
          break;
        case 'labelChange':
          this.label = this.command.label;
          break;
        case 'checkedChange':
          this.checked = this.command.checked;
          break;
      }
    }
  };

  /**
   * Whether the menu item is disabled or not.
   * @type {boolean}
   */
  cr.defineProperty(MenuItem, 'disabled', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the menu item is hidden or not.
   * @type {boolean}
   */
  cr.defineProperty(MenuItem, 'hidden', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the menu item is selected or not.
   * @type {boolean}
   */
  cr.defineProperty(MenuItem, 'selected', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the menu item is checked or not.
   * @type {boolean}
   */
  cr.defineProperty(MenuItem, 'checked', cr.PropertyKind.BOOL_ATTR);

  // Export
  return {
    MenuItem: MenuItem
  };
});
