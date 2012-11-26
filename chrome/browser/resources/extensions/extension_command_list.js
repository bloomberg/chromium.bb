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

  /**
   * Returns whether the passed in |keyCode| is a valid extension command
   * char or not. This is restricted to A-Z and 0-9 (ignoring modifiers) at
   * the moment.
   * @param {int} keyCode The keycode to consider.
   * @return {boolean} Returns whether the char is valid.
   */
  function validChar(keyCode) {
    return (keyCode >= 'A'.charCodeAt(0) && keyCode <= 'Z'.charCodeAt(0)) ||
           (keyCode >= '0'.charCodeAt(0) && keyCode <= '9'.charCodeAt(0));
  }

  /**
   * Convert a keystroke event to string form, while taking into account
   * (ignoring) invalid extension commands.
   * @param {Event} event The keyboard event to convert.
   * @return {string} The keystroke as a string.
   */
  function keystrokeToString(event) {
    var output = '';
    if (cr.isMac && event.metaKey)
      output = 'Command+';
    if (event.ctrlKey)
      output = 'Ctrl+';
    if (!event.ctrlKey && event.altKey)
      output += 'Alt+';
    if (event.shiftKey)
      output += 'Shift+';
    if (validChar(event.keyCode))
      output += String.fromCharCode('A'.charCodeAt(0) + event.keyCode - 65);
    return output;
  }

  ExtensionCommandList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * While capturing, this records the current (last) keyboard event generated
     * by the user. Will be |null| after capture and during capture when no
     * keyboard event has been generated.
     * @type: {keyboard event}.
     * @private
     */
    currentKeyEvent_: null,

    /**
     * While capturing, this keeps track of the previous selection so we can
     * revert back to if no valid assignment is made during capture.
     * @type: {string}.
     * @private
     */
    oldValue_: '',

    /**
     * While capturing, this keeps track of which element the user asked to
     * change.
     * @type: {HTMLElement}.
     * @private
     */
    capturingElement_: null,

    /** @override */
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
      node.id = this.createElementId_(
          'command', command.extension_id, command.command_name);

      var description = node.querySelector('.command-description');
      description.textContent = command.description;

      var shortcutNode = node.querySelector('.command-shortcut-text');
      shortcutNode.addEventListener('mouseup',
                                    this.startCapture_.bind(this));
      shortcutNode.addEventListener('blur', this.endCapture_.bind(this));
      shortcutNode.addEventListener('keydown',
                                    this.handleKeyDown_.bind(this));
      shortcutNode.addEventListener('keyup', this.handleKeyUp_.bind(this));
      if (!command.active) {
        shortcutNode.textContent =
            loadTimeData.getString('extensionCommandsInactive');

        var commandShortcut = node.querySelector('.command-shortcut');
        commandShortcut.classList.add('inactive-keybinding');
      } else {
        shortcutNode.textContent = command.keybinding;
      }

      var commandClear = node.querySelector('.command-clear');
      commandClear.id = this.createElementId_(
          'clear', command.extension_id, command.command_name);
      commandClear.title = loadTimeData.getString('extensionCommandsDelete');
      commandClear.addEventListener('click', this.handleClear_.bind(this));

      this.appendChild(node);
    },

    /**
     * Starts keystroke capture to determine which key to use for a particular
     * extension command.
     * @param {Event} event The keyboard event to consider.
     * @private
     */
    startCapture_: function(event) {
      if (this.capturingElement_)
        return;  // Already capturing.

      chrome.send('setShortcutHandlingSuspended', [true]);

      var shortcutNode = event.target;
      this.oldValue_ = shortcutNode.textContent;
      shortcutNode.textContent =
          loadTimeData.getString('extensionCommandsStartTyping');
      shortcutNode.parentElement.classList.add('capturing');

      var commandClear =
          shortcutNode.parentElement.querySelector('.command-clear');
      commandClear.hidden = true;

      this.capturingElement_ = event.target;
    },

    /**
     * Ends keystroke capture and either restores the old value or (if valid
     * value) sets the new value as active..
     * @param {Event} event The keyboard event to consider.
     * @private
     */
    endCapture_: function(event) {
      if (!this.capturingElement_)
        return;  // Not capturing.

      chrome.send('setShortcutHandlingSuspended', [false]);

      var shortcutNode = this.capturingElement_;
      var commandShortcut = shortcutNode.parentElement;

      commandShortcut.classList.remove('capturing');
      commandShortcut.classList.remove('contains-chars');

      // When the capture ends, the user may have not given a complete and valid
      // input (or even no input at all). Only a valid key event followed by a
      // valid key combination will cause a shortcut selection to be activated.
      // If no valid selection was made, howver, revert back to what the textbox
      // had before to indicate that the shortcut registration was cancelled.
      if (!this.currentKeyEvent_ || !validChar(this.currentKeyEvent_.keyCode))
        shortcutNode.textContent = this.oldValue_;

      var commandClear = commandShortcut.querySelector('.command-clear');
      if (this.oldValue_ == '') {
        commandShortcut.classList.remove('clearable');
        commandClear.hidden = true;
      } else {
        commandShortcut.classList.add('clearable');
        commandClear.hidden = false;
      }

      this.oldValue_ = '';
      this.capturingElement_ = null;
      this.currentKeyEvent_ = null;
    },

    /**
     * The KeyDown handler.
     * @param {Event} event The keyboard event to consider.
     * @private
     */
    handleKeyDown_: function(event) {
      if (!this.capturingElement_)
        this.startCapture_(event);

      this.handleKey_(event);
    },

    /**
     * The KeyUp handler.
     * @param {Event} event The keyboard event to consider.
     * @private
     */
    handleKeyUp_: function(event) {
      // We want to make it easy to change from Ctrl+Shift+ to just Ctrl+ by
      // releasing Shift, but we also don't want it to be easy to lose for
      // example Ctrl+Shift+F to Ctrl+ just because you didn't release Ctrl
      // as fast as the other two keys. Therefore, we process KeyUp until you
      // have a valid combination and then stop processing it (meaning that once
      // you have a valid combination, we won't change it until the next
      // KeyDown message arrives).
      if (!this.currentKeyEvent_ || !validChar(this.currentKeyEvent_.keyCode)) {
        if (!event.ctrlKey && !event.altKey) {
          // If neither Ctrl nor Alt is pressed then it is not a valid shortcut.
          // That means we're back at the starting point so we should restart
          // capture.
          this.endCapture_(event);
          this.startCapture_(event);
        } else {
          this.handleKey_(event);
        }
      }
    },

    /**
     * A general key handler (used for both KeyDown and KeyUp).
     * @param {Event} event The keyboard event to consider.
     * @private
     */
    handleKey_: function(event) {
      // While capturing, we prevent all events from bubbling, to prevent
      // shortcuts lacking the right modifier (F3 for example) from activating
      // and ending capture prematurely.
      event.preventDefault();
      event.stopPropagation();

      if (!event.ctrlKey && !event.altKey && (!cr.isMac || !event.metaKey))
        return;  // Ctrl or Alt is a must (or Cmd on Mac).

      var shortcutNode = this.capturingElement_;
      var keystroke = keystrokeToString(event);
      shortcutNode.textContent = keystroke;
      event.target.classList.add('contains-chars');

      if (validChar(event.keyCode)) {
        var node = event.target;
        while (node && !node.id)
          node = node.parentElement;

        this.oldValue_ = keystroke;  // Forget what the old value was.
        var parsed = this.parseElementId_('command', node.id);
        chrome.send('setExtensionCommandShortcut',
                    [parsed.extensionId, parsed.commandName, keystroke]);
        this.endCapture_(event);
      }

      this.currentKeyEvent_ = event;
    },

    /**
     * A handler for the delete command button.
     * @param {Event} event The mouse event to consider.
     * @private
     */
    handleClear_: function(event) {
      var parsed = this.parseElementId_('clear', event.target.id);
      chrome.send('setExtensionCommandShortcut',
          [parsed.extensionId, parsed.commandName, '']);
    },

    /**
     * A utility function to create a unique element id based on a namespace,
     * extension id and a command name.
     * @param {string} namespace   The namespace to prepend the id with.
     * @param {string} extensionId The extension ID to use in the id.
     * @param {string} commandName The command name to append the id with.
     * @private
     */
    createElementId_: function(namespace, extensionId, commandName) {
      return namespace + '-' + extensionId + '-' + commandName;
    },

    /**
     * A utility function to parse a unique element id based on a namespace,
     * extension id and a command name.
     * @param {string} namespace   The namespace to prepend the id with.
     * @param {string} id          The id to parse.
     * @return {object} The parsed id, as an object with two members:
     *                  extensionID and commandName.
     * @private
     */
    parseElementId_: function(namespace, id) {
      var kExtensionIdLength = 32;
      return {
        extensionId: id.substring(namespace.length + 1,
                                  namespace.length + 1 + kExtensionIdLength),
        commandName: id.substring(namespace.length + 1 + kExtensionIdLength + 1)
      };
    },
  };

  return {
    ExtensionCommandList: ExtensionCommandList
  };
});
