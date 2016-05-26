// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  function StorageManager() {
    Page.call(this, 'storage',
              loadTimeData.getString('storageManagerPageTabTitle'),
              'storageManagerPage');
  }

  cr.addSingletonGetter(StorageManager);

  StorageManager.prototype = {
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('storage-confirm').onclick = function() {
        PageManager.closeOverlay();
      };
    }
  };

  return {
    StorageManager: StorageManager
  };
});
