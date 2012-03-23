// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  /**
   * Encapsulated handling of the keyboard overlay.
   * @constructor
   */
  function KeyboardOverlay() {
    options.SettingsDialog.call(this, 'keyboard-overlay',
        templateData.keyboardOverlayTitle,
        'keyboard-overlay',
        $('keyboard-confirm'), $('keyboard-cancel'));
  }

  cr.addSingletonGetter(KeyboardOverlay);

  KeyboardOverlay.prototype = {
    __proto__: options.SettingsDialog.prototype,
  };

  // Export
  return {
    KeyboardOverlay: KeyboardOverlay
  };
});
