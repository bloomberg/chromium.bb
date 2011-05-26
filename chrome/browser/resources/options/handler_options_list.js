// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;
  const HandlerOptions = options.HandlerOptions;

  const localStrings = new LocalStrings();

  /**
   * Creates a new protocol / content handler list item.
   *
   * Accepts values in the form
   * { protocol: 'mailto',
   *   handlers: [
   *     ['http://www.thesite.com/%s', 'The title of the protocol'],
   *     ...,
   *   ],
   * }
   * @param {Object} entry A dictionary describing the handlers for a given
   *     protocol.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function HandlerListItem(entry) {
    var el = cr.doc.createElement('div');
    el.dataItem = entry;
    el.__proto__ = HandlerListItem.prototype;
    el.decorate();

    return el;
  }

  HandlerListItem.prototype = {
    __proto__: ListItem.prototype,

    buildWidget_: function(data, delegate) {
      // Protocol.
      var protocolElement = document.createElement('div');
      protocolElement.innerText = data.protocol;
      protocolElement.className = 'handlers-type-column';
      this.appendChild(protocolElement);

      // Handler selection.
      var handlerElement = document.createElement('div');
      var selectElement = document.createElement('select');
      var defaultOptionElement = document.createElement('option');
      defaultOptionElement.selected = data.default_handler == -1;
      defaultOptionElement.innerText =
          localStrings.getString('handlers_none_handler');
      defaultOptionElement.value = -1;
      selectElement.appendChild(defaultOptionElement);

      for (var i = 0; i < data.handlers.length; ++i) {
        var optionElement = document.createElement('option');
        optionElement.selected = i == data.default_handler;
        optionElement.innerText = data.handlers[i][1];
        optionElement.value = i;
        selectElement.appendChild(optionElement);
      }

      selectElement.addEventListener('change', function (e) {
        var index = e.target.value;
        if (index == -1) {
          this.classList.add('none');
          delegate.clearDefault(data.protocol);
        } else {
          handlerElement.classList.remove('none');
          delegate.setDefault([data.protocol].concat(data.handlers[index]));
        }
      });
      handlerElement.appendChild(selectElement);
      handlerElement.className = 'handlers-site-column';
      if (data.default_handler == -1)
        this.classList.add('none');
      this.appendChild(handlerElement);

      // Remove link.
      var removeElement = document.createElement('div');
      removeElement.innerText =
          localStrings.getString('handlers_remove_link');
      removeElement.addEventListener('click', function (e) {
        var value = selectElement ? selectElement.value : 0;
        delegate.removeHandler(value,
            [data.protocol].concat(data.handlers[value]));
      });
      removeElement.className = 'handlers-remove-column handlers-remove-link';
      this.appendChild(removeElement);
    },

    /** @inheritDoc */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      var self = this;
      var delegate = {
        removeHandler: function(index, handler) {
          chrome.send('removeHandler', [handler]);
        },
        setDefault: function(handler) {
          chrome.send('setDefault', [handler]);
        },
        clearDefault: function(protocol) {
          chrome.send('clearDefault', [protocol]);
        },
      };

      this.buildWidget_(this.dataItem, delegate);
    },
  };

  /**
   * Create a new passwords list.
   * @constructor
   * @extends {cr.ui.List}
   */
  var HandlersList = cr.ui.define('list');

  HandlersList.prototype = {
    __proto__: List.prototype,

    /** @inheritDoc */
    createItem: function(entry) {
      return new HandlerListItem(entry);
    },

    /**
     * The length of the list.
     */
    get length() {
      return this.dataModel.length;
    },

    /**
     * Set the protocol handlers displayed by this list.
     * See HandlerListItem for an example of the format the list should take.
     *
     * @param {Object} list A list of protocols with their registered handlers.
     */
    setHandlers: function(list) {
      this.dataModel = new ArrayDataModel(list);
    },
  };

  return {
    HandlerListItem: HandlerListItem,
    HandlersList: HandlersList,
  };
});
