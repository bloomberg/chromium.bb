// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const ListSelectionModel = cr.ui.ListSelectionModel;

  //
  // BrowserOptions class
  // Encapsulated handling of browser options page.
  //
  function BrowserOptions() {
    OptionsPage.call(this, 'browser', templateData.browserPage, 'browserPage');
  }

  cr.addSingletonGetter(BrowserOptions);

  BrowserOptions.prototype = {
    // Inherit BrowserOptions from OptionsPage.
    __proto__: options.OptionsPage.prototype,

    homepage_pref_: {
      'name': 'homepage',
      'value': '',
      'managed': false
    },

    homepage_is_newtabpage_pref_: {
      'name': 'homepage_is_newtabpage',
      'value': true,
      'managed': false
    },

    /**
     * Initialize BrowserOptions page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      // Wire up controls.
      $('startupAddButton').onclick = function(event) {
        OptionsPage.showOverlay('addStartupPageOverlay');
      };
      $('startupRemoveButton').onclick =
          this.removeSelectedStartupPages_.bind(this);
      $('startupUseCurrentButton').onclick = function(event) {
        chrome.send('setStartupPagesToCurrentPages');
      };
      $('defaultSearchManageEnginesButton').onclick = function(event) {
        OptionsPage.showPageByName('searchEngines');
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ManageSearchEngines']);
      };
      $('instantEnableCheckbox').onclick = function(event) {
        var alreadyConfirmed = $('instantDialogShown').checked;

        if (this.checked && !alreadyConfirmed) {
          // Leave disabled for now. The PrefCheckbox handler already set it to
          // true so undo that.
          Preferences.setBooleanPref(this.pref, false, this.metric);
          OptionsPage.showOverlay('instantConfirmOverlay');
        }
      };

      var homepageField = $('homepageURL');
      $('homepageUseNTPButton').onchange =
          this.handleHomepageUseNTPButtonChange_.bind(this);
      $('homepageUseURLButton').onchange =
          this.handleHomepageUseURLButtonChange_.bind(this);
      homepageField.onchange =
          this.handleHomepageURLChange_.bind(this);

      // Ensure that changes are committed when closing the page.
      window.addEventListener('unload', function() {
          if (document.activeElement == homepageField)
            homepageField.blur();
          });

      if (!cr.isChromeOS) {
        $('defaultBrowserUseAsDefaultButton').onclick = function(event) {
          chrome.send('becomeDefaultBrowser');
        };
      }

      var list = $('startupPages');
      options.browser_options.StartupPageList.decorate(list);
      list.selectionModel = new ListSelectionModel;

      list.selectionModel.addEventListener(
          'change', this.updateRemoveButtonState_.bind(this));

      this.addEventListener('visibleChange', function(event) {
        $('startupPages').redraw();
      });

      // Check if we are in the guest mode.
      if (cr.commandLine.options['--bwsi']) {
        // Disable input and button elements under the startup section.
        var elements = $('startupSection').querySelectorAll('input, button');
        for (var i = 0; i < elements.length; i++) {
          elements[i].disabled = true;
          elements[i].manually_disabled = true;
        }
      } else {
        // Initialize control enabled states.
        Preferences.getInstance().addEventListener('session.restore_on_startup',
            this.updateCustomStartupPageControlStates_.bind(this));
        Preferences.getInstance().addEventListener('homepage_is_newtabpage',
            this.handleHomepageIsNewTabPageChange_.bind(this));
        Preferences.getInstance().addEventListener('homepage',
            this.handleHomepageChange_.bind(this));

        this.updateCustomStartupPageControlStates_();
      }

      // Remove Windows-style accelerators from button labels.
      // TODO(stuartmorgan): Remove this once the strings are updated.
      $('startupAddButton').textContent =
          localStrings.getStringWithoutAccelerator('startupAddButton');
      $('startupRemoveButton').textContent =
          localStrings.getStringWithoutAccelerator('startupRemoveButton');
    },

    /**
     * Update the Default Browsers section based on the current state.
     * @private
     * @param {string} statusString Description of the current default state.
     * @param {boolean} isDefault Whether or not the browser is currently
     *     default.
     * @param {boolean} canBeDefault Whether or not the browser can be default.
     */
    updateDefaultBrowserState_: function(statusString, isDefault,
                                         canBeDefault) {
      var label = $('defaultBrowserState');
      label.textContent = statusString;

      $('defaultBrowserUseAsDefaultButton').disabled = !canBeDefault ||
                                                       isDefault;
    },

    /**
     * Clears the search engine popup.
     * @private
     */
    clearSearchEngines_: function() {
      $('defaultSearchEngine').textContent = '';
    },

    /**
     * Updates the search engine popup with the given entries.
     * @param {Array} engines List of available search engines.
     * @param {Integer} defaultValue The value of the current default engine.
     */
    updateSearchEngines_: function(engines, defaultValue) {
      this.clearSearchEngines_();
      engineSelect = $('defaultSearchEngine');
      engineCount = engines.length;
      var defaultIndex = -1;
      for (var i = 0; i < engineCount; i++) {
        var engine = engines[i];
        var option = new Option(engine['name'], engine['index']);
        if (defaultValue == option.value)
          defaultIndex = i;
        engineSelect.appendChild(option);
      }
      if (defaultIndex >= 0)
        engineSelect.selectedIndex = defaultIndex;
    },

    /**
     * Returns true if the custom startup page control block should
     * be enabled.
     * @private
     * @returns {boolean} Whether the startup page controls should be
     *     enabled.
     */
    shouldEnableCustomStartupPageControls_: function(pages) {
      return $('startupShowPagesButton').checked;
    },

    /**
     * Updates the startup pages list with the given entries.
     * @private
     * @param {Array} pages List of startup pages.
     */
    updateStartupPages_: function(pages) {
      $('startupPages').dataModel = new ArrayDataModel(pages);
      this.updateRemoveButtonState_();
    },

    /**
     * Handles change events of the radio button 'homepageUseURLButton'.
     * @private
     * @param {event} change event.
     */
    handleHomepageUseURLButtonChange_: function(event) {
      Preferences.setBooleanPref('homepage_is_newtabpage', false);
    },

    /**
     * Handles change events of the radio button 'homepageUseNTPButton'.
     * @private
     * @param {event} change event.
     */
    handleHomepageUseNTPButtonChange_: function(event) {
      Preferences.setBooleanPref('homepage_is_newtabpage', true);
    },

    /**
     * Handles change events of the text field 'homepageURL'.
     * @private
     * @param {event} change event.
     */
    handleHomepageURLChange_: function(event) {
      Preferences.setStringPref('homepage', $('homepageURL').value);
    },

    /**
     * Handle change events of the preference 'homepage'.
     * @private
     * @param {event} preference changed event.
     */
    handleHomepageChange_: function(event) {
      this.homepage_pref_.value = event.value['value'];
      this.homepage_pref_.managed = event.value['managed'];
      if (this.isHomepageURLNewTabPageURL_() && !this.homepage_pref_.managed &&
          !this.homepage_is_newtabpage_pref_.managed) {
        var useNewTabPage = this.isHomepageIsNewTabPageChoiceSelected_();
        Preferences.setStringPref('homepage', '')
        Preferences.setBooleanPref('homepage_is_newtabpage', useNewTabPage)
      }
      this.updateHomepageControlStates_();
    },

    /**
     * Handle change events of the preference homepage_is_newtabpage.
     * @private
     * @param {event} preference changed event.
     */
    handleHomepageIsNewTabPageChange_: function(event) {
      this.homepage_is_newtabpage_pref_.value = event.value['value'];
      this.homepage_is_newtabpage_pref_.managed = event.value['managed'];
      this.updateHomepageControlStates_();
    },

    /**
     * Update homepage preference UI controls.  Here's a table describing the
     * desired characteristics of the homepage choice radio value, its enabled
     * state and the URL field enabled state. They depend on the values of the
     * managed bits for homepage (m_hp) and homepageIsNewTabPage (m_ntp)
     * preferences, as well as the value of the homepageIsNewTabPage preference
     * (ntp) and whether the homepage preference is equal to the new tab page
     * URL (hpisntp).
     *
     * m_hp m_ntp ntp hpisntp| choice value| choice enabled| URL field enabled
     * ------------------------------------------------------------------------
     * 0    0     0   0      | homepage    | 1             | 1
     * 0    0     0   1      | new tab page| 1             | 0
     * 0    0     1   0      | new tab page| 1             | 0
     * 0    0     1   1      | new tab page| 1             | 0
     * 0    1     0   0      | homepage    | 0             | 1
     * 0    1     0   1      | homepage    | 0             | 1
     * 0    1     1   0      | new tab page| 0             | 0
     * 0    1     1   1      | new tab page| 0             | 0
     * 1    0     0   0      | homepage    | 1             | 0
     * 1    0     0   1      | new tab page| 0             | 0
     * 1    0     1   0      | new tab page| 1             | 0
     * 1    0     1   1      | new tab page| 0             | 0
     * 1    1     0   0      | homepage    | 0             | 0
     * 1    1     0   1      | new tab page| 0             | 0
     * 1    1     1   0      | new tab page| 0             | 0
     * 1    1     1   1      | new tab page| 0             | 0
     *
     * thus, we have:
     *
     *    choice value is new tab page === ntp || (hpisntp && (m_hp || !m_ntp))
     *    choice enabled === !m_ntp && !(m_hp && hpisntp)
     *    URL field enabled === !ntp && !mhp && !(hpisntp && !m_ntp)
     *
     * which also make sense if you think about them.
     * @private
     */
    updateHomepageControlStates_: function() {
      var homepageField = $('homepageURL');
      homepageField.disabled = !this.isHomepageURLFieldEnabled_();
      homepageField.value = this.homepage_pref_.value;
      homepageField.style.backgroundImage = url('chrome://favicon/' +
                                                this.homepage_pref_.value);
      var disableChoice = !this.isHomepageChoiceEnabled_();
      $('homepageUseURLButton').disabled = disableChoice;
      $('homepageUseNTPButton').disabled = disableChoice;
      var useNewTabPage = this.isHomepageIsNewTabPageChoiceSelected_();
      $('homepageUseNTPButton').checked = useNewTabPage;
      $('homepageUseURLButton').checked = !useNewTabPage;
    },

    /**
     * Tests whether the value of the 'homepage' preference equls the new tab
     * page url (chrome://newtab).
     * @private
     * @returns {boolean} True if the 'homepage' value equals the new tab page
     *     url.
     */
    isHomepageURLNewTabPageURL_ : function() {
      return (this.homepage_pref_.value.toLowerCase() == 'chrome://newtab');
    },

    /**
     * Tests whether the Homepage choice "Use New Tab Page" is selected.
     * @private
     * @returns {boolean} True if "Use New Tab Page" is selected.
     */
    isHomepageIsNewTabPageChoiceSelected_: function() {
      return (this.homepage_is_newtabpage_pref_.value ||
              (this.isHomepageURLNewTabPageURL_() &&
               (this.homepage_pref_.managed ||
                !this.homepage_is_newtabpage_pref_.managed)));
    },

    /**
     * Tests whether the home page choice controls are enabled.
     * @private
     * @returns {boolean} True if the home page choice controls are enabled.
     */
    isHomepageChoiceEnabled_: function() {
      return (!this.homepage_is_newtabpage_pref_.managed &&
              !(this.homepage_pref_.managed &&
                this.isHomepageURLNewTabPageURL_()));
    },

    /**
     * Checks whether the home page field should be enabled.
     * @private
     * @returns {boolean} True if the home page field should be enabled.
     */
    isHomepageURLFieldEnabled_: function() {
      return (!this.homepage_is_newtabpage_pref_.value &&
              !this.homepage_pref_.managed &&
              !(this.isHomepageURLNewTabPageURL_() &&
                !this.homepage_is_newtabpage_pref_.managed));
    },

    /**
     * Sets the enabled state of the custom startup page list controls
     * based on the current startup radio button selection.
     * @private
     */
    updateCustomStartupPageControlStates_: function() {
      var disable = !this.shouldEnableCustomStartupPageControls_();
      $('startupAddButton').disabled = disable;
      $('startupUseCurrentButton').disabled = disable;
      this.updateRemoveButtonState_();
    },

    /**
     * Sets the enabled state of the startup page Remove button based on
     * the current selection in the startup pages list.
     * @private
     */
    updateRemoveButtonState_: function() {
      var groupEnabled = this.shouldEnableCustomStartupPageControls_();
      $('startupRemoveButton').disabled = !groupEnabled ||
          ($('startupPages').selectionModel.selectedIndex == -1);
    },

    /**
     * Removes the selected startup pages.
     * @private
     */
    removeSelectedStartupPages_: function() {
      var selections =
          $('startupPages').selectionModel.selectedIndexes.map(String);
      chrome.send('removeStartupPages', selections);
    },

    /**
     * Adds the given startup page at the current selection point.
     * @private
     */
    addStartupPage_: function(url) {
      var firstSelection = $('startupPages').selectionModel.selectedIndex;
      chrome.send('addStartupPage', [url, String(firstSelection)]);
    },

    /**
     * Set the default search engine based on the popup selection.
     */
    setDefaultSearchEngine: function() {
      var engineSelect = $('defaultSearchEngine');
      var selectedIndex = engineSelect.selectedIndex;
      if (selectedIndex >= 0) {
        var selection = engineSelect.options[selectedIndex];
        chrome.send('setDefaultSearchEngine', [String(selection.value)]);
      }
    },
  };

  BrowserOptions.updateDefaultBrowserState = function(statusString, isDefault,
                                                      canBeDefault) {
    if (!cr.isChromeOS) {
      BrowserOptions.getInstance().updateDefaultBrowserState_(statusString,
                                                              isDefault,
                                                              canBeDefault);
    }
  };

  BrowserOptions.updateSearchEngines = function(engines, defaultValue) {
    BrowserOptions.getInstance().updateSearchEngines_(engines, defaultValue);
  };

  BrowserOptions.updateStartupPages = function(pages) {
    BrowserOptions.getInstance().updateStartupPages_(pages);
  };

  BrowserOptions.addStartupPage = function(url) {
    BrowserOptions.getInstance().addStartupPage_(url);
  };

  // Export
  return {
    BrowserOptions: BrowserOptions
  };

});
