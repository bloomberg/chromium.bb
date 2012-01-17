// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
     * Flag indicating if currently scanning for Bluetooth devices.
     * @type {boolean}
     */
    isScanning_: false,

    /**
     * Initializes the page.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      // Date and time section (CrOS only).
      if (cr.isChromeOS && AccountsOptions.loggedInAsGuest()) {
        // Disable time-related settings if we're not logged in as a real user.
        $('timezone-select').disabled = true;
        $('use-24hour-clock').disabled = true;
      }

      // Privacy section.
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

      // Bluetooth (CrOS only).
      if (cr.isChromeOS) {
        options.system.bluetooth.BluetoothDeviceList.decorate(
            $('bluetooth-paired-devices-list'));

        $('bluetooth-add-device').onclick = function(event) {
          if (!this.isScanning_)
            findBluetoothDevices_(true);
          OptionsPage.navigateToPage('bluetooth');
        };
        $('enable-bluetooth').onclick = function(event) {
          chrome.send('bluetoothEnableChange', [Boolean(true)]);
        };
        $('disable-bluetooth').onclick = function(event) {
          chrome.send('bluetoothEnableChange', [Boolean(false)]);
        };
      }

      // Passwords and Forms section.
      $('autofill-settings').onclick = function(event) {
        OptionsPage.navigateToPage('autofill');
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowAutofillSettings']);
      };
      $('manage-passwords').onclick = function(event) {
        OptionsPage.navigateToPage('passwords');
        OptionsPage.showTab($('passwords-nav-tab'));
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowPasswordManager']);
      };
      if (AdvancedOptions.GuestModeActive()) {
        // Disable and turn off Autofill in guest mode.
        var autofillEnabled = $('autofill-enabled');
        autofillEnabled.disabled = true;
        autofillEnabled.checked = false;
        cr.dispatchSimpleEvent(autofillEnabled, 'change');
        $('autofill-settings').disabled = true;

        // Disable and turn off Password Manager in guest mode.
        var passwordManagerEnabled = $('password-manager-enabled');
        passwordManagerEnabled.disabled = true;
        passwordManagerEnabled.checked = false;
        cr.dispatchSimpleEvent(passwordManagerEnabled, 'change');
        $('manage-passwords').disabled = true;

        // Hide the entire section on ChromeOS
        if (cr.isChromeOS)
          $('passwords-and-autofill-section').hidden = true;
      }
      $('mac-passwords-warning').hidden =
          !(localStrings.getString('macPasswordsWarning'));

      // Network section.
      if (!cr.isChromeOS) {
        $('proxiesConfigureButton').onclick = function(event) {
          chrome.send('showNetworkProxySettings');
        };
      }

      // Web Content section.
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

      // Languages section.
      $('language-button').onclick = function(event) {
        OptionsPage.navigateToPage('languages');
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_LanuageAndSpellCheckSettings']);
      };

      // Downloads section.
      if (!cr.isChromeOS) {
        $('downloadLocationChangeButton').onclick = function(event) {
          chrome.send('selectDownloadLocation');
        };
        // This text field is always disabled. Setting ".disabled = true" isn't
        // enough, since a policy can disable it but shouldn't re-enable when
        // it is removed.
        $('downloadLocationPath').setDisabled('readonly', true);
        $('autoOpenFileTypesResetToDefault').onclick = function(event) {
          chrome.send('autoOpenFileTypesAction');
        };
      }

      // HTTPS/SSL section.
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
      $('sslCheckRevocation').onclick = function(event) {
        chrome.send('checkRevocationCheckboxAction',
            [String($('sslCheckRevocation').checked)]);
      };

      // Cloud Print section.
      // 'cloudPrintProxyEnabled' is true for Chrome branded builds on
      // certain platforms, or could be enabled by a lab.
      if (!cr.isChromeOS) {
        $('cloudPrintConnectorSetupButton').onclick = function(event) {
          if ($('cloudPrintManageButton').style.display == 'none') {
            // Disable the button, set it's text to the intermediate state.
            $('cloudPrintConnectorSetupButton').textContent =
              localStrings.getString('cloudPrintConnectorEnablingButton');
            $('cloudPrintConnectorSetupButton').disabled = true;
            chrome.send('showCloudPrintSetupDialog');
          } else {
            chrome.send('disableCloudPrintConnector');
          }
        };
      }
      $('cloudPrintManageButton').onclick = function(event) {
        chrome.send('showCloudPrintManagePage');
      };

      // Accessibility section (CrOS only).
      if (cr.isChromeOS) {
        $('accessibility-spoken-feedback-check').onchange = function(event) {
          chrome.send('spokenFeedbackChange',
          [$('accessibility-spoken-feedback-check').checked]);
        };
        $('accessibility-high-contrast-check').onchange = function(event) {
          chrome.send('highContrastChange',
          [$('accessibility-high-contrast-check').checked]);
        };
        $('accessibility-screen-magnifier-check').onchange = function(event) {
          chrome.send('screenMagnifierChange',
          [$('accessibility-screen-magnifier-check').checked]);
        };
        $('accessibility-virtual-keyboard-check').onchange = function(event) {
          chrome.send('virtualKeyboardChange',
          [$('accessibility-virtual-keyboard-check').checked]);
        };
      }

      // Background mode section.
      if ($('backgroundModeCheckbox')) {
        $('backgroundModeCheckbox').onclick = function(event) {
          chrome.send('backgroundModeAction',
              [String($('backgroundModeCheckbox').checked)]);
        };
      }
    }
  };

  /**
   * Scan for bluetooth devices.
   * @param {boolean} reset Indicates if the list of unpaired devices should be
   *     cleared.
   * @private
   */
  function findBluetoothDevices_(reset) {
    this.isScanning_ = true;
    if (reset)
      $('bluetooth-unpaired-devices-list').clear();
    chrome.send('findBluetoothDevices');
  }

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

  /**
   * Returns whether the browser in guest mode. Some features are disabled or
   * hidden in guest mode.
   * @return {boolean} True if guest mode is currently active.
   */
  AdvancedOptions.GuestModeActive = function() {
    return cr.commandLine && cr.commandLine.options['--bwsi'];
  };

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
  AdvancedOptions.SetupCloudPrintConnectorSection = function(
        disabled, label, allowed) {
    if (!cr.isChromeOS) {
      $('cloudPrintConnectorLabel').textContent = label;
      if (disabled || !allowed) {
        $('cloudPrintConnectorSetupButton').textContent =
          localStrings.getString('cloudPrintConnectorDisabledButton');
        $('cloudPrintManageButton').style.display = 'none';
      } else {
        $('cloudPrintConnectorSetupButton').textContent =
          localStrings.getString('cloudPrintConnectorEnabledButton');
        $('cloudPrintManageButton').style.display = 'inline';
      }
      $('cloudPrintConnectorSetupButton').disabled = !allowed;
    }
  };

  AdvancedOptions.RemoveCloudPrintConnectorSection = function() {
    if (!cr.isChromeOS) {
      var connectorSectionElm = $('cloud-print-connector-section');
      if (connectorSectionElm)
        connectorSectionElm.parentNode.removeChild(connectorSectionElm);
    }
  };

  /**
   * Set the initial state of the spoken feedback checkbox.
   */
  AdvancedOptions.setSpokenFeedbackCheckboxState = function(checked) {
    $('accessibility-spoken-feedback-check').checked = checked;
  };

  /**
   * Set the initial state of the high contrast checkbox.
   */
  AdvancedOptions.setHighContrastCheckboxState = function(checked) {
    $('accessibility-high-contrast-check').checked = checked;
  };

  /**
   * Set the initial state of the screen magnifier checkbox.
   */
  AdvancedOptions.setScreenMagnifierCheckboxState = function(checked) {
    $('accessibility-screen-magnifier-check').checked = checked;
  };

  /**
   * Set the initial state of the virtual keyboard checkbox.
   */
  AdvancedOptions.setVirtualKeyboardCheckboxState = function(checked) {
    $('accessibility-virtual-keyboard-check').checked = checked;
  };

  /**
   * Activate the bluetooth settings section on the System settings page.
   */
  AdvancedOptions.showBluetoothSettings = function() {
    $('bluetooth-devices').hidden = false;
  };

  /**
   * Sets the state of the checkbox indicating if bluetooth is turned on. The
   * state of the "Find devices" button and the list of discovered devices may
   * also be affected by a change to the state.
   * @param {boolean} checked Flag Indicating if Bluetooth is turned on.
   */
  AdvancedOptions.setBluetoothState = function(checked) {
    $('disable-bluetooth').hidden = !checked;
    $('enable-bluetooth').hidden = checked;
    $('bluetooth-paired-devices-list').parentNode.hidden = !checked;
    $('bluetooth-add-device').hidden = !checked;
    // Flush list of previously discovered devices if bluetooth is turned off.
    if (!checked) {
      $('bluetooth-paired-devices-list').clear();
      $('bluetooth-unpaired-devices-list').clear();
    }
    if (checked && ! this.isScanning_)
      findBluetoothDevices_(true);
  }

  /**
   * Adds an element to the list of available bluetooth devices. If an element
   * with a matching address is found, the existing element is updated.
   * @param {{name: string,
   *          address: string,
   *          icon: string,
   *          paired: boolean,
   *          connected: boolean}} device
   *     Decription of the bluetooth device.
   */
  AdvancedOptions.addBluetoothDevice = function(device) {
    if (device.paired) {
      // Test to see if the device is currently in the unpaired list, in which
      // case it should be removed from that list.
      var index = $('bluetooth-unpaired-devices-list').find(device.address);
      if (index != undefined)
        $('bluetooth-unpaired-devices-list').deleteItemAtIndex(index);
      $('bluetooth-paired-devices-list').appendDevice(device);
    } else {
      $('bluetooth-unpaired-devices-list').appendDevice(device);
    }
    // One device can be in the process of pairing.  If found, display
    // the Bluetooth pairing overlay.
    if (device.pairing)
      BluetoothPairing.showDialog(device);
  };

  /**
   * Notification that a single pass of device discovery has completed.
   */
  AdvancedOptions.notifyBluetoothSearchComplete = function() {
    // TODO(kevers): Determine the fate of this method once continuous
    // scanning is implemented in the Bluetooth code.
    this.isScanning_ = false;
  };

  // Export
  return {
    AdvancedOptions: AdvancedOptions
  };

});
