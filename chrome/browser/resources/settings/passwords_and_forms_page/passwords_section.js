// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-section' is the collapsible section containing
 * the list of saved passwords as well as the list of sites that will never
 * save any passwords.
 */

/**
 * Interface for all callbacks to the password API.
 * @interface
 */
class PasswordManager {
  /**
   * Add an observer to the list of saved passwords.
   * @param {function(!Array<!PasswordManager.PasswordUiEntry>):void} listener
   */
  addSavedPasswordListChangedListener(listener) {}

  /**
   * Remove an observer from the list of saved passwords.
   * @param {function(!Array<!PasswordManager.PasswordUiEntry>):void} listener
   */
  removeSavedPasswordListChangedListener(listener) {}

  /**
   * Request the list of saved passwords.
   * @param {function(!Array<!PasswordManager.PasswordUiEntry>):void} callback
   */
  getSavedPasswordList(callback) {}

  /**
   * Should remove the saved password and notify that the list has changed.
   * @param {number} index The index for the password entry being removed.
   *     No-op if |index| is not in the list.
   */
  removeSavedPassword(index) {}

  /**
   * Add an observer to the list of password exceptions.
   * @param {function(!Array<!PasswordManager.ExceptionEntry>):void} listener
   */
  addExceptionListChangedListener(listener) {}

  /**
   * Remove an observer from the list of password exceptions.
   * @param {function(!Array<!PasswordManager.ExceptionEntry>):void} listener
   */
  removeExceptionListChangedListener(listener) {}

  /**
   * Request the list of password exceptions.
   * @param {function(!Array<!PasswordManager.ExceptionEntry>):void} callback
   */
  getExceptionList(callback) {}

  /**
   * Should remove the password exception and notify that the list has changed.
   * @param {number} index The index for the exception url entry being removed.
   *     No-op if |index| is not in the list.
   */
  removeException(index) {}

  /**
   * Should undo the last saved password or exception removal and notify that
   * the list has changed.
   */
  undoRemoveSavedPasswordOrException() {}

  /**
   * Gets the saved password for a given login pair.
   * @param {number} index The index for password entry that should be
   *     retrieved. No-op if |index| is not in the list.
   * @param {function(!PasswordManager.PlaintextPasswordEvent):void} callback
   */
  getPlaintextPassword(index, callback) {}

  /**
   * Triggers the dialogue for importing passwords.
   */
  importPasswords() {}

  /**
   * Triggers the dialogue for exporting passwords.
   */
  exportPasswords() {}
}

/** @typedef {chrome.passwordsPrivate.PasswordUiEntry} */
PasswordManager.PasswordUiEntry;

/** @typedef {chrome.passwordsPrivate.LoginPair} */
PasswordManager.LoginPair;

/** @typedef {chrome.passwordsPrivate.ExceptionEntry} */
PasswordManager.ExceptionEntry;

/** @typedef {chrome.passwordsPrivate.PlaintextPasswordEventParameters} */
PasswordManager.PlaintextPasswordEvent;

/**
 * Implementation that accesses the private API.
 * @implements {PasswordManager}
 */
class PasswordManagerImpl {
  /** @override */
  addSavedPasswordListChangedListener(listener) {
    chrome.passwordsPrivate.onSavedPasswordsListChanged.addListener(listener);
  }

  /** @override */
  removeSavedPasswordListChangedListener(listener) {
    chrome.passwordsPrivate.onSavedPasswordsListChanged.removeListener(
        listener);
  }

  /** @override */
  getSavedPasswordList(callback) {
    chrome.passwordsPrivate.getSavedPasswordList(callback);
  }

  /** @override */
  removeSavedPassword(index) {
    chrome.passwordsPrivate.removeSavedPassword(index);
  }

  /** @override */
  addExceptionListChangedListener(listener) {
    chrome.passwordsPrivate.onPasswordExceptionsListChanged.addListener(
        listener);
  }

  /** @override */
  removeExceptionListChangedListener(listener) {
    chrome.passwordsPrivate.onPasswordExceptionsListChanged.removeListener(
        listener);
  }

  /** @override */
  getExceptionList(callback) {
    chrome.passwordsPrivate.getPasswordExceptionList(callback);
  }

  /** @override */
  removeException(index) {
    chrome.passwordsPrivate.removePasswordException(index);
  }

  /** @override */
  undoRemoveSavedPasswordOrException() {
    chrome.passwordsPrivate.undoRemoveSavedPasswordOrException();
  }

