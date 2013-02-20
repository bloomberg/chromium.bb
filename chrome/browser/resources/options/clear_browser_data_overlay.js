// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
    OptionsPage.call(this, 'clearBrowserData',
                     loadTimeData.getString('clearBrowserDataOverlayTabTitle'),
                     'clear-browser-data-overlay');
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
                   'browser.clear_data.form_data',
                   'browser.clear_data.hosted_apps_data',
                   'browser.clear_data.content_licenses'];
      types.forEach(function(type) {
          Preferences.getInstance().addEventListener(type, f);
      });

      var checkboxes = document.querySelectorAll(
          '#cbd-content-area input[type=checkbox]');
      for (var i = 0; i < checkboxes.length; i++) {
        checkboxes[i].onclick = f;
      }
      this.updateCommitButtonState_();

      $('clear-browser-data-dismiss').onclick = function(event) {
        ClearBrowserDataOverlay.dismiss();
      };
      $('clear-browser-data-commit').onclick = function(event) {
        ClearBrowserDataOverlay.setClearingState(true);
        chrome.send('performClearBrowserData');
      };
    },

    // Set the enabled state of the commit button.
    updateCommitButtonState_: function() {
      var checkboxes = document.querySelectorAll(
          '#cbd-content-area input[type=checkbox]');
      var isChecked = false;
      for (var i = 0; i < checkboxes.length; i++) {
        if (checkboxes[i].checked) {
          isChecked = true;
          break;
        }
      }
      $('clear-browser-data-commit').disabled = !isChecked;
    },
  };

  //
  // Chrome callbacks
  //
  /**
   * Updates the disabled status of the browsing-history and downloads
   * checkboxes, also unchecking them if they are disabled. This is called in
   * response to a change in the corresponding preference.
   */
  ClearBrowserDataOverlay.updateHistoryCheckboxes = function(allowed) {
    $('delete-browsing-history-checkbox').disabled = !allowed;
    $('delete-download-history-checkbox').disabled = !allowed;
    if (!allowed) {
      $('delete-browsing-history-checkbox').checked = false;
      $('delete-download-history-checkbox').checked = false;
    }
  };

  ClearBrowserDataOverlay.setClearingState = function(state) {
    $('delete-browsing-history-checkbox').disabled = state;
    $('delete-download-history-checkbox').disabled = state;
    $('delete-cache-checkbox').disabled = state;
    $('delete-cookies-checkbox').disabled = state;
    $('delete-passwords-checkbox').disabled = state;
    $('delete-form-data-checkbox').disabled = state;
    $('delete-hosted-apps-data-checkbox').disabled = state;
    $('deauthorize-content-licenses-checkbox').disabled = state;
    $('clear-browser-data-time-period').disabled = state;
    $('cbd-throbber').style.visibility = state ? 'visible' : 'hidden';
    $('clear-browser-data-dismiss').disabled = state;

    if (state)
      $('clear-browser-data-commit').disabled = true;
    else
      ClearBrowserDataOverlay.getInstance().updateCommitButtonState_();
  };

  ClearBrowserDataOverlay.setBannerVisibility = function(args) {
    var visible = args[0];
    $('info-banner').hidden = !visible;
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
