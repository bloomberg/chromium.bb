// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * StopSyncingOverlay class
 * Encapsulated handling of the 'Stop Syncing This Account' overlay page.
 * @class
 */
function StopSyncingOverlay() {
  OptionsPage.call(this, 'stopSyncingOverlay',
                   templateData.stop_syncing_title,
                   'stopSyncingOverlay');
}

cr.addSingletonGetter(StopSyncingOverlay);

StopSyncingOverlay.prototype = {
  // Inherit StopSyncingOverlay from OptionsPage.
  __proto__: OptionsPage.prototype,

  /**
   * Initialize the page.
   */
  initializePage: function() {
    // Call base class implementation to starts preference initialization.
    OptionsPage.prototype.initializePage.call(this);

    $('stop-syncing-cancel').onclick = function(e) {
      OptionsPage.clearOverlays();
    }

    $('stop-syncing-confirm').onclick = function(e) {
      chrome.send('stopSyncing');
      OptionsPage.clearOverlays();
    }
  }
};
