// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * ClearBrowserDataOverlay class
   * Encapsulated handling of the 'Clear Browser Data' overlay page.
   * @class
   */
  function ClearBrowserDataOverlay() {
    OptionsPage.call(this, 'clearBrowserDataOverlay',
                     templateData.clearBrowserDataOverlayTabTitle,
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

      var f = this.updateCommitButtonState_.bind(this);
      var types = ['browser.clear_data.browsing_history',
                   'browser.clear_data.download_history',
                   'browser.clear_data.cache',
                   'browser.clear_data.cookies',
                   'browser.clear_data.passwords',
                   'browser.clear_data.form_data'];
      types.forEach(function(type) {
          Preferences.getInstance().addEventListener(type, f);
      });

      var checkboxes = document.querySelectorAll(
          '#cbdContentArea input[type=checkbox]');
      for (var i = 0; i < checkboxes.length; i++) {
        checkboxes[i].onclick = f;
      }
      this.updateCommitButtonState_();

      $('clearBrowserDataDismiss').onclick = function(event) {
        ClearBrowserDataOverlay.dismiss();
      };
      $('clearBrowserDataCommit').onclick = function(event) {
        chrome.send('performClearBrowserData');
      };
    },

    // Set the enabled state of the commit button.
    updateCommitButtonState_: function() {
      var checkboxes = document.querySelectorAll(
          '#cbdContentArea input[type=checkbox]');
      var isChecked = false;
      for (var i = 0; i < checkboxes.length; i++) {
        if (checkboxes[i].checked) {
          isChecked = true;
          break;
        }
      }
      $('clearBrowserDataCommit').disabled = !isChecked;
    },
  };

  //
  // Chrome callbacks
  //
  ClearBrowserDataOverlay.setClearingState = function(state) {
    $('deleteBrowsingHistoryCheckbox').disabled = state;
    $('deleteDownloadHistoryCheckbox').disabled = state;
    $('deleteCacheCheckbox').disabled = state;
    $('deleteCookiesCheckbox').disabled = state;
    $('deletePasswordsCheckbox').disabled = state;
    $('deleteFormDataCheckbox').disabled = state;
    $('clearBrowserDataTimePeriod').disabled = state;
    $('cbdThrobber').style.visibility = state ? 'visible' : 'hidden';

    if (state)
      $('clearBrowserDataCommit').disabled = true;
    else
      ClearBrowserDataOverlay.getInstance().updateCommitButtonState_();
  };

  ClearBrowserDataOverlay.setClearLocalDataLabel = function(label) {
    $('deleteCookiesLabel').innerText = label;
  };

  ClearBrowserDataOverlay.doneClearing = function() {
    // The delay gives the user some feedback that the clearing
    // actually worked. Otherwise the dialog just vanishes instantly in most
    // cases.
    window.setTimeout(function() {
      ClearBrowserDataOverlay.dismiss();
    }, 200);
  };

  ClearBrowserDataOverlay.dismiss = function() {
    OptionsPage.closeOverlay();
    this.setClearingState(false);
  };

  // Export
  return {
    ClearBrowserDataOverlay: ClearBrowserDataOverlay
  };
});
