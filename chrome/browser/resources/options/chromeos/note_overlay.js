// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /**
   * Encapsulated handling of the note overlay.
   * @constructor
   * @extends {options.SettingsDialog}
   */
  function NoteOverlay() {
    options.SettingsDialog.call(this, 'note-overlay',
         loadTimeData.getString('noteOverlayTabTitle'),
        'note-overlay',
        assertInstanceof($('note-confirm'), HTMLButtonElement),
        assertInstanceof($('note-cancel'), HTMLButtonElement));
  }

  cr.addSingletonGetter(NoteOverlay);

  NoteOverlay.prototype = {
    __proto__: options.SettingsDialog.prototype,

    /** @override */
    initializePage: function() {
      options.SettingsDialog.prototype.initializePage.call(this);
    },
  };

  // Export
  return {
    NoteOverlay: NoteOverlay
  };
});
