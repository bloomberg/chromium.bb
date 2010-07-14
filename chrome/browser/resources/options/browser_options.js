// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// BrowserOptions class
// Encapsulated handling of browser options page.
//
function BrowserOptions(model) {
  OptionsPage.call(this, 'browser', templateData.browserPage, 'browserPage');
}

BrowserOptions.getInstance = function() {
  if (!BrowserOptions.instance_) {
    BrowserOptions.instance_ = new BrowserOptions(null);
  }
  return BrowserOptions.instance_;
}

BrowserOptions.prototype = {
  // Inherit BrowserOptions from OptionsPage.
  __proto__: OptionsPage.prototype,

  // Initialize BrowserOptions page.
  initializePage: function() {
    // Call base class implementation to start preference initialization.
    OptionsPage.prototype.initializePage.call(this);

    // Fetch initial state of the default browser pref section.
    chrome.send('updateDefaultBrowserState');

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
  },

  // Update the Default Browsers section based on the current state.
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
};

BrowserOptions.updateDefaultBrowserStateCallback = function(statusString,
                                                            isDefault) {
  BrowserOptions.getInstance().updateDefaultBrowserState_(statusString,
                                                          isDefault);
}

