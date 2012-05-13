// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings;
var browserBridge;

/**
 * Class that keeps track of current burn process state.
 * @param {Object} strings Localized state strings.
 * @constructor
 */
function State(strings) {
  this.setStrings(strings);
  this.changeState(State.StatesEnum.DEVICE_NONE);
}

/**
 * State Enum object.
 */
State.StatesEnum = {
  DEVICE_NONE: {
    cssState: 'device-detected-none',
  },
  DEVICE_USB: {
    cssState: 'device-detected-usb warning',
  },
  DEVICE_SD: {
    cssState: 'device-detected-sd warning',
  },
  DEVICE_MUL: {
    cssState: 'device-detected-mul warning',
  },
  ERROR_NO_NETWORK: {
    cssState: 'warning-no-conf',
  },
  ERROR_DEVICE_TOO_SMALL: {
    cssState: 'warning-no-conf',
  },
  PROGRESS_DOWNLOAD: {
    cssState: 'progress progress-canceble',
  },
  PROGRESS_UNZIP: {
    cssState: 'progress progress-canceble',
  },
  PROGRESS_BURN: {
    cssState: 'progress',
  },
  FAIL: {
    cssState: 'error',
  },
  SUCCESS: {
    cssState: 'success',
  },
};

State.prototype = {
  /**
   * Sets the state strings.
   * @param {Object} strings Localized state strings.
   */
  setStrings: function(strings) {
    State.StatesEnum.DEVICE_NONE.statusText =
        strings.getString('statusDevicesNone');
    State.StatesEnum.DEVICE_NONE.warningText =
        strings.getString('warningDevicesNone');
    State.StatesEnum.DEVICE_USB.statusText =
      strings.getString('statusDeviceUSB');
    State.StatesEnum.DEVICE_SD.statusText = strings.getString('statusDeviceSD');
    State.StatesEnum.DEVICE_MUL.statusText =
        strings.getString('statusDevicesMultiple');
    State.StatesEnum.ERROR_NO_NETWORK.statusText =
        strings.getString('statusNoConnection');
    State.StatesEnum.ERROR_NO_NETWORK.warningText =
        strings.getString('warningNoConnection');
    State.StatesEnum.ERROR_DEVICE_TOO_SMALL.statusText =
        strings.getString('statusNoSpace');
    State.StatesEnum.PROGRESS_DOWNLOAD.statusText =
        strings.getString('statusDownloading');
    State.StatesEnum.PROGRESS_UNZIP.statusText =
        strings.getString('statusUnzip');
    State.StatesEnum.PROGRESS_BURN.statusText = strings.getString('statusBurn');
    State.StatesEnum.FAIL.statusText = strings.getString('statusError');
    State.StatesEnum.SUCCESS.statusText = strings.getString('statusSuccess');
    State.StatesEnum.SUCCESS.warningText = strings.getString('warningSuccess');
  },

  /**
   * Changes the current state to new state.
   * @param {Object} newState Specifies the new state object.
   */
  changeState: function(newState) {
    if (newState == this.state)
      return;
    this.state = newState;

    $('main-content').className = this.state.cssState;

    $('status-text').textContent = this.state.statusText;

    if (newState.warningText)
      $('warning-text').textContent = this.state.warningText;

    if (this.isInitialState() && this.state != State.StatesEnum.DEVICE_NONE) {
      $('warning-button').textContent = localStrings.getString('confirmButton');
    } else if (this.state == State.StatesEnum.FAIL) {
      $('warning-button').textContent =
          localStrings.getString('retryButton');
    }
  },

  /**
   * Reset to initial state.
   * @param {number} deviceCount Device count information.
   */
  gotoInitialState: function(deviceCount) {
    if (deviceCount == 0)
      this.changeState(State.StatesEnum.DEVICE_NONE);
    else if (deviceCount == 1)
      this.changeState(State.StatesEnum.DEVICE_USB);
    else
      this.changeState(State.StatesEnum.DEVICE_MUL);
  },

  /**
   * Returns true if the device is in initial state.
   * @return {boolean} True if the device is in initial state else false.
   */
  isInitialState: function() {
    return this.state == State.StatesEnum.DEVICE_NONE ||
           this.state == State.StatesEnum.DEVICE_USB ||
           this.state == State.StatesEnum.DEVICE_SD ||
           this.state == State.StatesEnum.DEVICE_MUL;
  },

  /**
   * Returns true if device state matches the given state name.
   * @param {string} stateName Given state name.
   * @return {boolean} True if the device state matches the given state name.
   */
  equals: function(stateName) {
    return this.state == stateName;
  }
};

/**
 * Class that keeps track of available devices.
 * @constructor
 */
