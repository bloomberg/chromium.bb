// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordManagerProxy is an abstraction over
 * chrome.passwordsPrivate which facilitates testing.
 */

/**
 * Interface for all callbacks to the password API.
 * @interface
 */
class PasswordManagerProxy {
  /**
   * Add an observer to the list of saved passwords.
   * @param {function(!Array<!PasswordManagerProxy.PasswordUiEntry>):void}
   *     listener
   */
  addSavedPasswordListChangedListener(listener) {}

  /**
   * Remove an observer from the list of saved passwords.
   * @param {function(!Array<!PasswordManagerProxy.PasswordUiEntry>):void}
   *     listener
   */
  removeSavedPasswordListChangedListener(listener) {}

  /**
   * Request the list of saved passwords.
   * TODO(https://crbug.com/919483): Return a promise instead of taking a
   * callback argument.
   * @param {function(!Array<!PasswordManagerProxy.PasswordUiEntry>):void}
   *     callback
   */
  getSavedPasswordList(callback) {}

  /**
   * Log that the Passwords page was accessed from the Chrome Settings WebUI.
   */
  recordPasswordsPageAccessInSettings() {}

  /**
   * Should remove the saved password and notify that the list has changed.
   * @param {number} id The id for the password entry being removed.
   *     No-op if |id| is not in the list.
   */
  removeSavedPassword(id) {}

  /**
   * Add an observer to the list of password exceptions.
   * @param {function(!Array<!PasswordManagerProxy.ExceptionEntry>):void}
   *     listener
   */
  addExceptionListChangedListener(listener) {}

  /**
   * Remove an observer from the list of password exceptions.
   * @param {function(!Array<!PasswordManagerProxy.ExceptionEntry>):void}
   *     listener
   */
  removeExceptionListChangedListener(listener) {}

  /**
   * Request the list of password exceptions.
   * TODO(https://crbug.com/919483): Return a promise instead of taking a
   * callback argument.
   * @param {function(!Array<!PasswordManagerProxy.ExceptionEntry>):void}
   *     callback
   */
  getExceptionList(callback) {}

  /**
   * Should remove the password exception and notify that the list has changed.
   * @param {number} id The id for the exception url entry being removed.
   *     No-op if |id| is not in the list.
   */
  removeException(id) {}

  /**
   * Should undo the last saved password or exception removal and notify that
   * the list has changed.
   */
  undoRemoveSavedPasswordOrException() {}

  /**
   * Gets the saved password for a given login pair.
   * @param {number} id The id for password entry that should be
   *     retrieved.
   * @return {!Promise<(string|undefined)>} Gets invoked with the password if
   *     it could be retrieved successfully.
   */
  getPlaintextPassword(id) {}

  /**
   * Triggers the dialogue for importing passwords.
   */
  importPasswords() {}

  /**
   * Triggers the dialogue for exporting passwords.
   * TODO(https://crbug.com/919483): Return a promise instead of taking a
   * callback argument.
   * @param {function():void} callback
   */
  exportPasswords(callback) {}

  /**
   * Queries the status of any ongoing export.
   * TODO(https://crbug.com/919483): Return a promise instead of taking a
   * callback argument.
   * @param {function(!PasswordManagerProxy.ExportProgressStatus):void}
   *     callback
   */
  requestExportProgressStatus(callback) {}

  /**
   * Add an observer to the export progress.
   * @param {function(!PasswordManagerProxy.PasswordExportProgress):void}
   *     listener
   */
  addPasswordsFileExportProgressListener(listener) {}

  /**
   * Remove an observer from the export progress.
   * @param {function(!PasswordManagerProxy.PasswordExportProgress):void}
   *     listener
   */
  removePasswordsFileExportProgressListener(listener) {}

  cancelExportPasswords() {}
}

/** @typedef {chrome.passwordsPrivate.PasswordUiEntry} */
PasswordManagerProxy.PasswordUiEntry;

/** @typedef {chrome.passwordsPrivate.LoginPair} */
PasswordManagerProxy.LoginPair;

/** @typedef {chrome.passwordsPrivate.ExceptionEntry} */
PasswordManagerProxy.ExceptionEntry;

/**
 * @typedef {{ entry: !PasswordManagerProxy.PasswordUiEntry, password: string }}
 */
PasswordManagerProxy.UiEntryWithPassword;

/** @typedef {chrome.passwordsPrivate.PasswordExportProgress} */
PasswordManagerProxy.PasswordExportProgress;

/** @typedef {chrome.passwordsPrivate.ExportProgressStatus} */
PasswordManagerProxy.ExportProgressStatus;

/**
 * Implementation that accesses the private API.
 * @implements {PasswordManagerProxy}
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
  recordPasswordsPageAccessInSettings() {
    chrome.passwordsPrivate.recordPasswordsPageAccessInSettings();
  }

  /** @override */
  removeSavedPassword(id) {
    chrome.passwordsPrivate.removeSavedPassword(id);
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
  removeException(id) {
    chrome.passwordsPrivate.removePasswordException(id);
  }

  /** @override */
  undoRemoveSavedPasswordOrException() {
    chrome.passwordsPrivate.undoRemoveSavedPasswordOrException();
  }

  /** @override */
  getPlaintextPassword(id) {
    return new Promise(resolve => {
      chrome.passwordsPrivate.requestPlaintextPassword(id, resolve);
    });
  }

  /** @override */
  importPasswords() {
    chrome.passwordsPrivate.importPasswords();
  }

  /** @override */
  exportPasswords(callback) {
    chrome.passwordsPrivate.exportPasswords(callback);
  }

  /** @override */
  requestExportProgressStatus(callback) {
    chrome.passwordsPrivate.requestExportProgressStatus(callback);
  }

  /** @override */
  addPasswordsFileExportProgressListener(listener) {
    chrome.passwordsPrivate.onPasswordsFileExportProgress.addListener(listener);
  }

  /** @override */
  removePasswordsFileExportProgressListener(listener) {
    chrome.passwordsPrivate.onPasswordsFileExportProgress.removeListener(
        listener);
  }

  /** @override */
  cancelExportPasswords() {
    chrome.passwordsPrivate.cancelExportPasswords();
  }
}

cr.addSingletonGetter(PasswordManagerImpl);
