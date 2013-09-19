// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for local_discovery.html, served from chrome://devices/
 * This is used to show discoverable devices near the user as well as
 * cloud devices registered to them.
 *
 * The object defined in this javascript file listens for callbacks from the
 * C++ code saying that a new device is available as well as manages the UI for
 * registering a device on the local network.
 */

cr.define('local_discovery', function() {
  'use strict';

  /**
   * Prefix for printer management page URLs, relative to base cloud print URL.
   * @type {string}
   */
  var PRINTER_MANAGEMENT_PAGE_PREFIX = '#printers/';

  /**
   * Map of service names to corresponding service objects.
   * @type {Object.<string,Service>}
   */
  var devices = {};

  /**
   * Whether or not the user is currently logged in.
   * @type bool
   */
  var isUserLoggedIn = true;

  /**
   * Focus manager for page.
   */
  var focusManager = null;

  /**
   * Object that represents a device in the device list.
   * @param {Object} info Information about the device.
   * @constructor
   */
  function Device(info, registerEnabled) {
    this.info = info;
    this.domElement = null;
    this.registerButton = null;
    this.registerEnabled = registerEnabled;
  }

  Device.prototype = {
    /**
     * Update the device.
     * @param {Object} info New information about the device.
     */
    updateDevice: function(info) {
      this.info = info;
      this.renderDevice();
    },

    /**
     * Delete the device.
     */
    removeDevice: function() {
      this.deviceContainer().removeChild(this.domElement);
    },

    /**
     * Render the device to the device list.
     */
    renderDevice: function() {
      if (this.domElement) {
        clearElement(this.domElement);
      } else {
        this.domElement = document.createElement('div');
        this.deviceContainer().appendChild(this.domElement);
      }

      this.registerButton = fillDeviceDescription(
        this.domElement,
        this.info.human_readable_name,
        this.info.description,
        loadTimeData.getString('serviceRegister'),
        this.showRegister.bind(this));

      this.setRegisterEnabled(this.registerEnabled);
    },

    /**
     * Return the correct container for the device.
     * @param {boolean} is_mine Whether or not the device is in the 'Registered'
     *    section.
     */
    deviceContainer: function() {
      return $('register-device-list');
    },
    /**
     * Register the device.
     */
    register: function() {
      recordUmaAction('DevicesPage_RegisterConfirmed');
      chrome.send('registerDevice', [this.info.service_name]);
      setRegisterPage('register-page-adding1');
    },
    /**
     * Show registrtation UI for device.
     */
    showRegister: function() {
      recordUmaAction('DevicesPage_RegisterClicked');
      $('register-message').textContent = loadTimeData.getStringF(
        'registerConfirmMessage',
        this.info.human_readable_name);
      $('register-continue-button').onclick = this.register.bind(this);
      showRegisterOverlay();
    },
    /**
     * Set registration button enabled/disabled
     */
    setRegisterEnabled: function(isEnabled) {
      this.registerEnabled = isEnabled;
      if (this.registerButton) {
        this.registerButton.disabled = !isEnabled;
      }
    }
  };

  /**
   * Manages focus for local devices page.
   * @constructor
   * @extends {cr.ui.FocusManager}
   */
  function LocalDiscoveryFocusManager() {
    cr.ui.FocusManager.call(this);
    this.focusParent_ = document.body;
  }

  LocalDiscoveryFocusManager.prototype = {
    __proto__: cr.ui.FocusManager.prototype,
    /** @override */
    getFocusParent: function() {
      return document.querySelector('#overlay .showing') ||
        $('main-page');
    }
  };

  /**
   * Returns a textual representation of the number of printers on the network.
   * @return {string} Number of printers on the network as localized string.
   */
  function generateNumberPrintersAvailableText(numberPrinters) {
    if (numberPrinters == 0) {
      return loadTimeData.getString('printersOnNetworkZero');
    } else if (numberPrinters == 1) {
      return loadTimeData.getString('printersOnNetworkOne');
    } else {
      return loadTimeData.getStringF('printersOnNetworkMultiple',
                                     numberPrinters);
    }
  }

  /**
   * Fill device element with the description of a device.
   * @param {HTMLElement} device_dom_element Element to be filled.
   * @param {string} name Name of device.
   * @param {string} description Description of device.
   * @param {string} button_text Text to appear on button.
   * @param {function()} button_action Action for button.
   * @return {HTMLElement} The button (for enabling/disabling/rebinding)
   */
  function fillDeviceDescription(device_dom_element,
                                name,
                                description,
                                button_text,
                                button_action) {
    device_dom_element.classList.add('device');
    device_dom_element.classList.add('printer');

    var deviceInfo = document.createElement('div');
    deviceInfo.className = 'device-info';
    device_dom_element.appendChild(deviceInfo);

    var deviceName = document.createElement('h3');
    deviceName.className = 'device-name';
    deviceName.textContent = name;
    deviceInfo.appendChild(deviceName);

    var deviceDescription = document.createElement('div');
    deviceDescription.className = 'device-subline';
    deviceDescription.textContent = description;
    deviceInfo.appendChild(deviceDescription);

    var button = document.createElement('button');
    button.textContent = button_text;
    button.addEventListener('click', button_action);
    device_dom_element.appendChild(button);

    return button;
  }

  /**
   * Show the register overlay.
   */
  function showRegisterOverlay() {
    recordUmaAction('DevicesPage_AddPrintersClicked');

    var registerOverlay = $('register-overlay');
    registerOverlay.classList.add('showing');
    registerOverlay.focus();

    $('overlay').hidden = false;
    setRegisterPage('register-page-confirm');
  }

  /**
   * Hide the register overlay.
   */
  function hideRegisterOverlay() {
    $('register-overlay').classList.remove('showing');
    $('overlay').hidden = true;
  }

  /**
   * Clear a DOM element of all children.
   * @param {HTMLElement} element DOM element to clear.
   */
  function clearElement(element) {
    while (element.firstChild) {
      element.removeChild(element.firstChild);
    }
  }

  /**
   * Announce that a registration failed.
   */
  function onRegistrationFailed() {
    setRegisterPage('register-page-error');
    recordUmaAction('DevicesPage_RegisterFailure');
  }

  /**
   * Update UI to reflect that registration has been confirmed on the printer.
   */
  function onRegistrationConfirmedOnPrinter() {
    setRegisterPage('register-page-adding2');
  }

  /**
   * Update device unregistered device list, and update related strings to
   * reflect the number of devices available to register.
   * @param {string} name Name of the device.
   * @param {string} info Additional info of the device or null if the device
   *                          has been removed.
   */
  function onUnregisteredDeviceUpdate(name, info) {
    if (info) {
      if (devices.hasOwnProperty(name)) {
        devices[name].updateDevice(info);
      } else {
        devices[name] = new Device(info, isUserLoggedIn);
        devices[name].renderDevice();
      }
    } else {
      if (devices.hasOwnProperty(name)) {
        devices[name].removeDevice();
        delete devices[name];
      }
    }

    updateUIToReflectState();
  }

  /**
   * Handle a list of cloud devices available to the user globally.
   * @param {Array.<Object>} devices_list List of devices.
   */
  function onCloudDeviceListAvailable(devices_list) {
    var devicesListLength = devices_list.length;
    var devicesContainer = $('cloud-devices');

    clearElement(devicesContainer);
    $('cloud-devices-loading').hidden = true;

    for (var i = 0; i < devicesListLength; i++) {
      var devicesDomElement = document.createElement('div');
      devicesContainer.appendChild(devicesDomElement);

      var description;
      if (devices_list[i].description == '') {
        description = loadTimeData.getString('noDescription');
      } else {
        description = devices_list[i].description;
      }

      fillDeviceDescription(devicesDomElement, devices_list[i].display_name,
                            description, 'Manage' /*Localize*/,
                            manageCloudDevice.bind(null, devices_list[i].id));
    }
  }

  /**
   * Handle the case where the list of cloud devices is not available.
   */
  function onCloudDeviceListUnavailable() {
    if (isUserLoggedIn) {
      $('cloud-devices-loading').hidden = true;
      $('cloud-devices-unavailable').hidden = false;
    }
  }

  /**
   * Handle the case where the cache for local devices has been flushed..
   */
  function onDeviceCacheFlushed() {
    for (var deviceName in devices) {
      devices[deviceName].removeDevice();
      delete devices[deviceName];
    }

    updateUIToReflectState();
  }

  /**
   * Update UI strings to reflect the number of local devices.
   */
  function updateUIToReflectState() {
    var numberPrinters = $('register-device-list').children.length;
    if (numberPrinters == 0) {
      $('no-printers-message').hidden = false;

      $('register-login-promo').hidden = true;
    } else {
      $('no-printers-message').hidden = true;
      $('register-login-promo').hidden = isUserLoggedIn;
    }
  }


  /**
   * Announce that a registration succeeeded.
   */
  function onRegistrationSuccess() {
    hideRegisterOverlay();
    requestPrinterList();
    recordUmaAction('DevicesPage_RegisterSuccess');
  }

  /**
   * Update visibility status for page.
   */
  function updateVisibility() {
    chrome.send('isVisible', [!document.webkitHidden]);
  }

  /**
   * Set the page that the register wizard is on.
   * @param {string} page_id ID string for page.
   */
  function setRegisterPage(page_id) {
    var pages = $('register-overlay').querySelectorAll('.register-page');
    var pagesLength = pages.length;
    for (var i = 0; i < pagesLength; i++) {
      pages[i].hidden = true;
    }

    $(page_id).hidden = false;
  }

  /**
   * Request the printer list.
   */
  function requestPrinterList() {
    if (isUserLoggedIn) {
      clearElement($('cloud-devices'));
      $('cloud-devices-loading').hidden = false;
      $('cloud-devices-unavailable').hidden = true;

      chrome.send('requestPrinterList');
    }
  }

  /**
   * Go to management page for a cloud device.
   * @param {string} device_id ID of device.
   */
  function manageCloudDevice(device_id) {
    recordUmaAction('DevicesPage_ManageClicked');
    chrome.send('openCloudPrintURL',
                [PRINTER_MANAGEMENT_PAGE_PREFIX + device_id]);
  }

  /**
   * Record an action in UMA.
   * @param {string} actionDesc The name of the action to be logged.
   */
  function recordUmaAction(actionDesc) {
    chrome.send('metricsHandler:recordAction', [actionDesc]);
  }

  /**
   * Cancel the registration.
   */
  function cancelRegistration() {
    hideRegisterOverlay();
    chrome.send('cancelRegistration');
    recordUmaAction('DevicesPage_RegisterCancel');
  }

  /**
   * Retry loading the devices from Google Cloud Print.
   */
  function retryLoadCloudDevices() {
    requestPrinterList();
  }

  /**
   * User is not logged in.
   */
  function setUserLoggedIn(userLoggedIn) {
    isUserLoggedIn = userLoggedIn;

    $('cloud-devices-login-promo').hidden = isUserLoggedIn;


    if (isUserLoggedIn) {
      requestPrinterList();
      $('register-login-promo').hidden = true;
    } else {
      $('cloud-devices-loading').hidden = true;
      $('cloud-devices-unavailable').hidden = true;
    }

    updateUIToReflectState();

    for (var device in devices) {
      devices[device].setRegisterEnabled(isUserLoggedIn);
    }
  }

  function openSignInPage() {
    chrome.send('showSyncUI');
  }

  function registerLoginButtonClicked() {
    recordUmaAction('DevicesPage_LogInStartedFromRegisterPromo');
    openSignInPage();
  }

  function cloudDevicesLoginButtonClicked() {
    recordUmaAction('DevicesPage_LogInStartedFromDeviceListPromo');
    openSignInPage();
  }

  document.addEventListener('DOMContentLoaded', function() {
    cr.ui.overlay.setupOverlay($('overlay'));
    cr.ui.overlay.globalInitialization();
    $('overlay').addEventListener('cancelOverlay', cancelRegistration);

    var cancelButtons = document.querySelectorAll('.register-cancel');
    var cancelButtonsLength = cancelButtons.length;
    for (var i = 0; i < cancelButtonsLength; i++) {
      cancelButtons[i].addEventListener('click', cancelRegistration);
    }

    $('register-error-exit').addEventListener('click', cancelRegistration);


    $('cloud-devices-retry-button').addEventListener('click',
                                                     retryLoadCloudDevices);

    $('cloud-devices-login-button').addEventListener(
      'click',
      cloudDevicesLoginButtonClicked);

    $('register-login-button').addEventListener(
      'click',
      registerLoginButtonClicked);

    updateVisibility();
    document.addEventListener('webkitvisibilitychange', updateVisibility,
                              false);


    focusManager = new LocalDiscoveryFocusManager();
    focusManager.initialize();

    chrome.send('start');
    recordUmaAction('DevicesPage_Opened');
  });

  return {
    onRegistrationSuccess: onRegistrationSuccess,
    onRegistrationFailed: onRegistrationFailed,
    onUnregisteredDeviceUpdate: onUnregisteredDeviceUpdate,
    onRegistrationConfirmedOnPrinter: onRegistrationConfirmedOnPrinter,
    onCloudDeviceListAvailable: onCloudDeviceListAvailable,
    onCloudDeviceListUnavailable: onCloudDeviceListUnavailable,
    onDeviceCacheFlushed: onDeviceCacheFlushed,
    setUserLoggedIn: setUserLoggedIn
  };
});
