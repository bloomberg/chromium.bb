// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @param {?string} initPassphrase Initial passphrase for the first passphrase
 *     request or NULL if not available. In such case, call to getPassphrase()
 *     will invoke a dialog.
 */
unpacker.PassphraseManager = function(initPassphrase) {
  /**
   * @private {?string}
   */
  this.initPassphrase_ = initPassphrase;

  /**
   * @public {?string}
   */
  this.rememberedPassphrase = initPassphrase;
};

/**
 * Requests a passphrase from the user. If a passphrase was previously
 * remembered, then tries it first. Otherwise shows a passphrase dialog.
 * @return {!Promise<string>}
 */
unpacker.PassphraseManager.prototype.getPassphrase = function() {
  return new Promise(function(fulfill, reject) {
    // For the first passphrase request try the init passphrase (which may be
    // incorrect though, so do it only once).
    if (this.initPassphrase_ != null) {
      fulfill(this.initPassphrase_);
      this.initPassphrase_ = null;
      return;
    }

    // Ask user for a passphrase.
    chrome.app.window.create(
        '../html/passphrase.html',
        /** @type {!chrome.app.window.CreateWindowOptions} */ ({
          innerBounds: {width: 320, height: 160},
          alwaysOnTop: true,
          resizable: false,
          frame: 'none',
          hidden: true
        }),
        function(passphraseWindow) {
          var passphraseSucceeded = false;

          passphraseWindow.onClosed.addListener(function() {
            if (passphraseSucceeded)
              return;
            reject('FAILED');
          }.bind(this));

          passphraseWindow.contentWindow.onPassphraseSuccess = function(
                                                                   passphrase,
                                                                   remember) {
            passphraseSucceeded = true;
            this.rememberedPassphrase = remember ? passphrase : null;
            fulfill(passphrase);
          }.bind(this);
        }.bind(this));
  }.bind(this));
};