function DeviceSelection() {
  this.deviceCount = 0;
}

DeviceSelection.prototype = {
  /**
   * Clears the given selection list.
   * @param {Array} list Device selection list.
   */
  clearSelectList: function(list) {
    list.innerHtml = '';
  },

  /**
   * Shows the currently selected device.
   * @return {number} Selected device count.
   */
  showDeviceSelection: function() {
    if (this.deviceCount == 0) {
      this.selectedDevice = undefined;
    } else {
      var devices = document.getElementsByClassName('selection-element');
      this.selectDevice(devices[0].devicePath);
    }
    return this.deviceCount;
  },

  /**
   * Handles device selected event.
   * @param {string} label Device label.
   * @param {string} filePath File path.
   * @param {string} devicePath Selected device path.
   */
  onDeviceSelected: function(label, filePath, devicePath) {
    $('warning-button').onclick =
        browserBridge.sendBurnImageMessage.bind(browserBridge, filePath,
            devicePath);

    this.selectedDevice = devicePath;

    $('warning-text').textContent =
        localStrings.getStringF('warningDevices', label);
  },

  /**
   * Selects the specified device based on the specified path.
   * @param {string} path Device path.
   */
  selectDevice: function(path) {
    var element = $('radio ' + path);
    element.checked = true;
    element.onclick.apply(element);
  },

  /**
   * Creates a new device element.
   * @param {Object} device Specifies new device information.
   * @return {HTMLLIElement} New device element.
   */
  createNewDeviceElement: function(device) {
    var element = document.createElement('li');
    var radioButton = document.createElement('input');
    radioButton.type = 'radio';
    radioButton.name = 'device';
    radioButton.value = device.label;
    radioButton.id = 'radio ' + device.devicePath;
    radioButton.className = 'float-start';
    var deviceLabelText = document.createElement('p');
    deviceLabelText.textContent = device.label;
    deviceLabelText.className = 'select-option float-start';
    var newLine = document.createElement('div');
    newLine.className = 'new-line';
    element.appendChild(radioButton);
    element.appendChild(deviceLabelText);
    element.appendChild(newLine);
    element.id = 'select ' + device.devicePath;
    element.devicePath = device.devicePath;
    element.className = 'selection-element';
    radioButton.onclick = this.onDeviceSelected.bind(this,
        device.label, device.filePath, device.devicePath);
    return element;
  },

  /**
   * Updates the list of selected devices.
   * @param {Array} devices List of devices.
   * @return {number} Device count.
   */
  getDevicesCallback: function(devices) {
    var selectListDOM = $('device-selection');
    this.clearSelectList(selectListDOM);
    this.deviceCount = devices.length;
    if (devices.length > 0) {
      for (var i = 0; i < devices.length; i++) {
        var element = this.createNewDeviceElement(devices[i]);
        selectListDOM.appendChild(element);
      }
      this.selectDevice(devices[0].devicePath);
    } else {
      this.selectedDevice = undefined;
    }
    return this.deviceCount;
  },

  /**
   * Handles device added event.
   * @param {Object} device Device information.
   * @param {boolean} allowSelect True to update the selected device info.
   * @return {number} Device count.
   */
  deviceAdded: function(device, allowSelect) {
    var selectListDOM = $('device-selection');
    selectListDOM.appendChild(this.createNewDeviceElement(device));
    this.deviceCount++;
    if (allowSelect && this.deviceCount == 1)
      this.selectDevice(device.devicePath);
    return this.deviceCount;
  },

  /**
   * Handles device removed event.
   * @param {string} devicePath Selected device path.
   * @param {boolean} allowSelect True to update the selected device info.
   * @return {number} Device count.
   */
  deviceRemoved: function(devicePath, allowSelect) {
    var deviceSelectElement = $('select ' + devicePath);
    deviceSelectElement.parentNode.removeChild(deviceSelectElement);
    this.deviceCount--;
    var devices = document.getElementsByClassName('selection-element');

    if (allowSelect) {
      if (devices.length > 0) {
        if (this.selectedDevice == devicePath)
          this.selectDevice(devices[0].devicePath);
      } else {
        this.selectedDevice = undefined;
      }
    }
    return this.deviceCount;
  }
};

/**
 * Class that handles communication with chrome.
 * @constructor
 */
function BrowserBridge() {
  this.currentState = new State(localStrings);
  this.devices = new DeviceSelection();
  // We will use these often so it makes sence making them class members to
  // avoid frequent document.getElementById calls.
  this.progressElement = $('progress-div');
  this.progressText = $('progress-text');
  this.progressTimeLeftText = $('pending-time');
}

