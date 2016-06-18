// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  function StorageClearDriveCacheOverlay() {
    Page.call(this, 'storageClearDriveCache',
              loadTimeData.getString('storageClearDriveCachePageTabTitle'),
              'clear-drive-cache-overlay-page');
  }

  cr.addSingletonGetter(StorageClearDriveCacheOverlay);

  StorageClearDriveCacheOverlay.prototype = {
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('clear-drive-cache-overlay-delete-button').onclick = function(e) {
        chrome.send('clearDriveCache');
        PageManager.closeOverlay();
      };
      $('clear-drive-cache-overlay-cancel-button').onclick = function(e) {
        PageManager.closeOverlay();
      };
    },
  };

  return {
    StorageClearDriveCacheOverlay: StorageClearDriveCacheOverlay
  };
});
