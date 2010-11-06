// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  //////////////////////////////////////////////////////////////////////////////
  // ContentSettings class:

  /**
   * Encapsulated handling of content settings page.
   * @constructor
   */
  function ContentSettings() {
    this.activeNavTab = null;
    OptionsPage.call(this, 'content', templateData.contentSettingsPage,
                     'contentSettingsPage');
  }

  cr.addSingletonGetter(ContentSettings);

  ContentSettings.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      chrome.send('getContentFilterSettings');

      var exceptionsAreas = this.pageDiv.querySelectorAll('div[contentType]');
      for (var i = 0; i < exceptionsAreas.length; i++) {
        options.contentSettings.ExceptionsArea.decorate(exceptionsAreas[i]);
      }

      cr.ui.decorate('.zippy', options.Zippy);
      this.pageDiv.addEventListener('measure', function(e) {
        if (e.target.classList.contains('zippy')) {
          var lists = e.target.querySelectorAll('list');
          for (var i = 0; i < lists.length; i++) {
            if (lists[i].redraw) {
              lists[i].redraw();
            }
          }
        }
      });

      // Cookies filter page ---------------------------------------------------
      $('block-third-party-cookies').onclick = function(event) {
        chrome.send('setAllowThirdPartyCookies',
                    [String($('block-third-party-cookies').checked)]);
      };

      $('show-cookies-button').onclick = function(event) {
        chrome.send('coreOptionsUserMetricsAction', ['Options_ShowCookies']);
        OptionsPage.showPageByName('cookiesView');
      };

      $('plugins-tab').onclick = function(event) {
        chrome.send('openPluginsTab');
      };
    },

    /**
     * Handles a hash value in the URL (such as bar in
     * chrome://options/foo#bar). Overrides the default action of showing an
     * overlay by instead navigating to a particular subtab.
     * @param {string} hash The hash value.
     */
    handleHash: function(hash) {
      OptionsPage.showTab($(hash + '-nav-tab'));
    },
  };

  /**
   * Sets the values for all the content settings radios.
   * @param {Object} dict A mapping from radio groups to the checked value for
   *     that group.
   */
  ContentSettings.setContentFilterSettingsValue = function(dict) {
    for (var group in dict) {
      document.querySelector('input[type=radio][name=' + group +
                             '][value=' + dict[group] + ']').checked = true;
    }
  };

  /**
   * Initializes an exceptions list.
   * @param {string} type The content type that we are setting exceptions for.
   * @param {Array} list An array of pairs, where the first element of each pair
   *     is the filter string, and the second is the setting (allow/block).
   */
  ContentSettings.setExceptions = function(type, list) {
    var exceptionsList =
        document.querySelector('div[contentType=' + type + ']' +
                               '[mode=normal] list');
    exceptionsList.clear();
    for (var i = 0; i < list.length; i++) {
      exceptionsList.addException(list[i]);
    }
  };

  ContentSettings.setOTRExceptions = function(type, list) {
    var exceptionsArea =
        document.querySelector('div[contentType=' + type + '][mode=otr]');
    exceptionsArea.otrProfileExists = true;

    // Find the containing zippy, set it to show OTR profiles, and remeasure it
    // to make it smoothly animate to the new size.
    var zippy = exceptionsArea;
    while (zippy &&
           (!zippy.classList || !zippy.classList.contains('zippy'))) {
      zippy = zippy.parentNode;
    }
    if (zippy) {
      zippy.classList.add('show-otr');
      zippy.remeasure();
    }

    var exceptionsList = exceptionsArea.querySelector('list');
    exceptionsList.clear();
    for (var i = 0; i < list.length; i++) {
      exceptionsList.addException(list[i]);
    }

    // If an OTR table is added while the normal exceptions area is already
    // showing (because the exceptions area is already expanded), then show
    // the new OTR table.
    var parentExceptionsArea =
        document.querySelector('div[contentType=' + type + '][mode=normal]');
    if (!parentExceptionsArea.classList.contains('hidden')) {
      exceptionsArea.querySelector('list').redraw();
    }
  };

  /**
   * Clears and hides the incognito exceptions lists.
   */
  ContentSettings.OTRProfileDestroyed = function() {
    // Find all zippies, set them to hide OTR profiles, and remeasure them
    // to make them smoothly animate to the new size.
    var zippies = document.querySelectorAll('.zippy');
    for (var i = 0; i < zippies.length; i++) {
      zippies[i].classList.remove('show-otr');
      zippies[i].remeasure();
    }

    var exceptionsAreas =
        document.querySelectorAll('div[contentType][mode=otr]');
    for (var i = 0; i < exceptionsAreas.length; i++) {
      exceptionsAreas[i].otrProfileExists = false;
      exceptionsAreas[i].querySelector('list').clear();
    }
  };

  /**
   * Sets the initial value for the Third Party Cookies checkbox.
   * @param {boolean=} block True if we are blocking third party cookies.
   */
  ContentSettings.setBlockThirdPartyCookies = function(block) {
    $('block-third-party-cookies').checked = block;
  };

  /**
   * The browser's response to a request to check the validity of a given URL
   * pattern.
   * @param {string} type The content type.
   * @param {string} mode The browser mode.
   * @param {string} pattern The pattern.
   * @param {bool} valid Whether said pattern is valid in the context of
   *     a content exception setting.
   */
  ContentSettings.patternValidityCheckComplete =
      function(type, mode, pattern, valid) {
    var exceptionsList =
        document.querySelector('div[contentType=' + type + '][mode=' + mode +
                               '] list');
    exceptionsList.patternValidityCheckComplete(pattern, valid);
  };

  // Export
  return {
    ContentSettings: ContentSettings
  };

});
