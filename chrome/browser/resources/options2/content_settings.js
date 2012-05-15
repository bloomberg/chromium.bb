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
    this.sessionRestoreEnabled = false;
    this.sessionRestoreSelected = false;
    OptionsPage.call(this, 'content',
                     loadTimeData.getString('contentSettingsPageTabTitle'),
                     'content-settings-page');

    // Keep track of the real value of the "clear on exit" preference. (The UI
    // might override it if the "continue where I left off" setting is
    // selected.)
    var self = this;
    Preferences.getInstance().addEventListener(
        'profile.clear_site_data_on_exit',
        function(event) {
          if (event.value && typeof event.value['value'] != 'undefined') {
            self.clearCookiesOnExit = event.value['value'] == true;
          }
        });
    Preferences.getInstance().addEventListener(
        'session.restore_on_startup',
        this.onSessionRestoreSelectedChanged.bind(this));
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
          page.showList(
              event.target.getAttribute('contentType'));
          OptionsPage.navigateToPage('contentExceptions');
          // Add on the proper hash for the content type, and store that in the
          // history so back/forward and tab restore works.
          var hash = event.target.getAttribute('contentType');
          var url = page.name + '#' + hash;
          window.history.replaceState({pageName: page.name},
                                      page.title,
                                      '/' + url);
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

      if (cr.isChromeOS)
        UIAccountTweaks.applyGuestModeVisibility(document);

      // Cookies filter page ---------------------------------------------------
      $('show-cookies-button').onclick = function(event) {
        chrome.send('coreOptionsUserMetricsAction', ['Options_ShowCookies']);
        OptionsPage.navigateToPage('cookies');
      };

      // Remove from DOM instead of hiding so :last-of-type applies the style
      // correctly.
      var intentsSection = $('intents-section');
      if (!loadTimeData.getBoolean('enable_web_intents') && intentsSection)
        intentsSection.parentNode.removeChild(intentsSection);

      if (loadTimeData.getBoolean('enable_restore_session_state')) {
        this.sessionRestoreEnabled = true;
        this.updateSessionRestoreContentSettings();
      }

      $('content-settings-overlay-confirm').onclick =
          OptionsPage.closeOverlay.bind(OptionsPage);
    },

    /**
     * Called when the value of the "On startup" setting changes.
     * @param {Event} event Change event.
     * @private
     */
    onSessionRestoreSelectedChanged: function(event) {
      if (!event.value || typeof event.value['value'] == 'undefined')
        return;

      this.sessionRestoreSelected = event.value['value'] == 1;

      if (this.sessionRestoreSelected)
        this.updateSessionRestoreContentSettings();
      else
        this.restoreContentSettings();
    },

    // If the "continue where I left off" setting is selected, disable the
    // "clear on exit" checkbox, and the "session only" setting for cookies.
    updateSessionRestoreContentSettings: function() {
      // This feature is behind a command line flag.
      if (this.sessionRestoreEnabled && this.sessionRestoreSelected) {
        $('clear-cookies-on-exit').checked = false;
        $('clear-cookies-on-exit').setDisabled('sessionrestore', true);

        if ($('cookies-session').checked) {
          $('cookies-allow').checked = true;
        }
        $('cookies-session').disabled = true;
      }
    },

    // Restore the values of the UI elements based on the real values of the
    // preferences.
    restoreContentSettings: function() {
      $('clear-cookies-on-exit').checked = this.clearCookiesOnExit;
      $('clear-cookies-on-exit').setDisabled('sessionrestore', false);

      if (this.cookiesSession && $('cookies-allow').checked) {
        $('cookies-session').checked = true;
      }
      $('cookies-session').disabled = false;
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
                             dict[group]['value'] + ']').checked = true;
      var radios = document.querySelectorAll('input[type=radio][name=' +
                                             group + ']');
      var managedBy = dict[group]['managedBy'];
      for (var i = 0, len = radios.length; i < len; i++) {
        radios[i].disabled = (managedBy != 'default');
        radios[i].controlledBy = managedBy;
      }
    }
    // Keep track of the real cookie content setting. (The UI might override it
    // if the reopen last pages setting is selected.)
    if ('cookies' in dict && 'value' in dict['cookies']) {
      ContentSettings.getInstance().cookiesSession =
          dict['cookies']['value'] == 'session';
    }
    ContentSettings.getInstance().updateSessionRestoreContentSettings();
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

  // Export
  return {
    ContentSettings: ContentSettings
  };

});
