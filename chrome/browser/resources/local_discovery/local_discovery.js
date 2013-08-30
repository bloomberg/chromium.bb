// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for local_discovery.html, served from chrome://devices/
 * This is used to show discoverable devices near the user.
 *
 * The simple object defined in this javascript file listens for
 * callbacks from the C++ code saying that a new device is available.
 */


<include src="../uber/uber_utils.js" />

cr.define('local_discovery', function() {
  'use strict';

  /**
   * Map of service names to corresponding service objects.
   * @type {Object.<string,Service>}
   */
  var devices = {};


  /**
   * Object that represents a device in the device list.
   * @param {Object} info Information about the device.
   * @constructor
   */
  function Device(info) {
    this.info = info;
    this.domElement = null;
  }

  Device.prototype = {
    /**
     * Update the device.
     * @param {Object} info New information about the device.
     */
    updateDevice: function(info) {
      if (this.domElement && info.is_mine != this.info.is_mine) {
        this.deviceContainer(this.info.is_mine).removeChild(this.domElement);
        this.deviceContainer(info.is_mine).appendChild(this.domElement);
      }
      this.info = info;
      this.renderDevice();
    },

    /**
     * Delete the device.
     */
    removeDevice: function() {
      this.deviceContainer(this.info.is_mine).removeChild(this.domElement);
    },

    /**
     * Render the device to the device list.
     */
    renderDevice: function() {
      if (this.domElement) {
        clearElement(this.domElement);
      } else {
        this.domElement = document.createElement('div');
        this.deviceContainer(this.info.is_mine).appendChild(this.domElement);
      }

      this.domElement.classList.add('device');
      this.domElement.classList.add('printer-active');

      var deviceInfo = document.createElement('div');
      deviceInfo.className = 'device-info';
      this.domElement.appendChild(deviceInfo);

      var deviceName = document.createElement('h3');
      deviceName.className = 'device-name';
      deviceName.textContent = this.info.human_readable_name;
      deviceInfo.appendChild(deviceName);

      var deviceDescription = document.createElement('div');
      deviceDescription.className = 'device-description';
      deviceDescription.textContent = this.info.description;
      deviceInfo.appendChild(deviceDescription);

      var buttonContainer = document.createElement('div');
      buttonContainer.className = 'button-container';
      this.domElement.appendChild(buttonContainer);

      var button = document.createElement('button');
      button.textContent = loadTimeData.getString('serviceRegister');

      if (this.info.registered) {
        button.disabled = 'disabled';
      } else {
        button.addEventListener(
          'click',
          sendRegisterDevice.bind(null, this.info.service_name));
      }

      buttonContainer.appendChild(button);
    },

    /**
     * Return the correct container for the device.
     * @param {boolean} is_mine Whether or not the device is in the 'Registered'
     *    section.
     */
    deviceContainer: function(is_mine) {
      if (is_mine) return $('registered-devices');
      return $('unregistered-devices');
    }
  };

  /**
   * Appends a row to the output table listing the new device.
   * @param {string} name Name of the device.
   * @param {string} info Additional info of the device, if empty device need to
   *                      be deleted.
   */
  function onDeviceUpdate(name, info) {
    if (info) {
      if (devices.hasOwnProperty(name)) {
        devices[name].updateDevice(info);
      } else {
        devices[name] = new Device(info);
        devices[name].renderDevice();
      }
    } else {
      devices[name].removeDevice();
      delete devices[name];
    }
  }

  /**
   * Hide the register overlay.
   */
  function showRegisterOverlay() {
    $('register-overlay').classList.add('showing');
    $('overlay').hidden = false;
    uber.invokeMethodOnParent('beginInterceptingEvents');
  }

  /**
   * Show the register overlay.
   */
  function hideRegisterOverlay() {
    $('register-overlay').classList.remove('showing');
    $('overlay').hidden = true;
    uber.invokeMethodOnParent('stopInterceptingEvents');
    chrome.send('cancelRegistration');
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
   * Register a device.
   * @param {string} device The device to register.
   */
  function sendRegisterDevice(device) {
    chrome.send('registerDevice', [device]);
  }

  /**
   * Announce that a registration failed.
   */
  function registrationFailed() {
    setRegisterPage('register-page-error');
  }

  /**
   * Update UI to reflect that registration has been confirmed on the printer.
   */
  function registrationConfirmedOnPrinter() {
    setRegisterPage('register-page-adding2');
  }

  /**
   * Announce that a registration succeeeded.
   */
  function registrationSuccess() {
    hideRegisterOverlay();
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
   * Request a user account from a list.
   * @param {Array} users Array of (index, username) tuples. Username may be
   *    displayed to the user; index must be passed opaquely to the UI handler.
   */
  function requestUser(users, printerName) {
    clearElement($('register-user-list'));

    var usersLength = users.length;
    for (var i = 0; i < usersLength; i++) {
      var userIndex = users[i][0];
      var userName = users[i][1];

      var option = document.createElement('option');
      option.textContent = userName;
      option.userData = { userIndex: userIndex, userName: userName };
      $('register-user-list').appendChild(option);
    }

    showRegisterOverlay();
    setRegisterPage('register-page-choose');
    $('register-message').textContent =
      loadTimeData.getStringF('registerConfirmMessage', printerName);
  }

  /**
   * Send user selection and begin registration.
   */
  function beginRegistration() {
    var userList = $('register-user-list');
    var selectedOption = userList.options[userList.selectedIndex];
    var userData = selectedOption.userData;
    chrome.send('chooseUser', [userData.userIndex, userData.userName]);
    setRegisterPage('register-page-adding1');
  }

  document.addEventListener('DOMContentLoaded', function() {
    uber.onContentFrameLoaded();

    cr.ui.overlay.setupOverlay($('overlay'));
    cr.ui.overlay.globalInitialization();
    $('overlay').addEventListener('cancelOverlay', hideRegisterOverlay);

    var cancelButtons = document.querySelectorAll('.register-cancel');
    var cancelButtonsLength = cancelButtons.length;
    for (var i = 0; i < cancelButtonsLength; i++) {
      cancelButtons[i].addEventListener('click', hideRegisterOverlay);
    }

    $('register-error-exit').addEventListener('click', hideRegisterOverlay);

    $('register-confirmation-continue').addEventListener(
      'click', beginRegistration);

    updateVisibility();
    document.addEventListener('webkitvisibilitychange', updateVisibility,
                              false);

    var title = loadTimeData.getString('devicesTitle');
    uber.invokeMethodOnParent('setTitle', {title: title});

    chrome.send('start');
  });

  return {
    registrationSuccess: registrationSuccess,
    registrationFailed: registrationFailed,
    onDeviceUpdate: onDeviceUpdate,
    requestUser: requestUser,
    registrationConfirmedOnPrinter: registrationConfirmedOnPrinter
  };
});
