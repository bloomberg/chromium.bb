// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const ListSelectionModel = cr.ui.ListSelectionModel;

  /**
   * Encapsulated handling of startup page management page.
   * @constructor
   */
  function StartupPageManager() {
    this.activeNavTab = null;
    OptionsPage.call(this, 'startupPages',
                     templateData.StartupPageManagerPage,
                     'startupPageManagerPage');
  }

  cr.addSingletonGetter(StartupPageManager);

  StartupPageManager.prototype = {
    __proto__: OptionsPage.prototype,
    list_: null,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var list = $('startupPagesFullList');
      options.browser_options.StartupPageList.decorate(list);
      list.autoExpands = true;
      list.selectionModel = new ListSelectionModel;

      // Wire up controls.
      $('startupAddButton').onclick = function(event) {
        OptionsPage.showOverlay('addStartupPageOverlay');
      };

      // Remove Windows-style accelerators from button labels.
      // TODO(stuartmorgan): Remove this once the strings are updated.
      $('startupAddButton').textContent =
          localStrings.getStringWithoutAccelerator('startupAddButton');
    },

    /**
     * Updates the startup pages list with the given entries.
     * @param {Array} pages List of startup pages.
     * @private
     */
    updateStartupPages_: function(pages) {
      $('startupPagesFullList').dataModel = new ArrayDataModel(pages);
    },

    /**
     * Adds the given startup page at the current selection point.
     * @private
     */
    addStartupPage_: function(url) {
      var firstSelection =
          $('startupPagesFullList').selectionModel.selectedIndex;
      chrome.send('addStartupPage', [url, String(firstSelection)]);
    },
  };

  StartupPageManager.updateStartupPages = function(pages) {
    StartupPageManager.getInstance().updateStartupPages_(pages);
  };

  StartupPageManager.addStartupPage = function(url) {
    StartupPageManager.getInstance().addStartupPage_(url);
  };

  // Export
  return {
    StartupPageManager: StartupPageManager
  };

});

