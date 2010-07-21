// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  __proto__: OptionsPage.prototype,

  /**
   * Initialize BrowserOptions page.
   */
  initializePage: function() {
    // Call base class implementation to start preference initialization.
    OptionsPage.prototype.initializePage.call(this);

    // Wire up buttons.
    $('startupAddButton').onclick = function(event) {
      // TODO(stuartmorgan): Spawn add sub-dialog.
    };
    $('startupRemoveButton').onclick = function(event) {
      // TODO(stuartmorgan): Remove selected element(s).
    };
    $('startupUseCurrentButton').onclick = function(event) {
      // TODO(stuartmorgan): Add all open tabs (except this one).
    };
    $('defaultSearchManageEnginesButton').onclick = function(event) {
      // TODO(stuartmorgan): Spawn search engine management sub-dialog.
    };
    $('defaultBrowserUseAsDefaultButton').onclick = function(event) {
      chrome.send('becomeDefaultBrowser');
    };

    // Remove Windows-style accelerators from button labels.
    // TODO(stuartmorgan): Remove this once the strings are updated.
    $('startupAddButton').textContent =
        localStrings.getStringWithoutAccelerator('startupAddButton');
    $('startupRemoveButton').textContent =
        localStrings.getStringWithoutAccelerator('startupRemoveButton');
  },

  /**
   * Update the Default Browsers section based on the current state.
   * @param {String} statusString Description of the current default state.
   * @param {Boolean} isDefault Whether or not the browser is currently default.
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
    engineCount = engines.length
    var defaultIndex = -1;
    for (var i = 0; i < engineCount; i++) {
      var engine = engines[i]
      var option = new Option(engine['name'], engine['index']);
      if (defaultValue == option.value)
        defaultIndex = i;
      engineSelect.appendChild(option);
    }
    if (defaultIndex >= 0)
      engineSelect.selectedIndex = defaultIndex;
  },

  /**
   * Set the default search engine based on the popup selection.
   */
  setDefaultBrowser: function() {
    var engineSelect = $('defaultSearchEngine');
    var selectedIndex = engineSelect.selectedIndex;
    if (selectedIndex >= 0) {
      var selection = engineSelect.options[selectedIndex];
      chrome.send('setDefaultSearchEngine', [String(selection.value)]);
    }
  },
};

BrowserOptions.updateDefaultBrowserState = function(statusString, isDefault) {
  BrowserOptions.getInstance().updateDefaultBrowserState_(statusString,
                                                          isDefault);
}

BrowserOptions.updateSearchEngines = function(engines, defaultValue) {
  BrowserOptions.getInstance().updateSearchEngines_(engines, defaultValue);
}
