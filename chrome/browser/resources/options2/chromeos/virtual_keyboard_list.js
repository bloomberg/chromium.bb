// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var List = cr.ui.List;
  /** @const */ var ListItem = cr.ui.ListItem;
  /** @const */ var VirtualKeyboardOptions = options.VirtualKeyboardOptions;

  /** @const */ var localStrings = new LocalStrings();

  /**
   * Creates a virtual keyboard list item.
   *
   * Accepts values in the form
   * { layout: 'us(dvorak)',
   *   layoutName: 'US Dvorak layout',
   *   preferredKeyboard: 'http://...',  [optional]
   *   supportedKeyboards: [
   *     { name: 'Simple Virtual Keyboard',
   *       isSystem: true,
   *       url: 'http://...' },
   *     { name: '3rd party Virtual Keyboard',
   *       isSystem: false,
   *       url: 'http://...' },
   *     ...,
   *   ]
   * }
   * @param {Object} entry A dictionary describing the virtual keyboards for a
   *     given layout.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function VirtualKeyboardListItem(entry) {
    var el = cr.doc.createElement('div');
    el.dataItem = entry;
    el.__proto__ = VirtualKeyboardListItem.prototype;
    el.decorate();
    return el;
  }

  VirtualKeyboardListItem.prototype = {
    __proto__: ListItem.prototype,

    buildWidget_: function(data, delegate) {
      // Layout name.
      var layoutNameElement = document.createElement('div');
      layoutNameElement.textContent = data.layoutName;
      layoutNameElement.className = 'virtual-keyboard-layout-column';
      this.appendChild(layoutNameElement);

      // Virtual keyboard selection.
      var keyboardElement = document.createElement('div');
      var selectElement = document.createElement('select');
      var defaultOptionElement = document.createElement('option');
      defaultOptionElement.selected = (data.preferredKeyboard == null);
      defaultOptionElement.textContent =
          localStrings.getString('defaultVirtualKeyboard');
      defaultOptionElement.value = -1;
      selectElement.appendChild(defaultOptionElement);

      for (var i = 0; i < data.supportedKeyboards.length; ++i) {
        var optionElement = document.createElement('option');
        optionElement.selected =
            (data.preferredKeyboard != null &&
             data.preferredKeyboard == data.supportedKeyboards[i].url);
        optionElement.textContent = data.supportedKeyboards[i].name;
        optionElement.value = i;
        selectElement.appendChild(optionElement);
      }

      selectElement.addEventListener('change', function(e) {
        var index = e.target.value;
        if (index == -1) {
          // The 'Default' menu item is selected. Delete the preference.
          delegate.clearPreference(data.layout);
        } else {
          delegate.setPreference(
              data.layout, data.supportedKeyboards[index].url);
        }
      });

      keyboardElement.appendChild(selectElement);
      keyboardElement.className = 'virtual-keyboard-keyboard-column';
      this.appendChild(keyboardElement);
    },

    /** @inheritDoc */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      var delegate = {
        clearPreference: function(layout) {
          // Call a C++ function in chrome/browser/ui/webui/options/chromeos/.
          chrome.send('clearVirtualKeyboardPreference', [layout]);
        },
        setPreference: function(layout, url) {
          chrome.send('setVirtualKeyboardPreference', [layout, url]);
        },
      };

      this.buildWidget_(this.dataItem, delegate);
    },
  };

  /**
   * Create a new virtual keyboard list.
   * @constructor
   * @extends {cr.ui.List}
   */
  var VirtualKeyboardsList = cr.ui.define('list');

  VirtualKeyboardsList.prototype = {
    __proto__: List.prototype,

    /** @inheritDoc */
    createItem: function(entry) {
      return new VirtualKeyboardListItem(entry);
    },

    /**
     * The length of the list.
     */
    get length() {
      return this.dataModel.length;
    },

    /**
     * Set the virtual keyboards displayed by this list.
     * See VirtualKeyboardListItem for an example of the format the list should
     * take.
     *
     * @param {Object} list A list of layouts with their registered virtual
     *     keyboards.
     */
    setVirtualKeyboardList: function(list) {
      this.dataModel = new ArrayDataModel(list);
    },
  };

  return {
    VirtualKeyboardListItem: VirtualKeyboardListItem,
    VirtualKeyboardsList: VirtualKeyboardsList,
  };
});
