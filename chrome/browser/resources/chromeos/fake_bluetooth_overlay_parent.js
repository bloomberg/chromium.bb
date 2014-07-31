// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * Encapsulated a fake parent page for bluetooth overlay page used by Web UI.
   * @constructor
   */
  function FakeBluetoothOverlayParent(model) {
    Page.call(this, 'bluetooth', '', 'bluetooth-container');
  }

  cr.addSingletonGetter(FakeBluetoothOverlayParent);

  FakeBluetoothOverlayParent.prototype = {
    // Inherit FakeBluetoothOverlayParent from Page.
    __proto__: Page.prototype,
  };

  // Export
  return {
    FakeBluetoothOverlayParent: FakeBluetoothOverlayParent
  };
});
