// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  'use strict';

  /**
   * Creates a new list of extension commands.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.div}
   */
  var ExtensionCommandList = cr.ui.define('div');

  ExtensionCommandList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.textContent = '';

      // Iterate over the extension data and add each item to the list.
      this.data_.commands.forEach(this.createNodeForExtension_.bind(this));
    },

    /**
     * Synthesizes and initializes an HTML element for the extension command
     * metadata given in |extension|.
     * @param {Object} extension A dictionary of extension metadata.
     * @private
     */
    createNodeForExtension_: function(extension) {
      var template = $('template-collection-extension-commands').querySelector(
          '.extension-command-list-extension-item-wrapper');
      var node = template.cloneNode(true);

      var title = node.querySelector('.extension-title');
      title.textContent = extension.name;

      this.appendChild(node);

      // Iterate over the commands data within the extension and add each item
      // to the list.
      extension.commands.forEach(this.createNodeForCommand_.bind(this));
    },

    /**
     * Synthesizes and initializes an HTML element for the extension command
     * metadata given in |command|.
     * @param {Object} command A dictionary of extension command metadata.
     * @private
     */
    createNodeForCommand_: function(command) {
      var template = $('template-collection-extension-commands').querySelector(
          '.extension-command-list-command-item-wrapper');
      var node = template.cloneNode(true);

      var description = node.querySelector('.command-description');
      description.textContent = command.description;

      var shortcut = node.querySelector('.command-shortcut');
      if (!command.active) {
        shortcut.textContent =
            loadTimeData.getString('extensionCommandsInactive');
        shortcut.classList.add('inactive-keybinding');
      } else {
        shortcut.textContent = command.keybinding;
      }

      this.appendChild(node);
    },
  };

  return {
    ExtensionCommandList: ExtensionCommandList
  };
});
