// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /**
   * Encapsulated handling of the stylus overlay.
   * @constructor
   * @extends {options.SettingsDialog}
   */
  function StylusOverlay() {
    options.SettingsDialog.call(this, 'stylus-overlay',
        loadTimeData.getString('stylusOverlayTabTitle'),
        'stylus-overlay',
        assertInstanceof($('stylus-confirm'), HTMLButtonElement),
        assertInstanceof($('stylus-cancel'), HTMLButtonElement));
  }

  cr.addSingletonGetter(StylusOverlay);

  StylusOverlay.prototype = {
    __proto__: options.SettingsDialog.prototype,

    /** @override */
    initializePage: function() {
      options.SettingsDialog.prototype.initializePage.call(this);

      // Disable auto-launch pref when enable stylus tools pref is false.
      Preferences.getInstance().addEventListener('settings.enable_stylus_tools',
          function(e) {
            $('launch-palette-on-eject-input').disabled = !e.value.value;
          });
    },
  };

  // Export
  return {
    StylusOverlay: StylusOverlay
  };
});
