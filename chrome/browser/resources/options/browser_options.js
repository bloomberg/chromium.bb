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

      // Initialize control enabled states.
      Preferences.getInstance().addEventListener('session.restore_on_startup',
          this.updateCustomStartupPageControlStates_.bind(this));
      Preferences.getInstance().addEventListener('homepage_is_newtabpage',
          this.updateHomepageFieldState_.bind(this));
      this.updateCustomStartupPageControlStates_();
      this.updateHomepageFieldState_();

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
     */
    updateDefaultBrowserState_: function(statusString, isDefault) {
      var label = $('defaultBrowserState');
      label.textContent = statusString;
      if (isDefault) {
        label.classList.add('current');
      } else {
        label.classList.remove('current');
      }

      $('defaultBrowserUseAsDefaultButton').disabled = isDefault;
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
     * Sets the enabled state of the homepage field based on the current
     * homepage radio button selection.
     * @private
     */
    updateHomepageFieldState_: function() {
      $('homepageURL').disabled = !$('homepageUseURLButton').checked;
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

  BrowserOptions.updateDefaultBrowserState = function(statusString, isDefault) {
    if (!cr.isChromeOS) {
      BrowserOptions.getInstance().updateDefaultBrowserState_(statusString,
                                                              isDefault);
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

