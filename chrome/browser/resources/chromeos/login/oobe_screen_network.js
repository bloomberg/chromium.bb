// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe network screen implementation.
 */

cr.define('oobe', function() {
  /**
   * Creates a new container for the drop down menu items.
   * @constructor
   * @extends{HTMLDivElement}
   */
  var DropDownContainer  = cr.ui.define('div');

  DropDownContainer.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.classList.add('dropdown-container');
      // Selected item in the menu list.
      this.selectedItem = null;
      // First item which could be selected.
      this.firstItem = null;
    },

    /**
     * Selects new item.
     * @param {!Object} selectedItem Item to be selected.
     */
    selectItem: function(selectedItem) {
      if (this.selectedItem)
        this.selectedItem.classList.remove('hover');
      selectedItem.classList.add('hover');
      this.selectedItem = selectedItem;
    }
  };

  /**
   * Creates a new DropDown div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var DropDown = cr.ui.define('div');

  DropDown.ITEM_DIVIDER_ID = -2;

  DropDown.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.appendChild(this.createOverlay_());
      this.appendChild(this.createTitle_());
      this.appendChild(new DropDownContainer());

      this.isShown = false;
      this.addEventListener('keydown', this.keyDownHandler_);
    },

    /**
     * Returns true if dropdown menu is shown.
     * @type {bool} Whether menu element is shown.
     */
    get isShown() {
      return !this.container.hidden;
    },

    /**
     * Sets dropdown menu visibility.
     * @param {bool} show New visibility state for dropdown menu.
     */
    set isShown(show) {
      this.firstElementChild.hidden = !show;
      this.container.hidden = !show;
      if (show)
        this.container.selectItem(this.container.firstItem);
    },

    /**
     * Returns title button.
     */
    get titleButton() {
      return this.childNodes[1];
    },

    /**
     * Returns container of the menu items.
     */
    get container() {
      return this.lastElementChild;
    },

    /**
     * Sets title and icon.
     * @param {string} title Text on dropdown.
     * @param {string} icon Icon in dataURL format.
     */
    setTitle: function(title, icon) {
      // TODO(nkostylev): Icon support for dropdown title.
      this.titleButton.textContent = title;
    },

    /**
     * Sets dropdown items.
     * @param {Array} items Dropdown items array.
     */
    setItems: function(items) {
      this.container.innerHTML = '';
      this.container.firstItem = null;
      this.container.selectedItem = null;
      for (var i = 0; i < items.length; ++i) {
        var item = items[i];
        if ('sub' in item) {
          // Workaround for submenus, add items on top level.
          // TODO(altimofeev): support submenus.
          for (var j = 0; j < item.sub.length; ++j)
            this.createItem_(this.container, item.sub[j]);
          continue;
        }
        this.createItem_(this.container, item);
      }
      this.container.selectItem(this.container.firstItem);
    },

    /**
     * Creates dropdown item element and adds into container.
     * @param {HTMLElement} container Container where item is added.
     * @param {!Object} item Item to be added.
     * @private
     */
    createItem_ : function(container, item) {
      var itemContentElement;
      var className = 'dropdown-item';
      if (item.id == DropDown.ITEM_DIVIDER_ID) {
        className = 'dropdown-divider';
        itemContentElement = this.ownerDocument.createElement('hr');
      } else {
        var span = this.ownerDocument.createElement('span');
        itemContentElement = span;
        span.textContent = item.label;
        if ('bold' in item && item.bold)
          span.classList.add('bold');
        var image = this.ownerDocument.createElement('img');
        if (item.icon)
          image.src = item.icon;
      }

      var itemElement = this.ownerDocument.createElement('div');
      itemElement.classList.add(className);
      itemElement.appendChild(itemContentElement);
      itemElement.iid = item.id;
      itemElement.controller = this;
      var enabled = 'enabled' in item && item.enabled;
      if (!enabled)
        itemElement.classList.add('disabled-item');

      if (item.id > 0) {
        var wrapperDiv = this.ownerDocument.createElement('div');
        wrapperDiv.classList.add('dropdown-item-container');
        var imageDiv = this.ownerDocument.createElement('div');
        imageDiv.classList.add('dropdown-image');
        imageDiv.appendChild(image);
        wrapperDiv.appendChild(imageDiv);
        wrapperDiv.appendChild(itemElement);
        wrapperDiv.addEventListener('click', function f(e) {
          var item = this.lastElementChild;
          if (item.iid < -1 || item.classList.contains('disabled-item'))
            return;
          item.controller.isShown = false;
          if (item.iid >= 0)
            chrome.send('networkItemChosen', [item.iid]);
        });
        wrapperDiv.addEventListener('mouseover', function f(e) {
          this.parentNode.selectItem(this);
        });
        itemElement = wrapperDiv;
      }
      container.appendChild(itemElement);
      if (!container.firstItem && item.id >= 0) {
        container.firstItem = itemElement;
      }
    },

    /**
     * Creates dropdown overlay element, which catches outside clicks.
     * @type {HTMLElement}
     * @private
     */
    createOverlay_: function() {
      var overlay = this.ownerDocument.createElement('div');
      overlay.classList.add('dropdown-overlay');
      overlay.addEventListener('click', function() {
        this.parentNode.titleButton.focus();
        this.parentNode.isShown = false;
      });
      return overlay;
    },

    /**
     * Creates dropdown title element.
     * @type {HTMLElement}
     * @private
     */
    createTitle_: function() {
      var el = this.ownerDocument.createElement('button');
      el.classList.add('dropdown-title');
      el.iid = -1;
      el.controller = this;
      el.enterPressed = false;

      el.addEventListener('click', function f(e) {
        this.focus();
        this.controller.isShown = !this.controller.isShown;

        if (this.enterPressed) {
          this.enterPressed = false;
          if (!this.controller.isShown) {
            var item = this.controller.container.selectedItem.lastElementChild;
            if (item.iid >= 0 && !item.classList.contains('disabled-item'))
              chrome.send('networkItemChosen', [item.iid]);
          }
        }
      });
      return el;
    },

    /**
     * Handles keydown event from the keyboard.
     * @private
     * @param {!Event} e Keydown event.
     */
    keyDownHandler_: function(e) {
      if (!this.isShown)
        return;
      var selected = this.container.selectedItem;
      switch(e.keyCode) {
        case 38: {  // Key up.
          do {
            selected = selected.previousSibling;
            if (!selected)
              selected = this.container.lastElementChild;
          } while (selected.iid < 0);
          this.container.selectItem(selected);
          break;
        }
        case 40: {  // Key down.
          do {
            selected = selected.nextSibling;
            if (!selected)
              selected = this.container.firstItem;
          } while (selected.iid < 0);
          this.container.selectItem(selected);
          break;
        }
        case 27: {  // Esc.
          this.isShown = false;
          break;
        }
        case 9: {  // Tab.
          this.isShown = false;
          break;
        }
        case 13: {  // Enter.
          this.titleButton.enterPressed = true;
          break;
        }
      };
    }
  };

  /**
   * Creates a new oobe screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var NetworkScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  NetworkScreen.register = function() {
    var screen = $('connect');
    NetworkScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  NetworkScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Dropdown element for networks selection.
     */
    dropdown_: null,

    /** @inheritDoc */
    decorate: function() {
      Oobe.setupSelect($('language-select'),
                       templateData.languageList,
                       'networkOnLanguageChanged');

      Oobe.setupSelect($('keyboard-select'),
                       templateData.inputMethodsList,
                       'networkOnInputMethodChanged');

      this.dropdown_ = $('networks-list');
      DropDown.decorate(this.dropdown_);
    },

    /**
     * Updates network list in dropdown.
     * @param {Array} items Dropdown items array.
     */
    updateNetworks: function(data) {
      this.dropdown_.setItems(data);
    },

    /**
     * Updates dropdown title and icon.
     * @param {string} title Text on dropdown.
     * @param {string} icon Icon in dataURL format.
     */
    updateNetworkTitle: function(title, icon) {
      this.dropdown_.setTitle(title, icon);
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return localStrings.getString('networkScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var continueButton = this.ownerDocument.createElement('button');
      continueButton.id = 'continue-button';
      continueButton.textContent = localStrings.getString('continueButton');
      continueButton.addEventListener('click', function(e) {
        chrome.send('networkOnExit', []);
      });
      buttons.push(continueButton);

      return buttons;
    }
  };

  /**
   * Updates networks list with the new data.
   * @param {!Object} data Networks list.
   */
  NetworkScreen.updateNetworks = function(data) {
    $('connect').updateNetworks(data);
  };

  /**
   * Updates network title, which is shown by the drop-down.
   * @param {string} title Title to be displayed.
   * @param {!Object} icon Icon to be displayed.
   */
  NetworkScreen.updateNetworkTitle = function(title, icon) {
    $('connect').updateNetworkTitle(title, icon);
  };

  /**
   * Shows the network error message.
   * @param {string} message Message to be shown.
   */
  NetworkScreen.showError = function(message) {
    var error = document.createElement('div');
    var messageDiv = document.createElement('div');
    messageDiv.className = 'error-message';
    messageDiv.textContent = message;
    error.appendChild(messageDiv);

    $('bubble').showContentForElement($('networks-list'), error);
  };

  /**
   * Hides the error notification bubble (if any).
   */
  NetworkScreen.clearErrors = function() {
    $('bubble').hide();
  };

  return {
    NetworkScreen: NetworkScreen
  };
});
