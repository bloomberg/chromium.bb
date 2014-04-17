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
     * Whether deleting history and downloads is allowed.
     * @type {boolean}
     * @private
     */
    allowDeletingHistory_: true,

    /**
     * Whether or not clearing browsing data is currently in progress.
     * @type {boolean}
     * @private
     */
    isClearingInProgress_: true,

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

      // At this point, assume that we are currently in the process of clearing
      // data, so as to prevent the controls from being hazardously enabled for
      // a very short time before ClearBrowserDataOverlay.setClearing() is
      // called by the native side with the authoritative state.
      this.setClearing(true);

      this.createStuffRemainsFooter_();

      $('clear-browser-data-dismiss').onclick = function(event) {
        ClearBrowserDataOverlay.dismiss();
      };
      $('clear-browser-data-commit').onclick = function(event) {
        ClearBrowserDataOverlay.setClearing(true);
        chrome.send('performClearBrowserData');
      };

      var show = loadTimeData.getBoolean('showDeleteBrowsingHistoryCheckboxes');
      this.showDeleteHistoryCheckboxes_(show);
    },

    /**
     * Create a footer that explains that some content is not cleared by the
     * clear browsing history dialog.
     */
    createStuffRemainsFooter_: function() {
      // The localized string is of the form "Saved [content settings] and
      // {search engines} will not be cleared and may reflect your browsing
      // habits.". The following parses out the parts in brackts and braces and
      // converts them into buttons whereas the remainders are represented as
      // span elements.
      var footer =
          document.querySelector('#some-stuff-remains-footer p');
      var footerFragments =
          loadTimeData.getString('contentSettingsAndSearchEnginesRemain')
                      .split(/([|#])/);
      for (var i = 0; i < footerFragments.length;) {
        var buttonId = '';
        if (i + 2 < footerFragments.length) {
          if (footerFragments[i] == '|' && footerFragments[i + 2] == '|') {
            buttonId = 'open-content-settings-from-clear-browsing-data';
          } else if (footerFragments[i] == '#' &&
                     footerFragments[i + 2] == '#') {
            buttonId = 'open-search-engines-from-clear-browsing-data';
          }
        }

        if (buttonId != '') {
          var button = document.createElement('button');
          button.setAttribute('id', buttonId);
          button.setAttribute('class', 'link-button');
          button.textContent = footerFragments[i + 1];
          footer.appendChild(button);
          i += 3;
        } else {
          var span = document.createElement('span');
          span.textContent = footerFragments[i];
          footer.appendChild(span);
          i += 1;
        }
      }
      $('open-content-settings-from-clear-browsing-data').onclick =
          function(event) {
        OptionsPage.navigateToPage('content');
      }
      $('open-search-engines-from-clear-browsing-data').onclick =
          function(event) {
        OptionsPage.navigateToPage('searchEngines');
      }
    },

    /**
     * Sets the enabled state of the checkboxes and buttons based on whether or
     * not we are in the process of clearing data.
     * @param {boolean} clearing Whether the browsing history is currently
     *     being cleared.
     */
    setClearing: function(clearing) {
      $('delete-browsing-history-checkbox').disabled = clearing;
      $('delete-download-history-checkbox').disabled = clearing;
      $('delete-cache-checkbox').disabled = clearing;
      $('delete-cookies-checkbox').disabled = clearing;
      $('delete-passwords-checkbox').disabled = clearing;
      $('delete-form-data-checkbox').disabled = clearing;
      $('delete-hosted-apps-data-checkbox').disabled = clearing;
      $('deauthorize-content-licenses-checkbox').disabled = clearing;
      $('clear-browser-data-time-period').disabled = clearing;
      $('cbd-throbber').style.visibility = clearing ? 'visible' : 'hidden';
      $('clear-browser-data-dismiss').disabled = clearing;

      // The enabled state of the commit button is further based on whether or
      // not any of the check boxes are checked.
      this.isClearingInProgress_ = clearing;
      this.updateCommitButtonState_();
    },

    /**
     * Sets the enabled state of the commit button.
     */
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
      $('clear-browser-data-commit').disabled =
          !isChecked || this.isClearingInProgress_;
    },

    setAllowDeletingHistory: function(allowed) {
      this.allowDeletingHistory_ = allowed;
    },

    showDeleteHistoryCheckboxes_: function(show) {
      if (!show) {
        $('delete-browsing-history-container').hidden = true;
        $('delete-download-history-container').hidden = true;
      }
    },

    /** @override */
    didShowPage: function() {
      var allowed = ClearBrowserDataOverlay.getInstance().allowDeletingHistory_;
      ClearBrowserDataOverlay.updateHistoryCheckboxes(allowed);
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
    ClearBrowserDataOverlay.getInstance().setAllowDeletingHistory(allowed);
  };

  ClearBrowserDataOverlay.setClearing = function(clearing) {
    ClearBrowserDataOverlay.getInstance().setClearing(clearing);
  };

  ClearBrowserDataOverlay.setBannerVisibility = function(args) {
    var visible = args[0];
    $('clear-browser-data-info-banner').hidden = !visible;
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
    var topmostVisiblePage = OptionsPage.getTopmostVisiblePage();
    if (topmostVisiblePage && topmostVisiblePage.name == 'clearBrowserData')
      OptionsPage.closeOverlay();
    this.setClearing(false);
  };

  // Export
  return {
    ClearBrowserDataOverlay: ClearBrowserDataOverlay
  };
});
