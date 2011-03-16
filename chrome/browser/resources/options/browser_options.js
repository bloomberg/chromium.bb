// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;
  const ArrayDataModel = cr.ui.ArrayDataModel;

  //
  // BrowserOptions class
  // Encapsulated handling of browser options page.
  //
  function BrowserOptions() {
    OptionsPage.call(this, 'browser',
                     templateData.browserPageTabTitle,
                     'browserPage');
  }

  cr.addSingletonGetter(BrowserOptions);

  BrowserOptions.prototype = {
    // Inherit BrowserOptions from OptionsPage.
    __proto__: options.OptionsPage.prototype,

    startup_pages_pref_: {
      'name': 'session.urls_to_restore_on_startup',
      'managed': false
    },

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
     * At autocomplete list that can be attached to a text field during editing.
     * @type {HTMLElement}
     * @private
     */
    autocompleteList_: null,

    // The cached value of the instant.confirm_dialog_shown preference.
    instantConfirmDialogShown_: false,

    /**
     * Initialize BrowserOptions page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      // Wire up controls.
      $('startupUseCurrentButton').onclick = function(event) {
        chrome.send('setStartupPagesToCurrentPages');
      };
      $('startupAddButton').onclick = function(event) {
        OptionsPage.navigateToPage('addStartupPage');
      };
      $('defaultSearchManageEnginesButton').onclick = function(event) {
        OptionsPage.navigateToPage('searchEngines');
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ManageSearchEngines']);
      };
      $('defaultSearchEngine').onchange = this.setDefaultSearchEngine;

      var self = this;
      $('instantEnableCheckbox').onclick = function(event) {
        if (this.checked && !self.instantConfirmDialogShown_) {
          // Leave disabled for now. The PrefCheckbox handler already set it to
          // true so undo that.
          Preferences.setBooleanPref(this.pref, false, this.metric);
          OptionsPage.navigateToPage('instantConfirm');
        }
      };

      Preferences.getInstance().addEventListener('instant.confirm_dialog_shown',
          this.onInstantConfirmDialogShownChanged_.bind(this));

      var homepageField = $('homepageURL');
      $('homepageUseNTPButton').onchange =
          this.handleHomepageUseNTPButtonChange_.bind(this);
      $('homepageUseURLButton').onchange =
          this.handleHomepageUseURLButtonChange_.bind(this);
      homepageField.onchange = this.handleHomepageURLChange_.bind(this);
      homepageField.oninput = this.handleHomepageURLChange_.bind(this);
      homepageField.addEventListener('focus', function(event) {
        self.autocompleteList_.attachToInput(homepageField);
      });
      homepageField.addEventListener('blur', function(event) {
        self.autocompleteList_.detach();
      });

      // Ensure that changes are committed when closing the page.
      window.addEventListener('unload', function() {
        if (document.activeElement == homepageField)
          homepageField.blur();
      });

      // Remove Windows-style accelerators from button labels.
      // TODO(stuartmorgan): Remove this once the strings are updated.
      $('startupAddButton').textContent =
          localStrings.getStringWithoutAccelerator('startupAddButton');

      if (!cr.isChromeOS) {
        $('defaultBrowserUseAsDefaultButton').onclick = function(event) {
          chrome.send('becomeDefaultBrowser');
        };
      }

      var list = $('startupPagesList');
      options.browser_options.StartupPageList.decorate(list);
      list.autoExpands = true;

      // Check if we are in the guest mode.
      if (cr.commandLine.options['--bwsi']) {
        // Hide the startup section.
        $('startupSection').classList.add('hidden');
      } else {
        // Initialize control enabled states.
        Preferences.getInstance().addEventListener('session.restore_on_startup',
            this.updateCustomStartupPageControlStates_.bind(this));
        Preferences.getInstance().addEventListener(
            this.startup_pages_pref_.name,
            this.handleStartupPageListChange_.bind(this));
        Preferences.getInstance().addEventListener(
            this.homepage_pref_.name,
            this.handleHomepageChange_.bind(this));
        Preferences.getInstance().addEventListener(
            this.homepage_is_newtabpage_pref_.name,
            this.handleHomepageIsNewTabPageChange_.bind(this));

        this.updateCustomStartupPageControlStates_();
      }

      var suggestionList = new options.AutocompleteList();
      suggestionList.autoExpands = true;
      suggestionList.addEventListener('change', function(e) {
        if (!suggestionList.targetInput || !suggestionList.selectedItem)
          return;
        suggestionList.targetInput.value = suggestionList.selectedItem['url'];
      });
      this.autocompleteList_ = suggestionList;
    },

    /**
     * Called when the value of the instant.confirm_dialog_shown preference
     * changes. Cache this value.
     * @param {Event} event Change event.
     * @private
     */
    onInstantConfirmDialogShownChanged_: function(event) {
      this.instantConfirmDialogShown_ = event.value['value'];
    },

    /**
     * Update the Default Browsers section based on the current state.
     * @param {string} statusString Description of the current default state.
     * @param {boolean} isDefault Whether or not the browser is currently
     *     default.
     * @param {boolean} canBeDefault Whether or not the browser can be default.
     * @private
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
     * @param {number} defaultValue The value of the current default engine.
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
     * @returns {boolean} Whether the startup page controls should be
     *     enabled.
     */
    shouldEnableCustomStartupPageControls: function(pages) {
      return $('startupShowPagesButton').checked &&
          !this.startup_pages_pref_.managed;
    },

    /**
     * Updates the startup pages list with the given entries.
     * @param {Array} pages List of startup pages.
     * @private
     */
    updateStartupPages_: function(pages) {
      $('startupPagesList').dataModel = new ArrayDataModel(pages);
    },

    /**
     * Handles change events of the radio button 'homepageUseURLButton'.
     * @param {event} change event.
     * @private
     */
    handleHomepageUseURLButtonChange_: function(event) {
      Preferences.setBooleanPref(this.homepage_is_newtabpage_pref_.name, false);
    },

    /**
     * Handles change events of the radio button 'homepageUseNTPButton'.
     * @param {event} change event.
     * @private
     */
    handleHomepageUseNTPButtonChange_: function(event) {
      Preferences.setBooleanPref(this.homepage_is_newtabpage_pref_.name, true);
    },

    /**
     * Handles input and change events of the text field 'homepageURL'.
     * @param {event} input/change event.
     * @private
     */
    handleHomepageURLChange_: function(event) {
      var homepageField = $('homepageURL');
      var doFixup = event.type == 'change' ? '1' : '0';
      chrome.send('setHomePage', [homepageField.value, doFixup]);

      if (!this.autocompleteList_.targetInput)
        this.autocompleteList_.attachToInput(homepageField);
      chrome.send('requestAutocompleteSuggestions', [homepageField.value]);
    },

    /**
     * Handle change events of the preference 'homepage'.
     * @param {event} preference changed event.
     * @private
     */
    handleHomepageChange_: function(event) {
      this.homepage_pref_.value = event.value['value'];
      this.homepage_pref_.managed = event.value['managed'];
      if (this.isHomepageURLNewTabPageURL_() && !this.homepage_pref_.managed &&
          !this.homepage_is_newtabpage_pref_.managed) {
        var useNewTabPage = this.isHomepageIsNewTabPageChoiceSelected_();
        Preferences.setStringPref(this.homepage_pref_.name, '')
        Preferences.setBooleanPref(this.homepage_is_newtabpage_pref_.name,
                                   useNewTabPage)
      }
      this.updateHomepageControlStates_();
    },

    /**
     * Handle change events of the preference homepage_is_newtabpage.
     * @param {event} preference changed event.
     * @private
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
      if (homepageField.value != this.homepage_pref_.value)
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
     * @returns {boolean} True if the 'homepage' value equals the new tab page
     *     url.
     * @private
     */
    isHomepageURLNewTabPageURL_ : function() {
      return (this.homepage_pref_.value.toLowerCase() == 'chrome://newtab');
    },

    /**
     * Tests whether the Homepage choice "Use New Tab Page" is selected.
     * @returns {boolean} True if "Use New Tab Page" is selected.
     * @private
     */
    isHomepageIsNewTabPageChoiceSelected_: function() {
      return (this.homepage_is_newtabpage_pref_.value ||
              (this.isHomepageURLNewTabPageURL_() &&
               (this.homepage_pref_.managed ||
                !this.homepage_is_newtabpage_pref_.managed)));
    },

    /**
     * Tests whether the home page choice controls are enabled.
     * @returns {boolean} True if the home page choice controls are enabled.
     * @private
     */
    isHomepageChoiceEnabled_: function() {
      return (!this.homepage_is_newtabpage_pref_.managed &&
              !(this.homepage_pref_.managed &&
                this.isHomepageURLNewTabPageURL_()));
    },

    /**
     * Checks whether the home page field should be enabled.
     * @returns {boolean} True if the home page field should be enabled.
     * @private
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
      var disable = !this.shouldEnableCustomStartupPageControls();
      $('startupPagesList').disabled = disable;
      $('startupUseCurrentButton').disabled = disable;
      $('startupAddButton').disabled = disable;
    },

    /**
     * Handle change events of the preference
     * 'session.urls_to_restore_on_startup'.
     * @param {event} preference changed event.
     * @private
     */
    handleStartupPageListChange_: function(event) {
      this.startup_pages_pref_.managed = event.value['managed'];
      this.updateCustomStartupPageControlStates_();
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

    /**
     * Adds the given startup page at the current selection point.
     * @private
     */
    addStartupPage_: function(url) {
      var selectedIndex =
          $('startupPagesList').selectionModel.selectedIndex;
      chrome.send('addStartupPage', [url, String(selectedIndex)]);
    },

    /**
     * Updates the autocomplete suggestion list with the given entries.
     * @param {Array} pages List of autocomplete suggestions.
     * @private
     */
    updateAutocompleteSuggestions_: function(suggestions) {
      var list = this.autocompleteList_;
      // If the trigger for this update was a value being selected from the
      // current list, do nothing.
      if (list.targetInput && list.selectedItem &&
          list.selectedItem['url'] == list.targetInput.value)
        return;
      list.suggestions = suggestions;
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

  BrowserOptions.updateAutocompleteSuggestions = function(suggestions) {
    BrowserOptions.getInstance().updateAutocompleteSuggestions_(suggestions);
  };

  // Export
  return {
    BrowserOptions: BrowserOptions
  };

});
