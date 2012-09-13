// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  //////////////////////////////////////////////////////////////////////////////
  // ContentSettings class:

  /**
   * Encapsulated handling of content settings page.
   * @constructor
   */
  function ContentSettings() {
    this.activeNavTab = null;
    OptionsPage.call(this, 'content',
                     loadTimeData.getString('contentSettingsPageTabTitle'),
                     'content-settings-page');
  }

  cr.addSingletonGetter(ContentSettings);

  ContentSettings.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      chrome.send('getContentFilterSettings');

      var exceptionsButtons =
          this.pageDiv.querySelectorAll('.exceptions-list-button');
      for (var i = 0; i < exceptionsButtons.length; i++) {
        exceptionsButtons[i].onclick = function(event) {
          var page = ContentSettingsExceptionsArea.getInstance();

          // Add on the proper hash for the content type, and store that in the
          // history so back/forward and tab restore works.
          var hash = event.target.getAttribute('contentType');
          var url = page.name + '#' + hash;
          window.history.replaceState({pageName: page.name},
                                      page.title,
                                      '/' + url);

          // Navigate after the history has been replaced in order to have the
          // correct hash loaded.
          OptionsPage.navigateToPage('contentExceptions');

          uber.invokeMethodOnParent('setPath', {path: url});
          uber.invokeMethodOnParent('setTitle',
              {title: loadTimeData.getString(hash + 'TabTitle')});
        };
      }

      var manageHandlersButton = $('manage-handlers-button');
      if (manageHandlersButton) {
        manageHandlersButton.onclick = function(event) {
          OptionsPage.navigateToPage('handlers');
        };
      }

      $('manage-galleries-button').onclick = function(event) {
        OptionsPage.navigateToPage('manageGalleries');
      };

      if (cr.isChromeOS)
        UIAccountTweaks.applyGuestModeVisibility(document);

      // Cookies filter page ---------------------------------------------------
      $('show-cookies-button').onclick = function(event) {
        chrome.send('coreOptionsUserMetricsAction', ['Options_ShowCookies']);
        OptionsPage.navigateToPage('cookies');
      };
      $('show-app-cookies-button').onclick = function(event) {
        OptionsPage.navigateToPage('app-cookies');
      };

      var intentsSection = $('intents-section');
      if (!loadTimeData.getBoolean('enable_web_intents') && intentsSection)
        intentsSection.parentNode.removeChild(intentsSection);

      $('content-settings-overlay-confirm').onclick =
          OptionsPage.closeOverlay.bind(OptionsPage);

      $('pepper-flash-cameramic-section').style.display = 'none';
      $('pepper-flash-cameramic-exceptions-div').style.display = 'none';
    },
  };

  ContentSettings.updateHandlersEnabledRadios = function(enabled) {
    var selector = '#content-settings-page input[type=radio][value=' +
        (enabled ? 'allow' : 'block') + '].handler-radio';
    document.querySelector(selector).checked = true;
  };

  /**
   * Sets the values for all the content settings radios.
   * @param {Object} dict A mapping from radio groups to the checked value for
   *     that group.
   */
  ContentSettings.setContentFilterSettingsValue = function(dict) {
    for (var group in dict) {
      document.querySelector('input[type=radio][name=' + group + '][value=' +
                             dict[group].value + ']').checked = true;
      var radios = document.querySelectorAll('input[type=radio][name=' +
                                             group + ']');
      var managedBy = dict[group]['managedBy'];
      for (var i = 0, len = radios.length; i < len; i++) {
        radios[i].disabled = (managedBy != 'default');
        radios[i].controlledBy = managedBy;
      }
    }
    OptionsPage.updateManagedBannerVisibility();
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
                               ' list[mode=normal]');
    exceptionsList.setExceptions(list);
  };

  ContentSettings.setHandlers = function(list) {
    $('handlers-list').setHandlers(list);
  };

  ContentSettings.setIgnoredHandlers = function(list) {
    $('ignored-handlers-list').setHandlers(list);
  };

  ContentSettings.setOTRExceptions = function(type, list) {
    var exceptionsList =
        document.querySelector('div[contentType=' + type + ']' +
                               ' list[mode=otr]');

    exceptionsList.parentNode.hidden = false;
    exceptionsList.setExceptions(list);
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
        document.querySelector('div[contentType=' + type + '] ' +
                               'list[mode=' + mode + ']');
    exceptionsList.patternValidityCheckComplete(pattern, valid);
  };

  /**
   * Enables the Pepper Flash camera and microphone settings.
   * Please note that whether the settings are actually showed or not is also
   * affected by the style class pepper-flash-settings.
   */
  ContentSettings.enablePepperFlashCameraMicSettings = function() {
    $('pepper-flash-cameramic-section').style.display = '';
    $('pepper-flash-cameramic-exceptions-div').style.display = '';
  }

  // Export
  return {
    ContentSettings: ContentSettings
  };

});
