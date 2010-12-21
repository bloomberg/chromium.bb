// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

  /**
   * EditSearchEngineOverlay class
   * Encapsulated handling of the 'Edit Search Engine' overlay page.
   * @class
   * @constructor
   */
  function EditSearchEngineOverlay() {
    OptionsPage.call(this, 'editSearchEngineOverlay',
                     templateData.editSearchEngineTitle,
                     'editSearchEngineOverlay');
  }

  cr.addSingletonGetter(EditSearchEngineOverlay);

  EditSearchEngineOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initializes the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var self = this;
      var editForm = $('editSearchEngineForm');
      editForm.onreset = function(e) {
        chrome.send('searchEngineEditCancelled');
        self.dismissOverlay_();
      };
      editForm.onsubmit = function(e) {
        chrome.send('searchEngineEditCompleted', self.getInputFieldValues_());
        self.dismissOverlay_();
        return false;
      };
      var fieldIDs = ['editSearchEngineName',
                      'editSearchEngineKeyword',
                      'editSearchEngineURL'];
      for (var i = 0; i < fieldIDs.length; i++) {
        var field = $(fieldIDs[i]);
        field.oninput = this.validateFields_.bind(this);
        field.onkeydown = function(e) {
          if (e.keyCode == 27)  // Esc
            editForm.reset();
        };
      }
    },

    /**
     * Clears any uncommited input, and dismisses the overlay.
     * @private
     */
    dismissOverlay_: function() {
      this.setEditDetails_();
      OptionsPage.clearOverlays();
    },

    /**
     * Fills the text fields from the given search engine.
     * @private
     */
    setEditDetails_: function(engineDetails) {
      if (engineDetails) {
        $('editSearchEngineName').value = engineDetails['name'];
        $('editSearchEngineKeyword').value = engineDetails['keyword'];
        var urlField = $('editSearchEngineURL');
        urlField.value = engineDetails['url'];
        urlField.disabled = engineDetails['urlLocked'];
        this.validateFields_();
      } else {
        $('editSearchEngineName').value = '';
        $('editSearchEngineKeyword').value = '';
        $('editSearchEngineURL').value = '';
        var invalid = { name: false, keyword: false, url: false };
        this.updateValidityWithResults_(invalid);
      }
    },

    /**
     * Starts the process of asynchronously validating the user input. Results
     * will be reported to updateValidityWithResults_.
     * @private
     */
    validateFields_: function() {
      chrome.send('checkSearchEngineInfoValidity', this.getInputFieldValues_());
    },

    /**
     * Sets the validation images and the enabled state of the Add button based
     * on the current values of the text fields.
     * @private
     * @param {Object} The dictionary of validity states.
     */
    updateValidityWithResults_: function(validity) {
      this.setBadgeValidity_($('editSearchEngineNameValidity'),
                             validity['name'],
                             'editSearchEngineInvalidTitleToolTip');
      this.setBadgeValidity_($('editSearchEngineKeywordValidity'),
                             validity['keyword'],
                             'editSearchEngineInvalidKeywordToolTip');
      this.setBadgeValidity_($('editSearchEngineURLValidity'),
                             validity['url'],
                             'editSearchEngineInvalidURLToolTip');
      $('editSearchEngineOkayButton').disabled =
          !(validity['name'] && validity['keyword'] && validity['url']);
    },

    /**
     * Updates the state of the given validity indicator badge.
     * @private
     * @param {HTMLElement} The badge element to adjust.
     * @param {boolean} Whether or not the badge should be set to the valid
     *     state.
     * @param {string} The tooltip string id for the invalid state.
     */
    setBadgeValidity_: function(element, isValid, tooltip_id) {
      if (isValid) {
        element.className = 'valid-badge';
        element.title = '';
      } else {
        element.className = 'alert-badge';
        element.title = localStrings.getString(tooltip_id);
      }
    },

    /**
     * Returns the input field values as an array suitable for passing to
     * chrome.send. The order of the array is important.
     * @private
     * @return {array} The current input field values.
     */
    getInputFieldValues_: function() {
      return [ $('editSearchEngineName').value,
               $('editSearchEngineKeyword').value,
               $('editSearchEngineURL').value ];
    }
  };

  EditSearchEngineOverlay.setEditDetails = function(engineDetails) {
    EditSearchEngineOverlay.getInstance().setEditDetails_(engineDetails);
  };

  EditSearchEngineOverlay.validityCheckCallback = function(validity) {
    EditSearchEngineOverlay.getInstance().updateValidityWithResults_(validity);
  };

  // Export
  return {
    EditSearchEngineOverlay: EditSearchEngineOverlay
  };

});
