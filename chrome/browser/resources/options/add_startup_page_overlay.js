// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

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

      var self = this;
      var addForm = $('addStartupPageForm');
      addForm.onreset = this.dismissOverlay_.bind(this);
      addForm.onsubmit =  function(e) {
        var urlField = $('addStartupPageURL');
        BrowserOptions.addStartupPage(urlField.value);

        self.dismissOverlay_();
        return false;
      };
      $('addStartupPageURL').oninput = this.updateAddButtonState_.bind(this);
      $('addStartupPageURL').onkeydown = function(e) {
        if (e.keyCode == 27)  // Esc
          $('addStartupPageForm').reset();
      };

      var list = $('addStartupRecentPageList');
      options.add_startup_page.RecentPageList.decorate(list);
      var selectionModel = new ListSingleSelectionModel;
      list.selectionModel = selectionModel;

      selectionModel.addEventListener('change',
                                      this.selectionChanged_.bind(this));

      this.addEventListener('visibleChange', function(event) {
          $('addStartupPageURL').focus();
          $('addStartupRecentPageList').redraw();
      });
    },

    /**
     * Clears any uncommited input, and dismisses the overlay.
     * @private
     */
    dismissOverlay_: function() {
      $('addStartupPageURL').value = '';
      $('addStartupRecentPageList').selectionModel.unselectAll();
      this.updateAddButtonState_();
      OptionsPage.clearOverlays();
    },

    /**
     * Sets the enabled state of the startup page Add button based on
     * the current value of the text field.
     * @private
     */
    updateAddButtonState_: function() {
      $('addStartupPageAddButton').disabled =
          $('addStartupPageURL').value == '';
    },

    /**
      * Updates the recent pages list list with the given entries.
      * @private
      * @param {Array} pages List of recent pages.
      */
    updateRecentPageList_: function(pages) {
      $('addStartupRecentPageList').dataModel = new ArrayDataModel(pages);
    },

    /**
     * Handles selection changes in the list so that we can populate the input
     * field.
     * @private
     * @param {!cr.Event} e Event with change info.
     */
    selectionChanged_: function(e) {
      var selectedIndex =
          $('addStartupRecentPageList').selectionModel.selectedIndex;
      if (selectedIndex != -1)
        chrome.send('updateAddStartupFieldWithPage', [String(selectedIndex)]);
    },

    /**
      * Sets the value of the input field to the given string.
      * @private
      * @param {string} url String value to set the input field to.
      */
    setInputFieldValue_: function(url) {
      $('addStartupPageURL').value = url;
      this.updateAddButtonState_();
    },
  };

  AddStartupPageOverlay.updateRecentPageList = function(pages) {
    AddStartupPageOverlay.getInstance().updateRecentPageList_(pages);
  };

  AddStartupPageOverlay.setInputFieldValue = function(url) {
    AddStartupPageOverlay.getInstance().setInputFieldValue_(url);
  };

  // Export
  return {
    AddStartupPageOverlay: AddStartupPageOverlay
  };

});