  /** @override */
  getPlaintextPassword(index, callback) {
    var listener = function(reply) {
      // Only handle the reply for our loginPair request.
      if (reply.index == index) {
        chrome.passwordsPrivate.onPlaintextPasswordRetrieved.removeListener(
            listener);
        callback(reply);
      }
    };
    chrome.passwordsPrivate.onPlaintextPasswordRetrieved.addListener(listener);
    chrome.passwordsPrivate.requestPlaintextPassword(index);
  }

  /** @override */
  importPasswords() {
    chrome.passwordsPrivate.importPasswords();
  }

  /** @override */
  exportPasswords() {
    chrome.passwordsPrivate.exportPasswords();
  }
}

cr.addSingletonGetter(PasswordManagerImpl);

/** @typedef {!{model: !{item: !chrome.passwordsPrivate.PasswordUiEntry}}} */
var PasswordUiEntryEvent;

/** @typedef {!{model: !{item: !chrome.passwordsPrivate.ExceptionEntry}}} */
var ExceptionEntryEntryEvent;

(function() {
'use strict';

Polymer({
  is: 'passwords-section',

  behaviors: [
    I18nBehavior,
    Polymer.IronA11yKeysBehavior,
    settings.GlobalScrollTargetBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * An array of passwords to display.
     * @type {!Array<!PasswordManager.PasswordUiEntry>}
     */
    savedPasswords: Array,

    /**
     * An array of sites to display.
     * @type {!Array<!PasswordManager.ExceptionEntry>}
     */
    passwordExceptions: Array,

    /**
     * Duration of the undo toast in ms
     * @private
     */
    toastDuration_: {
      type: Number,
      value: 5000,
    },

    /** @override */
    subpageRoute: {
      type: Object,
      value: settings.routes.MANAGE_PASSWORDS,
    },

    /**
     * The model for any password related action menus or dialogs.
     * @private {?chrome.passwordsPrivate.PasswordUiEntry}
     */
    activePassword: Object,


    /** The target of the key bindings defined below. */
    keyEventTarget: {
      type: Object,
      value: () => document,
    },

    /** @private */
    showImportExportPasswords_: {
      type: Boolean,
      value: function() {
        return loadTimeData.valueExists('showImportExportPasswords') &&
            loadTimeData.getBoolean('showImportExportPasswords');
      }
    },

    /** @private */
    showPasswordEditDialog_: Boolean,

    /** Filter on the saved passwords and exceptions. */
    filter: {
      type: String,
      value: '',
    },

    /** @private {!PasswordManager.PasswordUiEntry} */
    lastFocused_: Object,
  },

  listeners: {
    'show-password': 'showPassword_',
    'password-menu-tap': 'onPasswordMenuTap_',
  },

  keyBindings: {
    // <if expr="is_macosx">
    'meta+z': 'onUndoKeyBinding_',
    // </if>
    // <if expr="not is_macosx">
    'ctrl+z': 'onUndoKeyBinding_',
    // </if>
  },

  /**
   * The element to return focus to, when the currently active dialog is
   * closed.
   * @private {?HTMLElement}
   */
  activeDialogAnchor_: null,

  /**
   * @type {PasswordManager}
   * @private
   */
  passwordManager_: null,

  /**
   * @type {?function(!Array<PasswordManager.PasswordUiEntry>):void}
   * @private
   */
  setSavedPasswordsListener_: null,

  /**
   * @type {?function(!Array<PasswordManager.ExceptionEntry>):void}
   * @private
   */
  setPasswordExceptionsListener_: null,

  /** @override */
  attached: function() {
    // Create listener functions.
    var setSavedPasswordsListener = list => {
      this.savedPasswords = list;
    };

    var setPasswordExceptionsListener = list => {
      this.passwordExceptions = list;
    };

    this.setSavedPasswordsListener_ = setSavedPasswordsListener;
    this.setPasswordExceptionsListener_ = setPasswordExceptionsListener;

    // Set the manager. These can be overridden by tests.
    this.passwordManager_ = PasswordManagerImpl.getInstance();

    // Request initial data.
    this.passwordManager_.getSavedPasswordList(setSavedPasswordsListener);
    this.passwordManager_.getExceptionList(setPasswordExceptionsListener);

    // Listen for changes.
    this.passwordManager_.addSavedPasswordListChangedListener(
        setSavedPasswordsListener);
    this.passwordManager_.addExceptionListChangedListener(
        setPasswordExceptionsListener);

    Polymer.RenderStatus.afterNextRender(this, function() {
      Polymer.IronA11yAnnouncer.requestAvailability();
    });
  },

  /** @override */
  detached: function() {
    this.passwordManager_.removeSavedPasswordListChangedListener(
        /** @type {function(!Array<PasswordManager.PasswordUiEntry>):void} */ (
            this.setSavedPasswordsListener_));
    this.passwordManager_.removeExceptionListChangedListener(
        /** @type {function(!Array<PasswordManager.ExceptionEntry>):void} */ (
            this.setPasswordExceptionsListener_));
  },

  /**
   * Shows the edit password dialog.
   * @param {!Event} e
   * @private
   */
  onMenuEditPasswordTap_: function(e) {
    e.preventDefault();
    /** @type {CrActionMenuElement} */ (this.$.menu).close();
    this.showPasswordEditDialog_ = true;
  },

  /** @private */
  onPasswordEditDialogClosed_: function() {
    this.showPasswordEditDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.activeDialogAnchor_));
    this.activeDialogAnchor_ = null;
  },

  /**
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} savedPasswords
   * @param {string} filter
   * @return {!Array<!chrome.passwordsPrivate.PasswordUiEntry>}
   * @private
   */
  getFilteredPasswords_: function(savedPasswords, filter) {
    if (!filter)
      return savedPasswords;

    return savedPasswords.filter(p => {
      return [p.loginPair.urls.shown, p.loginPair.username].some(
          term => term.toLowerCase().includes(filter.toLowerCase()));
    });
  },

  /**
   * @param {string} filter
   * @return {function(!chrome.passwordsPrivate.ExceptionEntry): boolean}
   * @private
   */
  passwordExceptionFilter_: function(filter) {
    return function(exception) {
      return exception.urls.shown.toLowerCase().includes(filter.toLowerCase());
    };
  },

  /**
   * Fires an event that should delete the saved password.
   * @private
   */
  onMenuRemovePasswordTap_: function() {
    this.passwordManager_.removeSavedPassword(this.activePassword.index);
    this.fire('iron-announce', {text: this.$.undoLabel.textContent});
    this.$.undoToast.show();
    /** @type {CrActionMenuElement} */ (this.$.menu).close();
  },

  onUndoKeyBinding_: function(event) {
    this.passwordManager_.undoRemoveSavedPasswordOrException();
    this.$.undoToast.hide();
    // Preventing the default is necessary to not conflict with a possible
    // search action.
    event.preventDefault();
  },

  onUndoButtonTap_: function() {
    this.passwordManager_.undoRemoveSavedPasswordOrException();
    this.$.undoToast.hide();
  },
  /**
   * Fires an event that should delete the password exception.
   * @param {!ExceptionEntryEntryEvent} e The polymer event.
   * @private
   */
  onRemoveExceptionButtonTap_: function(e) {
    this.passwordManager_.removeException(e.model.item.index);
  },

  /**
   * Opens the password action menu.
   * @param {!Event} event
   * @private
   */
  onPasswordMenuTap_: function(event) {
    var menu = /** @type {!CrActionMenuElement} */ (this.$.menu);
    var target = /** @type {!HTMLElement} */ (event.detail.target);

    this.activePassword =
        /** @type {!chrome.passwordsPrivate.PasswordUiEntry} */ (
            event.detail.item);
    menu.showAt(target);
    this.activeDialogAnchor_ = target;
  },

  undoRemoveSavedPasswordOrException_: function(event) {
    this.passwordManager_.undoRemoveSavedPasswordOrException();
  },

  /**
   * Fires an event that should trigger the password import process.
   * @private
   */
  onImportTap_: function() {
    this.passwordManager_.importPasswords();
  },

  /**
   * Fires an event that should trigger the password export process.
   * @private
   */
  onExportTap_: function() {
    this.passwordManager_.exportPasswords();
  },

  /**
   * Returns true if the list exists and has items.
   * @param {Array<Object>} list
   * @return {boolean}
   * @private
   */
  hasSome_: function(list) {
    return !!(list && list.length);
  },

  /**
   * Listens for the show-password event, and calls the private API.
   * @param {!Event} event
   * @private
   */
  showPassword_: function(event) {
    this.passwordManager_.getPlaintextPassword(
        /** @type {!number} */ (event.detail.item.index), item => {
          event.detail.password = item.plaintextPassword;
        });
  },

  /**
   * @private
   * @param {boolean} toggleValue
   * @return {string}
   */
  getOnOffLabel_: function(toggleValue) {
    return toggleValue ? this.i18n('toggleOn') : this.i18n('toggleOff');
  }
});
})();
