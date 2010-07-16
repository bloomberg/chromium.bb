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
      chrome.send('selectDownloadLocation');
    };
    $('autoOpenFileTypesResetToDefault').onclick = function(event) {
      chrome.send('autoOpenFileTypesAction');
    };
    $('fontSettingsConfigureFontsOnlyButton').onclick = function(event) {
      // TODO(csilv): spawn font settings sub-dialog.
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
