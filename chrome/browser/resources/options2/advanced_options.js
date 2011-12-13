// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

var OptionsPage = options.OptionsPage;

  //
  // AdvancedOptions class
  // Encapsulated handling of advanced options page.
  //
  function AdvancedOptions() {
    OptionsPage.call(this, 'advanced', templateData.advancedPageTabTitle,
                     'advancedPage');
  }

  cr.addSingletonGetter(AdvancedOptions);

  AdvancedOptions.prototype = {
    // Inherit AdvancedOptions from OptionsPage.
    __proto__: options.OptionsPage.prototype,

    /**
     * Initializes the page.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      // Set up click handlers for buttons.
      $('privacyContentSettingsButton').onclick = function(event) {
        OptionsPage.navigateToPage('content');
        OptionsPage.showTab($('cookies-nav-tab'));
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ContentSettings']);
      };
      $('privacyClearDataButton').onclick = function(event) {
        OptionsPage.navigateToPage('clearBrowserData');
        chrome.send('coreOptionsUserMetricsAction', ['Options_ClearData']);
      };

      // 'metricsReportingEnabled' element is only present on Chrome branded
      // builds.
      if ($('metricsReportingEnabled')) {
        $('metricsReportingEnabled').onclick = function(event) {
          chrome.send('metricsReportingCheckboxAction',
              [String(event.target.checked)]);
        };
      }

      if (!cr.isChromeOS) {
        $('autoOpenFileTypesResetToDefault').onclick = function(event) {
          chrome.send('autoOpenFileTypesAction');
        };
      }

      $('fontSettingsCustomizeFontsButton').onclick = function(event) {
        OptionsPage.navigateToPage('fonts');
        chrome.send('coreOptionsUserMetricsAction', ['Options_FontSettings']);
      };
      $('defaultFontSize').onchange = function(event) {
        chrome.send('defaultFontSizeAction',
            [String(event.target.options[event.target.selectedIndex].value)]);
      };
      $('defaultZoomFactor').onchange = function(event) {
        chrome.send('defaultZoomFactorAction',
            [String(event.target.options[event.target.selectedIndex].value)]);
      };

      $('language-button').onclick = function(event) {
        OptionsPage.navigateToPage('languages');
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_LanuageAndSpellCheckSettings']);
      };

      if (cr.isWindows || cr.isMac) {
        $('certificatesManageButton').onclick = function(event) {
          chrome.send('showManageSSLCertificates');
        };
      } else {
        $('certificatesManageButton').onclick = function(event) {
          OptionsPage.navigateToPage('certificates');
          chrome.send('coreOptionsUserMetricsAction',
                      ['Options_ManageSSLCertificates']);
        };
      }

      if (!cr.isChromeOS) {
        $('proxiesConfigureButton').onclick = function(event) {
          chrome.send('showNetworkProxySettings');
        };
        $('downloadLocationChangeButton').onclick = function(event) {
          chrome.send('selectDownloadLocation');
        };
        // This text field is always disabled. Setting ".disabled = true" isn't
        // enough, since a policy can disable it but shouldn't re-enable when
        // it is removed.
        $('downloadLocationPath').setDisabled('readonly', true);
      }

      $('sslCheckRevocation').onclick = function(event) {
        chrome.send('checkRevocationCheckboxAction',
            [String($('sslCheckRevocation').checked)]);
      };

      if ($('backgroundModeCheckbox')) {
        $('backgroundModeCheckbox').onclick = function(event) {
          chrome.send('backgroundModeAction',
              [String($('backgroundModeCheckbox').checked)]);
        };
      }

      // 'cloudPrintProxyEnabled' is true for Chrome branded builds on
      // certain platforms, or could be enabled by a lab.
      if (!cr.isChromeOS) {
        $('cloudPrintProxySetupButton').onclick = function(event) {
          if ($('cloudPrintProxyManageButton').style.display == 'none') {
            // Disable the button, set it's text to the intermediate state.
            $('cloudPrintProxySetupButton').textContent =
              localStrings.getString('cloudPrintProxyEnablingButton');
            $('cloudPrintProxySetupButton').disabled = true;
            chrome.send('showCloudPrintSetupDialog');
          } else {
            chrome.send('disableCloudPrintProxy');
          }
        };
      }
      $('cloudPrintProxyManageButton').onclick = function(event) {
        chrome.send('showCloudPrintManagePage');
      };

  }
  };

  //
  // Chrome callbacks
  //

  // Set the checked state of the metrics reporting checkbox.
  AdvancedOptions.SetMetricsReportingCheckboxState = function(
      checked, disabled) {
    $('metricsReportingEnabled').checked = checked;
    $('metricsReportingEnabled').disabled = disabled;
    if (disabled)
      $('metricsReportingEnabledText').className = 'disable-services-span';
  }

  AdvancedOptions.SetMetricsReportingSettingVisibility = function(visible) {
    if (visible) {
      $('metricsReportingSetting').style.display = 'block';
    } else {
      $('metricsReportingSetting').style.display = 'none';
    }
  }

  // Set the font size selected item.
  AdvancedOptions.SetFontSize = function(font_size_value) {
    var selectCtl = $('defaultFontSize');
    for (var i = 0; i < selectCtl.options.length; i++) {
      if (selectCtl.options[i].value == font_size_value) {
        selectCtl.selectedIndex = i;
        if ($('Custom'))
          selectCtl.remove($('Custom').index);
        return;
      }
    }

    // Add/Select Custom Option in the font size label list.
    if (!$('Custom')) {
      var option = new Option(localStrings.getString('fontSizeLabelCustom'),
                              -1, false, true);
      option.setAttribute("id", "Custom");
      selectCtl.add(option);
    }
    $('Custom').selected = true;
  };

  /**
    * Populate the page zoom selector with values received from the caller.
    * @param {Array} items An array of items to populate the selector.
    *     each object is an array with three elements as follows:
    *       0: The title of the item (string).
    *       1: The value of the item (number).
    *       2: Whether the item should be selected (boolean).
    */
  AdvancedOptions.SetupPageZoomSelector = function(items) {
    var element = $('defaultZoomFactor');

    // Remove any existing content.
    element.textContent = '';

    // Insert new child nodes into select element.
    var value, title, selected;
    for (var i = 0; i < items.length; i++) {
      title = items[i][0];
      value = items[i][1];
      selected = items[i][2];
      element.appendChild(new Option(title, value, false, selected));
    }
  };

  // Set the enabled state for the autoOpenFileTypesResetToDefault button.
  AdvancedOptions.SetAutoOpenFileTypesDisabledAttribute = function(disabled) {
    if (!cr.isChromeOS) {
      $('autoOpenFileTypesResetToDefault').disabled = disabled;

      if (disabled)
        $('auto-open-file-types-label').classList.add('disabled');
      else
        $('auto-open-file-types-label').classList.remove('disabled');
    }
  };

  // Set the enabled state for the proxy settings button.
  AdvancedOptions.SetupProxySettingsSection = function(disabled, label) {
    if (!cr.isChromeOS) {
      $('proxiesConfigureButton').disabled = disabled;
      $('proxiesLabel').textContent = label;
    }
  };

  // Set the checked state for the sslCheckRevocation checkbox.
  AdvancedOptions.SetCheckRevocationCheckboxState = function(
      checked, disabled) {
    $('sslCheckRevocation').checked = checked;
    $('sslCheckRevocation').disabled = disabled;
  };

  // Set the checked state for the backgroundModeCheckbox element.
  AdvancedOptions.SetBackgroundModeCheckboxState = function(checked) {
    $('backgroundModeCheckbox').checked = checked;
  };

  // Set the Cloud Print proxy UI to enabled, disabled, or processing.
  AdvancedOptions.SetupCloudPrintProxySection = function(
        disabled, label, allowed) {
    if (!cr.isChromeOS) {
      $('cloudPrintProxyLabel').textContent = label;
      if (disabled || !allowed) {
        $('cloudPrintProxySetupButton').textContent =
          localStrings.getString('cloudPrintProxyDisabledButton');
        $('cloudPrintProxyManageButton').style.display = 'none';
      } else {
        $('cloudPrintProxySetupButton').textContent =
          localStrings.getString('cloudPrintProxyEnabledButton');
        $('cloudPrintProxyManageButton').style.display = 'inline';
      }
      $('cloudPrintProxySetupButton').disabled = !allowed;
    }
  };

  AdvancedOptions.RemoveCloudPrintProxySection = function() {
    if (!cr.isChromeOS) {
      var proxySectionElm = $('cloud-print-proxy-section');
      if (proxySectionElm)
        proxySectionElm.parentNode.removeChild(proxySectionElm);
    }
  };

  // Export
  return {
    AdvancedOptions: AdvancedOptions
  };

});
