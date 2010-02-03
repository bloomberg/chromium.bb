// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A command is an abstraction of an action a user can do in the
 * UI.
 *
 * When the focus changes in the document for each command a canExecute event
 * is dispatched on the active element. By listening to this event you can
 * enable and disable the command by setting the event.canExecute property.
 *
 * When a command is executed a command event is dispatched on the active
 * element. Note that you should stop the propagation after you have handled the
 * command if there might be other command listeners higher up in the DOM tree.
 */

cr.define('cr.ui', function() {

  /**
   * Creates a new command element.
   * @constructor
   * @extends {HTMLElement}
   */
  var Command = cr.ui.define('command');

  Command.prototype = {
    __proto__: HTMLElement.prototype,

    /**
     * Initializes the command.
     */
    decorate: function() {
      CommandManager.init(this.ownerDocument);
    },

    /**
     * Executes the command. This dispatches a command event on the active
     * element. If the command is {@code disabled} this does nothing.
     */
    execute: function() {
      if (this.disabled)
        return;
      var doc = this.ownerDocument;
      if (doc.activeElement) {
        var e = new cr.Event('command', true, false);
        e.command = this;
        doc.activeElement.dispatchEvent(e);
      }
    },

    /**
     * Call this when there have been changes that might change whether the
     * command can be executed or not.
     */
    canExecuteChange: function() {
      dispatchCanExecuteEvent(this, this.ownerDocument.activeElement);
    },

    /**
     * The keyboard shortcut that triggers the command. This is a string
     * consisting of a keyIdentifier (as reported by WebKit in keydown) as
     * well as optional key modifiers joinded with a '-'.
     * For example:
     *   "F1"
     *   "U+0008-Meta" for Apple command backspace.
     *   "U+0041-Ctrl" for Control A
     *
     * @type {string}
     */
    shortcut_: null,
    get shortcut() {
      return this.shortcut_;
    },
    set shortcut(shortcut) {
      var oldShortcut = this.shortcut_;
      if (shortcut !== oldShortcut) {
        this.shortcut_ = shortcut;

        // TODO(arv): Multiple shortcuts?

        var mods = {};
        var ident = '';
        shortcut.split('-').forEach(function(part) {
          var partLc = part.toLowerCase();
          switch (partLc) {
            case 'alt':
            case 'ctrl':
            case 'meta':
            case 'shift':
              mods[partLc + 'Key'] = true;
              break;
            default:
              if (ident)
                throw Error('Multiple keyboard shortcuts are not supported');
              ident = part;
          }

          this.keyIdentifier_ = ident;
          this.keyModifiers_ = mods;
        }, this);

        cr.dispatchPropertyChange(this, 'shortcut', this.shortcut_,
                                  oldShortcut);
      }
    },

    /**
     * Whether the event object matches the shortcut for this command.
     * @param {!Event} e The key event object.
     * @return {boolean} Whether it matched or not.
     */
    matchesEvent: function(e) {
      if (!this.keyIdentifier_)
        return false;

      if (e.keyIdentifier == this.keyIdentifier_) {
        // All keyboard modifiers needs to match.
        var mods = this.keyModifiers_;
        return ['altKey', 'ctrlKey', 'metaKey', 'shiftKey'].every(function(k) {
          return e[k] == !!mods[k];
        });
      }
      return false;
    }
  };

  /**
   * The label of the command.
   * @type {string}
   */
  cr.defineProperty(Command, 'label', cr.PropertyKind.ATTR);

  /**
   * Whether the command is disabled or not.
   * @type {boolean}
   */
  cr.defineProperty(Command, 'disabled', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the command is hidden or not.
   * @type {boolean}
   */
  cr.defineProperty(Command, 'hidden', cr.PropertyKind.BOOL_ATTR);

  /**
   * Dispatches a canExecute event on the target.
   * @param {cr.ui.Command} command The command that we are testing for.
   * @param {Element} target The target element to dispatch the event on.
   */
  function dispatchCanExecuteEvent(command, target) {
    var e = new CanExecuteEvent(command, true)
    target.dispatchEvent(e);
    command.disabled = !e.canExecute;
  }

  /**
   * The command managers for different documents.
   */
  var commandManagers = {};

  /**
   * Keeps track of the focused element and updates the commands when the focus
   * changes.
   * @param {!Document} doc The document that we are managing the commands for.
   * @constructor
   */
  function CommandManager(doc) {
    doc.addEventListener('focus', cr.bind(this.handleFocus_, this), true);
    doc.addEventListener('keydown', cr.bind(this.handleKeyDown_, this), true);
  }

  /**
   * Initializes a command manager for the document as needed.
   * @param {!Document} doc The document to manage the commands for.
   */
  CommandManager.init = function(doc) {
    var uid = cr.getUid(doc);
    if (!(uid in commandManagers)) {
      commandManagers[uid] = new CommandManager(doc);
    }
  },

  CommandManager.prototype = {

    /**
     * Handles focus changes on the document.
     * @param {Event} e The focus event object.
     * @private
     */
    handleFocus_: function(e) {
      var target = e.target;
      var commands = Array.prototype.slice.call(
          target.ownerDocument.querySelectorAll('command'));

      commands.forEach(function(command) {
        dispatchCanExecuteEvent(command, target);
      });
    },

    /**
     * Handles the keydown event and routes it to the right command.
     * @param {!Event} e The keydown event.
     */
    handleKeyDown_: function(e) {
      var target = e.target;
      var commands = Array.prototype.slice.call(
          target.ownerDocument.querySelectorAll('command'));

      for (var i = 0, command; command = commands[i]; i++) {
        if (!command.disabled && command.matchesEvent(e)) {
          e.preventDefault();
          // We do not want any other element to handle this.
          e.stopPropagation();

          command.execute();
          return;
        }
      }
    }
  };

  /**
   * The event type used for canExecute events.
   * @param {!cr.ui.Command} command The command that we are evaluating.
   * @extends {Event}
   */
  function CanExecuteEvent(command) {
    var e = command.ownerDocument.createEvent('Event');
    e.initEvent('canExecute', true, false);
    e.__proto__ = CanExecuteEvent.prototype;
    e.command = command;
    return e;
  }

  CanExecuteEvent.prototype = {
    __proto__: Event.prototype,

    /**
     * The current command
     * @type {cr.ui.Command}
     */
    command: null,

    /**
     * Whether the target can execute the command. Setting this also stops the
     * propagation.
     * @type {boolean}
     */
    canExecute_: false,
    get canExecute() {
      return this.canExecute_;
    },
    set canExecute(canExecute) {
      this.canExecute_ = canExecute;
      this.stopPropagation();
    }
  };

  // Export
  return {
    Command: Command,
    CanExecuteEvent: CanExecuteEvent
  };
});
