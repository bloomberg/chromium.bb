// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * Encapsulated handling of the power overlay.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function PowerOverlay() {
    Page.call(this, 'power-overlay',
              loadTimeData.getString('powerOverlayTabTitle'),
              'power-overlay');
  }

  cr.addSingletonGetter(PowerOverlay);

  PowerOverlay.prototype = {
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('power-confirm').onclick =
          PageManager.closeOverlay.bind(PageManager);
    },

    setBatteryStatusText_: function(status) {
      $('battery-status-value').textContent = status;
    },
  };

  PowerOverlay.setBatteryStatusText = function(value) {
    PowerOverlay.getInstance().setBatteryStatusText_(value);
  };

  // Export
  return {
    PowerOverlay: PowerOverlay
  };
});
