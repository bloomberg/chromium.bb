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
      node.id = 'command-' + command.extension_id + '-' + command.command_name;

      var description = node.querySelector('.command-description');
      description.textContent = command.description;

      var command_shortcut = node.querySelector('.command-shortcut');
      command_shortcut.addEventListener('mouseup',
                                        this.startCapture_.bind(this));
      command_shortcut.addEventListener('blur', this.endCapture_.bind(this));
      command_shortcut.addEventListener('keydown',
                                        this.handleKeyDown_.bind(this));
      command_shortcut.addEventListener('keyup', this.handleKeyUp_.bind(this));

      if (!command.active) {
        command_shortcut.textContent =
            loadTimeData.getString('extensionCommandsInactive');
        command_shortcut.classList.add('inactive-keybinding');
      } else {
        command_shortcut.textContent = command.keybinding;
      }

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

      this.oldValue_ = event.target.textContent;
      event.target.textContent =
          loadTimeData.getString('extensionCommandsStartTyping');
      event.target.classList.add('capturing');

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

      this.capturingElement_.classList.remove('capturing');
      this.capturingElement_.classList.remove('contains-chars');
      if (!this.currentKeyEvent_ || !validChar(this.currentKeyEvent_.keyCode))
        this.capturingElement_.textContent = this.oldValue_;

      if (this.oldValue_ == '')
        this.capturingElement_.classList.remove('clearable');
      else
        this.capturingElement_.classList.add('clearable');

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

      if (!event.ctrlKey && !event.altKey)
        return;  // Ctrl or Alt is a must.

      var keystroke = keystrokeToString(event);
      event.target.textContent = keystroke;
      event.target.classList.add('contains-chars');

      if (validChar(event.keyCode)) {
        var node = event.target;
        while (node && !node.id)
          node = node.parentElement;
        // The id is set to namespacePrefix-extensionId-commandName. We don't
        // care about the namespacePrefix (but we care about the other two).
        var id = node.id.substring(8, 40);
        var command_name = node.id.substring(41);

        this.oldValue_ = keystroke;  // Forget what the old value was.
        chrome.send('setExtensionCommandShortcut',
                    [id, command_name, keystroke]);
        this.endCapture_(event);
      }

      this.currentKeyEvent_ = event;
    },
  };

  return {
    ExtensionCommandList: ExtensionCommandList
  };
});