BrowserBridge.prototype = {
  sendCancelMessage: function() {
    chrome.send('cancelBurnImage');
  },

  sendGetDevicesMessage: function() {
    chrome.send('getDevices');
  },

  sendWebuiInitializedMessage: function() {
    chrome.send('webuiInitialized');
  },

  /**
   * Sends the burn image message to c++ code.
   * @param {string} filePath Specifies the file path.
   * @param {string} devicePath Specifies the device path.
   */
  sendBurnImageMessage: function(filePath, devicePath) {
    chrome.send('burnImage', [devicePath, filePath]);
  },

  reportSuccess: function() {
    this.currentState.changeState(State.StatesEnum.SUCCESS);
  },

  /**
   * Update the device state to report a failure and display an error message to
   * the user.
   * @param {string} errorMessage Specifies the warning text message.
   */
  reportFail: function(errorMessage) {
    this.currentState.changeState(State.StatesEnum.FAIL);
    $('warning-text').textContent = errorMessage;
    $('warning-button').onclick = this.onBurnRetry.bind(this);
  },

  /**
   * Handles device added event.
   * @param {Object} device Device information.
   */
  deviceAdded: function(device) {
    var inInitialState = this.currentState.isInitialState();
    var deviceCount = this.devices.deviceAdded(device, inInitialState);
    if (inInitialState)
      this.currentState.gotoInitialState(deviceCount);
  },

  /**
   * Handles device removed event.
   * @param {Object} device Device information.
   */
  deviceRemoved: function(device) {
    var inInitialState = this.currentState.isInitialState();
    var deviceCount = this.devices.deviceRemoved(device, inInitialState);
    if (inInitialState)
      this.currentState.gotoInitialState(deviceCount);
  },

  /**
   * Gets device callbacks and update the current state.
   * @param {Array} devices List of devices.
   */
  getDevicesCallback: function(devices) {
    var deviceCount = this.devices.getDevicesCallback(devices);
    this.currentState.gotoInitialState(deviceCount);
    this.sendWebuiInitializedMessage();
  },

  /**
   *  Updates the progress information based on the signal received.
   *  @param {Object} updateSignal Specifies the signal information.
   */
  updateProgress: function(updateSignal) {
    if (updateSignal.progressType == 'download' &&
        !this.currentState.equals(State.StatesEnum.PROGRESS_DOWNLOAD)) {
      this.currentState.changeState(State.StatesEnum.PROGRESS_DOWNLOAD);
    } else if (updateSignal.progressType == 'unzip' &&
        !this.currentState.equals(State.StatesEnum.PROGRESS_UNZIP)) {
      this.currentState.changeState(State.StatesEnum.PROGRESS_UNZIP);
    } else if (updateSignal.progressType == 'burn' &&
        !this.currentState.equals(State.StatesEnum.PROGRESS_BURN)) {
      this.currentState.changeState(State.StatesEnum.PROGRESS_BURN);
    }

    if (!(updateSignal.amountTotal > 0)) {
      this.progressElement.removeAttribute('value');
    } else {
      this.progressElement.value = updateSignal.amountFinished;
      this.progressElement.max = updateSignal.amountTotal;
    }

    this.progressText.textContent = updateSignal.progressText;
    this.progressTimeLeftText.textContent = updateSignal.timeLeftText;
  },

  reportNoNetwork: function() {
    this.currentState.changeState(State.StatesEnum.ERROR_NO_NETWORK);
  },

  reportNetworkDetected: function() {
    if (this.currentState.equals(State.StatesEnum.ERROR_NO_NETWORK)) {
      var deviceCount = this.devices.showDeviceSelection();
      this.currentState.gotoInitialState(deviceCount);
    }
  },

  /**
   *  Updates the current state to report device too small error.
   *  @param {number} deviceSize Received device size.
   */
  reportDeviceTooSmall: function(deviceSize) {
    this.currentState.changeState(State.StatesEnum.ERROR_DEVICE_TOO_SMALL);
    $('warning-text').textContent =
        localStrings.getStringF('warningNoSpace', deviceSize);
  },

  /**
   * Processes click on 'Retry' button in FAIL state.
   */
  onBurnRetry: function() {
    var deviceCount = this.devices.showDeviceSelection();
    this.currentState.gotoInitialState(deviceCount);
  }
};

document.addEventListener('DOMContentLoaded', function() {
  localStrings = new LocalStrings();
  browserBridge = new BrowserBridge();

  jstProcess(new JsEvalContext(templateData), $('more-info-link'));

  $('cancel-button').onclick =
      browserBridge.sendCancelMessage.bind(browserBridge);
  browserBridge.sendGetDevicesMessage();
});
