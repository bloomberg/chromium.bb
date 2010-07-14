// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// AdvancedOptions class
// Encapsulated handling of advanced options page.
//
function AdvancedOptions(model) {
  OptionsPage.call(this, 'advanced', templateData.advancedPage, 'advancedPage');
}

AdvancedOptions.getInstance = function() {
  if (!AdvancedOptions.instance_) {
    AdvancedOptions.instance_ = new AdvancedOptions(null);
  }
  return AdvancedOptions.instance_;
}

AdvancedOptions.prototype = {
  // Inherit AdvancedOptions from OptionsPage.
  __proto__: OptionsPage.prototype,

  // Initialize AdvancedOptions page.
  initializePage: function() {
    // Call base class implementation to starts preference initialization.
    OptionsPage.prototype.initializePage.call(this);

    // Setup function handlers for buttons.
    $('privacyContentSettingsButton').onclick = function(event) {
      OptionsPage.showPageByName('content');
    };
    $('privacyClearDataButton').onclick = function(event) {
      // TODO(csilv): spawn clear data overlay dialog.
    };
    $('proxiesConfigureButton').onclick = function(event) {
      if (cr.isMac) {
        chrome.send('showNetworkProxySettings');
      } else {
        // TODO(csilv): spawn network proxy settings sub-dialog.
      }
    };
    $('downloadLocationBrowseButton').onclick = function(event) {
      // TODO(csilv): spawn OS native file prompt dialog.
    };
    $('autoOpenFileTypesResetToDefault').onclick = function(event) {
      // TODO(csilv): do whatever this button must do...?
    };
    $('fontSettingsConfigureFontsOnlyButton').onclick = function(event) {
      // TODO(csilv): spawn font settings sub-dialog.
    };
    $('certificatesManageButton').onclick = function(event) {
      if (cr.isMac) {
        chrome.send('showManageSSLCertificates');
      } else {
        // TODO(csilv): spawn manage SSL certs sub-dialog.
      }
    };

    // Hide tabsToLinks checkbox on non-Mac platforms.
    if (!cr.isMac) {
      $('tabsToLinksCheckbox').style.display = 'none';
    }
  },
};

