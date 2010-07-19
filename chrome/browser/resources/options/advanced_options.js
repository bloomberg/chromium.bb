// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// AdvancedOptions class
// Encapsulated handling of advanced options page.
//
function AdvancedOptions() {
  OptionsPage.call(this, 'advanced', templateData.advancedPage, 'advancedPage');
}

cr.addSingletonGetter(AdvancedOptions);

AdvancedOptions.prototype = {
  // Inherit AdvancedOptions from OptionsPage.
  __proto__: OptionsPage.prototype,

  // Initialize AdvancedOptions page.
  initializePage: function() {
    // Call base class implementation to starts preference initialization.
    OptionsPage.prototype.initializePage.call(this);

    // Setup click handlers for buttons.
    $('privacyContentSettingsButton').onclick = function(event) {
      OptionsPage.showPageByName('content');
    };
    $('privacyClearDataButton').onclick = function(event) {
      OptionsPage.showOverlay('clearBrowserDataOverlay');
    };
    $('proxiesConfigureButton').onclick = function(event) {
      chrome.send('showNetworkProxySettings');
    };
    $('downloadLocationBrowseButton').onclick = function(event) {
      chrome.send('selectDownloadLocation');
    };
    $('autoOpenFileTypesResetToDefault').onclick = function(event) {
      chrome.send('autoOpenFileTypesAction');
    };
    $('fontSettingsConfigureFontsOnlyButton').onclick = function(event) {
      OptionsPage.showOverlay('fontSettingsOverlay');
    };
    $('certificatesManageButton').onclick = function(event) {
      chrome.send('showManageSSLCertificates');
    };

    if (cr.isWindows) {
      $('sslCheckRevocation').onclick = function(event) {
        chrome.send('checkRevocationCheckboxAction',
            [String($('sslCheckRevocation').checked)]);
      };
      $('sslUseSSL2').onclick = function(event) {
        chrome.send('useSSL2CheckboxAction',
            [String($('sslUseSSL2').checked)]);
      };
    }

    // Remove Windows-style accelerators from the Browse button label.
    // TODO(csilv): Remove this after the accelerator has been removed from
    // the localized strings file, pending removal of old options window.
    $('downloadLocationBrowseButton').textContent =
        localStrings.getStringWithoutAccelerator(
            'downloadLocationBrowseButton');
  }
};

//
// Chrome callbacks
//

// Set the download path.
function advancedOptionsSetDownloadLocationPath(path) {
  $('downloadLocationPath').value = path;
}

// Set the enabled state for the autoOpenFileTypesResetToDefault button.
function advancedOptionsSetAutoOpenFileTypesDisabledAttribute(disabled) {
  $('autoOpenFileTypesResetToDefault').disabled = disabled;
}

// Set the checked state for the sslCheckRevocation checkbox.
function advancedOptionsSetCheckRevocationCheckboxState(checked) {
  $('sslCheckRevocation').checked = checked;
}

// Set the checked state for the sslUseSSL2 checkbox.
function advancedOptionsSetUseSSL2CheckboxState(checked) {
  $('sslUseSSL2').checked = checked;
}
