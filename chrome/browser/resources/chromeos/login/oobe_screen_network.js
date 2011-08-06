// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe network screen implementation.
 */

cr.define('oobe', function() {
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
      this.appendChild(this.createTitle_());

      // Create menu items container.
      var container = this.ownerDocument.createElement('div')
      container.classList.add('dropdown-container');
      this.appendChild(container);
      this.isShown = false;
    },

    /**
     * Returns true if dropdown menu is shown.
     * @type {bool} Whether menu element is shown.
     */
    get isShown() {
      return !this.lastElementChild.hidden;
    },

    /**
     * Sets dropdown menu visibility.
     * @param {bool} show New visibility state for dropdown menu.
     */
    set isShown(show) {
      this.lastElementChild.hidden = !show;
    },

    /**
     * Sets title and icon.
     * @param {string} title Text on dropdown.
     * @param {string} icon Icon in dataURL format.
     */
    setTitle: function(title, icon) {
      // TODO(nkostylev): Icon support for dropdown title.
      this.firstElementChild.textContent = title;
    },

    /**
     * Sets dropdown items.
     * @param {Array} items Dropdown items array.
     */
    setItems: function(items) {
      var container = this.lastElementChild;
      container.innerHTML = '';
      for (var i = 0; i < items.length; ++i) {
        var item = items[i];
        if ('sub' in item) {
          // Workaround for submenus, add items on top level.
          // TODO(altimofeev): support submenus.
          for (var j = 0; j < item.sub.length; ++j)
            this.createItem_(container, item.sub[j]);
          continue;
        }
        this.createItem_(container, item);
      }
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
          item.controller.isShown = !item.controller.isShown;
          if (item.iid >= 0)
            chrome.send('networkItemChosen', [item.iid]);
        });
        itemElement = wrapperDiv;
      }
      container.appendChild(itemElement);
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
      el.addEventListener('click', function f(e) {
        this.controller.isShown = !this.controller.isShown;
      });
      return el;
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

  NetworkScreen.updateNetworks = function(data) {
    $('connect').updateNetworks(data);
  };

  NetworkScreen.updateNetworkTitle = function(title, icon) {
    $('connect').updateNetworkTitle(title, icon);
  };

  return {
    NetworkScreen: NetworkScreen
  };
});
