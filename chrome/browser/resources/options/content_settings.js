// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;

  // Lookup table to generate the i18n strings.
  /** @const */ var permissionsLookup = {
    'location': 'location',
    'notifications': 'notifications',
    'media-stream': 'mediaStream',
    'cookies': 'cookies',
    'multiple-automatic-downloads': 'multipleAutomaticDownloads',
    'images': 'images',
    'plugins': 'plugins',
    'popups': 'popups',
    'javascript': 'javascript'
  };

  //////////////////////////////////////////////////////////////////////////////
  // ContentSettings class:

  /**
   * Encapsulated handling of content settings page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function ContentSettings() {
    this.activeNavTab = null;
    Page.call(this, 'content',
              loadTimeData.getString('contentSettingsPageTabTitle'),
              'content-settings-page');
  }

  cr.addSingletonGetter(ContentSettings);

  ContentSettings.prototype = {
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      var exceptionsButtons =
          this.pageDiv.querySelectorAll('.exceptions-list-button');
      for (var i = 0; i < exceptionsButtons.length; i++) {
        exceptionsButtons[i].onclick = function(event) {
          var hash = event.currentTarget.getAttribute('contentType');
          PageManager.showPageByName('contentExceptions', true,
                                     {hash: '#' + hash});
        };
      }

      var experimentalExceptionsButtons =
          this.pageDiv.querySelectorAll('.website-settings-permission-button');
      for (var i = 0; i < experimentalExceptionsButtons.length; i++) {
        experimentalExceptionsButtons[i].onclick = function(event) {
          var hash = event.currentTarget.getAttribute('contentType');
          WebsiteSettingsManager.showWebsiteSettings(hash);
        };
      }

      var manageHandlersButton = $('manage-handlers-button');
      if (manageHandlersButton) {
        manageHandlersButton.onclick = function(event) {
          PageManager.showPageByName('handlers');
        };
      }

      if (cr.isChromeOS) {
        // Disable some controls for Guest in Chrome OS.
        UIAccountTweaks.applyGuestSessionVisibility(document);

        // Disable some controls for Public session in Chrome OS.
        UIAccountTweaks.applyPublicSessionVisibility(document);
      }

      // Cookies filter page ---------------------------------------------------
      $('show-cookies-button').onclick = function(event) {
        chrome.send('coreOptionsUserMetricsAction', ['Options_ShowCookies']);
        PageManager.showPageByName('cookies');
      };

      $('content-settings-overlay-confirm').onclick =
          PageManager.closeOverlay.bind(PageManager);

      $('media-pepper-flash-default').hidden = true;
      $('media-pepper-flash-exceptions').hidden = true;

      $('media-select-mic').addEventListener('change',
          ContentSettings.setDefaultMicrophone_);
      $('media-select-camera').addEventListener('change',
          ContentSettings.setDefaultCamera_);

      if (loadTimeData.getBoolean('websiteSettingsManagerEnabled')) {
        var oldUI =
            this.pageDiv.querySelectorAll('.replace-with-website-settings');
        for (var i = 0; i < oldUI.length; i++) {
          oldUI[i].hidden = true;
        }

        var newUI =
            this.pageDiv.querySelectorAll('.experimental-website-settings');
        for (var i = 0; i < newUI.length; i++) {
          newUI[i].hidden = false;
        }
      }
    },
  };

  ContentSettings.updateHandlersEnabledRadios = function(enabled) {
    var selector = '#content-settings-page input[type=radio][value=' +
        (enabled ? 'allow' : 'block') + '].handler-radio';
    document.querySelector(selector).checked = true;
  };

  /**
   * Sets the values for all the content settings radios and labels.
   * @param {Object} dict A mapping from radio groups to the checked value for
   *     that group.
   */
  ContentSettings.setContentFilterSettingsValue = function(dict) {
    for (var group in dict) {
      var settingLabel = $(group + '-default-string');
      if (settingLabel) {
        var value = dict[group].value;
        var valueId =
            permissionsLookup[group] + value[0].toUpperCase() + value.slice(1);
        settingLabel.textContent = loadTimeData.getString(valueId);
      }

      var managedBy = dict[group].managedBy;
      var controlledBy = managedBy == 'policy' || managedBy == 'extension' ?
          managedBy : null;
      document.querySelector('input[type=radio][name=' + group + '][value=' +
                             dict[group].value + ']').checked = true;
      var radios = document.querySelectorAll('input[type=radio][name=' +
                                             group + ']');
      for (var i = 0, len = radios.length; i < len; i++) {
        radios[i].disabled = (managedBy != 'default');
        radios[i].controlledBy = controlledBy;
      }
      var indicators = document.querySelectorAll(
          'span.controlled-setting-indicator[content-setting=' + group + ']');
      if (indicators.length == 0)
        continue;
      // Create a synthetic pref change event decorated as
      // CoreOptionsHandler::CreateValueForPref() does.
      var event = new Event(group);
      event.value = {
        value: dict[group].value,
        controlledBy: controlledBy,
      };
      for (var i = 0; i < indicators.length; i++) {
        indicators[i].handlePrefChange(event);
      }
    }
  };

  /**
   * Updates the labels and indicators for the Media settings. Those require
   * special handling because they are backed by multiple prefs and can change
   * their scope based on the managed state of the backing prefs.
   * @param {{askText: string, blockText: string, cameraDisabled: boolean,
   *          micDisabled: boolean, showBubble: boolean, bubbleText: string}}
   *     mediaSettings A dictionary containing the following fields:
   *     askText The label for the ask radio button.
   *     blockText The label for the block radio button.
   *     cameraDisabled Whether to disable the camera dropdown.
   *     micDisabled Whether to disable the microphone dropdown.
   *     showBubble Wether to show the managed icon and bubble for the media
   *                label.
   *     bubbleText The text to use inside the bubble if it is shown.
   */
  ContentSettings.updateMediaUI = function(mediaSettings) {
    $('media-stream-ask-label').innerHTML =
        loadTimeData.getString(mediaSettings.askText);
    $('media-stream-block-label').innerHTML =
        loadTimeData.getString(mediaSettings.blockText);

    if (mediaSettings.micDisabled)
      $('media-select-mic').disabled = true;
    if (mediaSettings.cameraDisabled)
      $('media-select-camera').disabled = true;

    PageManager.hideBubble();
    // Create a synthetic pref change event decorated as
    // CoreOptionsHandler::CreateValueForPref() does.
    // TODO(arv): It was not clear what event type this should use?
    var event = new Event('undefined');
    event.value = {};

    if (mediaSettings.showBubble) {
      event.value = { controlledBy: 'policy' };
      $('media-indicator').setAttribute(
          'textpolicy', loadTimeData.getString(mediaSettings.bubbleText));
      $('media-indicator').location = cr.ui.ArrowLocation.TOP_START;
    }

    $('media-indicator').handlePrefChange(event);
  };

  /**
   * Initializes an exceptions list.
   * @param {string} type The content type that we are setting exceptions for.
   * @param {Array} exceptions An array of pairs, where the first element of
   *     each pair is the filter string, and the second is the setting
   *     (allow/block).
   */
  ContentSettings.setExceptions = function(type, exceptions) {
    this.getExceptionsList(type, 'normal').setExceptions(exceptions);
  };

  ContentSettings.setHandlers = function(handlers) {
    $('handlers-list').setHandlers(handlers);
  };

  ContentSettings.setIgnoredHandlers = function(ignoredHandlers) {
    $('ignored-handlers-list').setHandlers(ignoredHandlers);
  };

  ContentSettings.setOTRExceptions = function(type, otrExceptions) {
    var exceptionsList = this.getExceptionsList(type, 'otr');
    // Settings for Guest hides many sections, so check for null first.
    if (exceptionsList) {
      exceptionsList.parentNode.hidden = false;
      exceptionsList.setExceptions(otrExceptions);
    }
  };

  /**
   * @param {string} type The type of exceptions (e.g. "location") to get.
   * @param {string} mode The mode of the desired exceptions list (e.g. otr).
   * @return {?options.contentSettings.ExceptionsList} The corresponding
   *     exceptions list or null.
   */
  ContentSettings.getExceptionsList = function(type, mode) {
    var exceptionsList = document.querySelector(
        'div[contentType=' + type + '] list[mode=' + mode + ']');
    return !exceptionsList ? null :
        assertInstanceof(exceptionsList,
                         options.contentSettings.ExceptionsList);
  };

  /**
   * The browser's response to a request to check the validity of a given URL
   * pattern.
   * @param {string} type The content type.
   * @param {string} mode The browser mode.
   * @param {string} pattern The pattern.
   * @param {boolean} valid Whether said pattern is valid in the context of
   *     a content exception setting.
   */
  ContentSettings.patternValidityCheckComplete =
      function(type, mode, pattern, valid) {
    this.getExceptionsList(type, mode).patternValidityCheckComplete(pattern,
                                                                    valid);
  };

  /**
   * Shows/hides the link to the Pepper Flash camera and microphone default
   * settings.
   * Please note that whether the link is actually showed or not is also
   * affected by the style class pepper-flash-settings.
   */
  ContentSettings.showMediaPepperFlashDefaultLink = function(show) {
    $('media-pepper-flash-default').hidden = !show;
  };

  /**
   * Shows/hides the link to the Pepper Flash camera and microphone
   * site-specific settings.
   * Please note that whether the link is actually showed or not is also
   * affected by the style class pepper-flash-settings.
   */
  ContentSettings.showMediaPepperFlashExceptionsLink = function(show) {
    $('media-pepper-flash-exceptions').hidden = !show;
  };

  /**
   * Shows/hides the whole Web MIDI settings.
   * @param {boolean} show Wether to show the whole Web MIDI settings.
   */
  ContentSettings.showExperimentalWebMIDISettings = function(show) {
    $('experimental-web-midi-settings').hidden = !show;
  };

  /**
   * Updates the microphone/camera devices menu with the given entries.
   * @param {string} type The device type.
   * @param {Array} devices List of available devices.
   * @param {string} defaultdevice The unique id of the current default device.
   */
  ContentSettings.updateDevicesMenu = function(type, devices, defaultdevice) {
    var deviceSelect = '';
    if (type == 'mic') {
      deviceSelect = $('media-select-mic');
    } else if (type == 'camera') {
      deviceSelect = $('media-select-camera');
    } else {
      console.error('Unknown device type for <device select> UI element: ' +
                    type);
      return;
    }

    deviceSelect.textContent = '';

    var deviceCount = devices.length;
    var defaultIndex = -1;
    for (var i = 0; i < deviceCount; i++) {
      var device = devices[i];
      var option = new Option(device.name, device.id);
      if (option.value == defaultdevice)
        defaultIndex = i;
      deviceSelect.appendChild(option);
    }
    if (defaultIndex >= 0)
      deviceSelect.selectedIndex = defaultIndex;
  };

  /**
   * Enables/disables the protected content exceptions button.
   * @param {boolean} enable Whether to enable the button.
   */
  ContentSettings.enableProtectedContentExceptions = function(enable) {
    var exceptionsButton = $('protected-content-exceptions');
    if (exceptionsButton)
      exceptionsButton.disabled = !enable;
  };

  /**
   * Set the default microphone device based on the popup selection.
   * @private
   */
  ContentSettings.setDefaultMicrophone_ = function() {
    var deviceSelect = $('media-select-mic');
    chrome.send('setDefaultCaptureDevice', ['mic', deviceSelect.value]);
  };

  /**
   * Set the default camera device based on the popup selection.
   * @private
   */
  ContentSettings.setDefaultCamera_ = function() {
    var deviceSelect = $('media-select-camera');
    chrome.send('setDefaultCaptureDevice', ['camera', deviceSelect.value]);
  };

  // Export
  return {
    ContentSettings: ContentSettings
  };

});
