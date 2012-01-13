// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

  /**
   * Encapsulated handling of the keyboard overlay.
   * @constructor
   */
  function KeyboardOverlay() {
    OptionsPage.call(this, 'keyboard-overlay',
                     templateData.keyboardOverlayTitle, 'keyboard-overlay');
  }

  cr.addSingletonGetter(KeyboardOverlay);

  KeyboardOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    /** @inheritDoc */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('keyboard-overlay-dismiss-button').onclick = function() {
        OptionsPage.closeOverlay();
      };
    }
  };

  // Export
  return {
    KeyboardOverlay: KeyboardOverlay
  };
});
