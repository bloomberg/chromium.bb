// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

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

  ClearBrowserDataOverlay.throbIntervalId = 0;

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

      // Setup option values for the time period select control.
      $('clearBrowsingDataTimePeriod').initializeValues(
          templateData.clearBrowsingDataTimeList)

      // Setup click handler for the clear(Ok) button.
      $('clearBrowsingDataCommit').onclick = function(event) {
        chrome.send('performClearBrowserData');
      };
    }
  };

  //
  // Chrome callbacks
  //
  function clearBrowserDataSetClearingState(state) {
    $('deleteBrowsingHistoryCheckbox').disabled = state;
    $('deleteDownloadHistoryCheckbox').disabled = state;
    $('deleteCacheCheckbox').disabled = state;
    $('deleteCookiesCheckbox').disabled = state;
    $('deletePasswordsCheckbox').disabled = state;
    $('deleteFormDataCheckbox').disabled = state;
    $('clearBrowsingDataTimePeriod').disabled = state;
    $('clearBrowsingDataCommit').disabled = state;
    $('clearBrowsingDataDismiss').disabled = state;
    $('cbdThrobber').style.visibility = state ? 'visible' : 'hidden';

    function advanceThrobber() {
      var throbber = $('cbdThrobber');
      // TODO(csilv): make this smoother using time-based animation?
      throbber.style.backgroundPositionX =
          ((parseInt(getComputedStyle(throbber).backgroundPositionX, 10) - 16) %
          576) + 'px';
    }
    if (state) {
      ClearBrowserDataOverlay.throbIntervalId =
          setInterval(advanceThrobber, 30);
    } else {
      clearInterval(ClearBrowserDataOverlay.throbIntervalId);
    }
  }

  function clearBrowserDataDismiss() {
    OptionsPage.clearOverlays();
    clearBrowserDataSetClearingState(false);
  }

  // Export
  return {
    ClearBrowserDataOverlay: ClearBrowserDataOverlay
  };

});

