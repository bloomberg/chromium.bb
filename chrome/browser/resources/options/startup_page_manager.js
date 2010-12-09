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

      list.selectionModel.addEventListener(
          'change', this.updateRemoveButtonState_.bind(this));

      // Wire up controls.
      $('startupAddButton').onclick = function(event) {
        OptionsPage.showOverlay('addStartupPageOverlay');
      };
      $('startupRemoveButton').onclick =
          this.removeSelectedStartupPages_.bind(this);

      // Remove Windows-style accelerators from button labels.
      // TODO(stuartmorgan): Remove this once the strings are updated.
      $('startupAddButton').textContent =
          localStrings.getStringWithoutAccelerator('startupAddButton');
      $('startupRemoveButton').textContent =
          localStrings.getStringWithoutAccelerator('startupRemoveButton');
    },

    /**
     * Updates the startup pages list with the given entries.
     * @param {Array} pages List of startup pages.
     * @private
     */
    updateStartupPages_: function(pages) {
      $('startupPagesFullList').dataModel = new ArrayDataModel(pages);
      this.updateRemoveButtonState_();
    },

    /**
     * Sets the enabled state of the startup page Remove button based on
     * the current selection in the startup pages list.
     * @private
     */
    updateRemoveButtonState_: function() {
      $('startupRemoveButton').disabled =
          $('startupPagesFullList').selectionModel.selectedIndex == -1;
    },

    /**
     * Removes the selected startup pages.
     * @private
     */
    removeSelectedStartupPages_: function() {
      var selections =
          $('startupPagesFullList').selectionModel.selectedIndexes.map(String);
      chrome.send('removeStartupPages', selections);
    },
  };

  StartupPageManager.updateStartupPages = function(pages) {
    StartupPageManager.getInstance().updateStartupPages_(pages);
  };

  // Export
  return {
    StartupPageManager: StartupPageManager
  };

});

