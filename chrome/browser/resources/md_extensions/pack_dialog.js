// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /** @interface */
  class PackDialogDelegate {
    /**
     * Opens a file browser for the user to select the root directory.
     * @return {Promise<string>} A promise that is resolved with the path the
     *     user selected.
     */
    choosePackRootDirectory() {}

    /**
     * Opens a file browser for the user to select the private key file.
     * @return {Promise<string>} A promise that is resolved with the path the
     *     user selected.
     */
    choosePrivateKeyPath() {}

    /**
     * Packs the extension into a .crx.
     * @param {string} rootPath
     * @param {string} keyPath
     * @param {number=} flag
     * @param {function(chrome.developerPrivate.PackDirectoryResponse)=}
     *     callback
     */
    packExtension(rootPath, keyPath, flag, callback) {}
  }

  const PackDialog = Polymer({
    is: 'extensions-pack-dialog',
    properties: {
      /** @type {extensions.PackDialogDelegate} */
      delegate: Object,

      /** @private */
      packDirectory_: {
        type: String,
        value: '',  // Initialized to trigger binding when attached.
      },

      /** @private */
      keyFile_: String,

      /** @private {?chrome.developerPrivate.PackDirectoryResponse} */
      lastResponse_: Object,
    },

    show: function() {
      this.$.dialog.showModal();
    },

    /** @private */
    onRootBrowse_: function() {
      this.delegate.choosePackRootDirectory().then(path => {
        if (path)
          this.set('packDirectory_', path);
      });
    },

    /** @private */
    onKeyBrowse_: function() {
      this.delegate.choosePrivateKeyPath().then(path => {
        if (path)
          this.set('keyFile_', path);
      });
    },

    /** @private */
    onCancelTap_: function() {
      this.$.dialog.cancel();
    },

    /** @private */
    onConfirmTap_: function() {
      this.delegate.packExtension(
          this.packDirectory_, this.keyFile_, 0,
          this.onPackResponse_.bind(this));
    },

    /**
     * @param {chrome.developerPrivate.PackDirectoryResponse} response the
     *    response from request to pack an extension.
     * @private
     */
    onPackResponse_: function(response) {
      if (response.status === chrome.developerPrivate.PackStatus.SUCCESS) {
        this.$.dialog.close();
      } else {
        this.set('lastResponse_', response);
      }
    },

    /**
     * The handler function when user chooses to 'Proceed Anyway' upon
     * receiving a waring.
     * @private
     */
    onWarningConfirmed_: function() {
      this.delegate.packExtension(
          this.lastResponse_.item_path, this.lastResponse_.pem_path,
          this.lastResponse_.override_flags, this.onPackResponse_.bind(this));
    },

    resetResponse_: function() {
      this.lastResponse_ = null;
    },
  });

  return {PackDialog: PackDialog, PackDialogDelegate: PackDialogDelegate};
});
