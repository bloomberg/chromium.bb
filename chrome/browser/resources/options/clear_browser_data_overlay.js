// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * ClearBrowserData class
 * Encapsulated handling of the 'Clear Browser Data' overlay page.
 * @class
 */
function ClearBrowserDataOverlay() {
  OptionsPage.call(this, 'clearBrowserDataOverlay',
                   templateData.clearBrowserDataTitle,
                   'clearBrowserDataOverlay');
}

cr.addSingletonGetter(ClearBrowserDataOverlay);

ClearBrowserDataOverlay.prototype = {
  // Inherit ClearBrowserDataOverlay from OptionsPage.
  __proto__: OptionsPage.prototype,

  /**
   * Initialize the page.
   */
  initializePage: function() {
    // Call base class implementation to starts preference initialization.
    OptionsPage.prototype.initializePage.call(this);

    // TODO(csilv): add any initialization here or delete method and/or class.
  }
};
