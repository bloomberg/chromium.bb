// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline message screen implementation.
 */

cr.define('login', function() {
  /**
   * Creates a new TPM error message screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var TPMErrorMessageScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  TPMErrorMessageScreen.register = function() {
    var screen = $('tpm-error-message');
    TPMErrorMessageScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  TPMErrorMessageScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
    }
  };

  /**
   * Show TPM screen.
   */
  TPMErrorMessageScreen.show = function() {
    Oobe.showScreen({id: SCREEN_TPM_ERROR});
  };

  return {
    TPMErrorMessageScreen: TPMErrorMessageScreen
  };
});
