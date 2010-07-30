// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /**
   * AddStartupPageOverlay class
   * Encapsulated handling of the 'Add Page' overlay page.
   * @class
   */
  function AddStartupPageOverlay() {
    OptionsPage.call(this, 'addStartupPageOverlay',
                     templateData.addStartupPageTitle,
                     'addStartupPageOverlay');
  }

  cr.addSingletonGetter(AddStartupPageOverlay);

  AddStartupPageOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initializes the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('addStartupPageCancelButton').onclick = function(e) {
        OptionsPage.clearOverlays();
      };
    }
  };

  // Export
  return {
    AddStartupPageOverlay: AddStartupPageOverlay
  };

});

