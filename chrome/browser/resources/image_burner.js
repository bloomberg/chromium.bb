// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings;
var browserBridge;

// Class that keeps track of current burn process state.
function State(strings) {
  this.setStrings(strings);
  this.changeState(State.StatesEnum.DEVICE_NONE);
}

State.StatesEnum = {
  DEVICE_NONE : {
    cssState : "device-detected-none",
  },
  DEVICE_USB : {
    cssState : "device-detected-usb warning",
  },
  DEVICE_SD : {
    cssState : "device-detected-sd warning",
  },
  DEVICE_MUL : {
    cssState: "device-detected-mul warning",
  },
  ERROR_NO_NETWORK : {
    cssState : "warning-no-conf",
  },
  ERROR_DEVICE_TOO_SMALL : {
    cssState : "warning-no-conf",
  },
  PROGRESS_DOWNLOAD : {
    cssState : "progress progress-canceble",
  },
  PROGRESS_UNZIP : {
    cssState : "progress progress-canceble",
  },
  PROGRESS_BURN : {
    cssState : "progress",
  },
  FAIL : {
    cssState : "error",
  },
  SUCCESS : {
    cssState : "success",
  },
};

State.prototype = {
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

  changeState: function(newState) {
    if (newState == this.state)
      return;
    this.state = newState;

    $('main-content').className = this.state.cssState;

    $('status-text').textContent = this.state.statusText;

    if (newState.warningText) {
      $('warning-text').textContent = this.state.warningText;
    }

    if (this.isInitialState() && this.state != State.StatesEnum.DEVICE_NONE) {
      $('warning-button').textContent = localStrings.getString('confirmButton');
    } else if (this.state == State.StatesEnum.FAIL) {
      $('warning-button').textContent =
          localStrings.getString('retryButton');
    }
  },

  gotoInitialState: function(deviceCount) {
    if (deviceCount == 0) {
      this.changeState(State.StatesEnum.DEVICE_NONE);
    } else if (deviceCount == 1) {
      this.changeState(State.StatesEnum.DEVICE_USB);
    } else {
      this.changeState(State.StatesEnum.DEVICE_MUL);
    }
  },

  isInitialState: function() {
    return (this.state == State.StatesEnum.DEVICE_NONE ||
            this.state == State.StatesEnum.DEVICE_USB ||
            this.state == State.StatesEnum.DEVICE_SD ||
            this.state == State.StatesEnum.DEVICE_MUL);
  },

  equals: function(stateName) {
    return this.state == stateName;
  }
};

// Class that keeps track of available devices.
function DeviceSelection() {
  this.deviceCount = 0;
};

DeviceSelection.prototype = {
  clearSelectList: function(list) {
    list.innerHtml = '';
  },

  showDeviceSelection: function() {
    if (this.deviceCount == 0) {
      this.selectedDevice = undefined;
    } else {
      var devices = document.getElementsByClassName('selection-element');
      this.selectDevice(devices[0].devicePath);
    }
    return this.deviceCount;
  },

  onDeviceSelected: function(label, filePath, devicePath) {
    $('warning-button').onclick =
        browserBridge.sendBurnImageMessage.bind(browserBridge, filePath,
            devicePath);

    this.selectedDevice = devicePath;

    $('warning-text').textContent =
        localStrings.getStringF('warningDevices', label);
  },

  selectDevice: function(path) {
    var element = $('radio ' + path);
    element.checked = true;
    element.onclick.apply(element);
  },

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

  deviceAdded: function(device, allowSelect) {
    var selectListDOM = $('device-selection');
    selectListDOM.appendChild(this.createNewDeviceElement(device));
    this.deviceCount++;
    if (allowSelect && this.deviceCount == 1)
      this.selectDevice(device.devicePath);
    return this.deviceCount;
  },

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

// Class that handles communication with chrome.
function BrowserBridge() {
  this.currentState = new State(localStrings);
  this.devices = new DeviceSelection();
  // We will use these often so it makes sence making them class members to
  // avoid frequent document.getElementById calls.
  this.progressElement = $('progress-div');
  this.progressText = $('progress-text');
  this.progressTimeLeftText = $('pending-time');
};

BrowserBridge.prototype = {
  sendCancelMessage: function() {
    chrome.send("cancelBurnImage");
  },

  sendGetDevicesMessage: function() {
    chrome.send("getDevices");
  },

  sendWebuiInitializedMessage: function() {
    chrome.send("webuiInitialized");
  },

  sendBurnImageMessage: function(filePath, devicePath) {
    chrome.send('burnImage', [devicePath, filePath]);
  },

  reportSuccess: function() {
    this.currentState.changeState(State.StatesEnum.SUCCESS);
  },

  reportFail: function(errorMessage) {
    this.currentState.changeState(State.StatesEnum.FAIL);
    $('warning-text').textContent = errorMessage;
    $('warning-button').onclick = this.onBurnRetry.bind(this);
  },

  deviceAdded: function(device) {
    var inInitialState = this.currentState.isInitialState();
    var deviceCount = this.devices.deviceAdded(device, inInitialState);
    if (inInitialState)
      this.currentState.gotoInitialState(deviceCount);
  },

  deviceRemoved: function(device) {
    var inInitialState = this.currentState.isInitialState();
    var deviceCount = this.devices.deviceRemoved(device, inInitialState);
    if (inInitialState)
      this.currentState.gotoInitialState(deviceCount);
  },

  getDevicesCallback: function(devices) {
    var deviceCount = this.devices.getDevicesCallback(devices);
    this.currentState.gotoInitialState(deviceCount);
    this.sendWebuiInitializedMessage();
  },

  updateProgress: function(update_signal) {
    if (update_signal.progressType == 'download' &&
        !this.currentState.equals(State.StatesEnum.PROGRESS_DOWNLOAD)) {
      this.currentState.changeState(State.StatesEnum.PROGRESS_DOWNLOAD);
    } else if (update_signal.progressType == 'unzip' &&
        !this.currentState.equals(State.StatesEnum.PROGRESS_UNZIP)) {
      this.currentState.changeState(State.StatesEnum.PROGRESS_UNZIP);
    } else if (update_signal.progressType == 'burn' &&
        !this.currentState.equals(State.StatesEnum.PROGRESS_BURN)) {
      this.currentState.changeState(State.StatesEnum.PROGRESS_BURN);
    }

    if (!(update_signal.amountTotal > 0)) {
      this.progressElement.removeAttribute('value');
    } else {
      this.progressElement.value = update_signal.amountFinished;
      this.progressElement.max = update_signal.amountTotal;
    }

    this.progressText.textContent = update_signal.progressText;
    this.progressTimeLeftText.textContent = update_signal.timeLeftText;
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

  reportDeviceTooSmall: function(device_size) {
    this.currentState.changeState(State.StatesEnum.ERROR_DEVICE_TOO_SMALL);
    $('warning-text').textContent =
        localStrings.getStringF('warningNoSpace', device_size);
  },

  // Processes click on "Retry" button in FAIL state.
  onBurnRetry: function () {
    var deviceCount = this.devices.showDeviceSelection();
    this.currentState.gotoInitialState(deviceCount);
  }
};

document.addEventListener('DOMContentLoaded', function() {
  localStrings = new LocalStrings();
  browserBridge = new BrowserBridge();

  jstProcess(new JsEvalContext(templateData), $("more-info-link"));

  $('cancel-button').onclick =
      browserBridge.sendCancelMessage.bind(browserBridge);
  browserBridge.sendGetDevicesMessage();
});
