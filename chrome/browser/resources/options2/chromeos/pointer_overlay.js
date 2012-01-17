// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const SettingsDialog = options.SettingsDialog;

  /**
   * PointerOverlay class
   * Dialog that allows users to set pointer settings (touchpad/mouse).
   * @extends {SettingsDialog}
   */
  function PointerOverlay() {
    SettingsDialog.call(this, 'pointer-overlay',
                        templateData.pointerOverlayTitle, 'pointer-overlay',
                        $('pointer-overlay-confirm'),
                        $('pointer-overlay-cancel'));
  }

  cr.addSingletonGetter(PointerOverlay);

  PointerOverlay.prototype = {
    __proto__: SettingsDialog.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      SettingsDialog.prototype.initializePage.call(this);
    },
  };

  /**
   * Sets the visibility state of the touchpad group.
   */
  PointerOverlay.showTouchpadControls = function(show) {
    $('pointer-section-touchpad').hidden = !show;
  };

  /**
   * Sets the visibility state of the mouse group.
   */
  PointerOverlay.showMouseControls = function(show) {
    $('pointer-section-mouse').hidden = !show;
  };

  // Export
  return {
    PointerOverlay: PointerOverlay
  };
});
