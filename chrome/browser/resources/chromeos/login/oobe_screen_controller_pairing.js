// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview controller pairing screen implementation.
 */

login.createScreen('ControllerPairingScreen', 'controller-pairing', function() {
  'use strict';

  // Keep these constants synced with corresponding constants defined in
  // controller_pairing_screen_actor.{h,cc}.
  /** @const */ var CONTEXT_KEY_PAGE = 'page';
  /** @const */ var CONTEXT_KEY_CONTROLS_DISABLED = 'controlsDisabled';
  /** @const */ var CONTEXT_KEY_DEVICES = 'devices';
  /** @const */ var CONTEXT_KEY_CONFIRMATION_CODE = 'code';
  /** @const */ var CONTEXT_KEY_SELECTED_DEVICE = 'selectedDevice';
  /** @const */ var CONTEXT_KEY_ACCOUNT_ID = 'accountId';
  /** @const */ var CONTEXT_KEY_ENROLLMENT_DOMAIN = 'enrollmentDomain';

  /** @const */ var ACTION_ENROLL = 'enroll';

  /** @const */ var PAGE_DEVICES_DISCOVERY = 'devices-discovery';
  /** @const */ var PAGE_DEVICE_SELECT = 'device-select';
  /** @const */ var PAGE_DEVICE_NOT_FOUND = 'device-not-found';
  /** @const */ var PAGE_ESTABLISHING_CONNECTION = 'establishing-connection';
  /** @const */ var PAGE_ESTABLISHING_CONNECTION_ERROR =
      'establishing-connection-error';
  /** @const */ var PAGE_CODE_CONFIRMATION = 'code-confirmation';
  /** @const */ var PAGE_HOST_UPDATE = 'host-update';
  /** @const */ var PAGE_HOST_CONNECTION_LOST = 'host-connection-lost';
  /** @const */ var PAGE_ENROLLMENT_INTRODUCTION = 'enrollment-introduction';
  /** @const */ var PAGE_AUTHENTICATION = 'authentication';
  /** @const */ var PAGE_HOST_ENROLLMENT = 'host-enrollment';
  /** @const */ var PAGE_HOST_ENROLLMENT_ERROR = 'host-enrollment-error';
  /** @const */ var PAGE_PAIRING_DONE = 'pairing-done';

  /** @const */ var PAGE_NAMES = [
      PAGE_DEVICES_DISCOVERY,
      PAGE_DEVICE_SELECT,
      PAGE_DEVICE_NOT_FOUND,
      PAGE_ESTABLISHING_CONNECTION,
      PAGE_ESTABLISHING_CONNECTION_ERROR,
      PAGE_CODE_CONFIRMATION,
      PAGE_HOST_UPDATE,
      PAGE_HOST_CONNECTION_LOST,
      PAGE_ENROLLMENT_INTRODUCTION,
      PAGE_AUTHENTICATION,
      PAGE_HOST_ENROLLMENT,
      PAGE_HOST_ENROLLMENT_ERROR,
      PAGE_PAIRING_DONE];

  return {
    deviceSelectionChangedCallback_: null,
    gaiaHost_: null,
    pages_: null,

    /** @override */
    decorate: function() {
      this.initialize();

      this.pages_ = {};
      PAGE_NAMES.forEach(function(pageName) {
        var page = this.querySelector('.page-' + pageName);
        if (page === null)
          throw Error('Page "' + pageName + '" was not found.');
        page.hidden = true;
        this.pages_[pageName] = page;
      }, this);

      this.disableControls_(true);

      this.addContextObserver(CONTEXT_KEY_PAGE, this.pageChanged_);
      this.addContextObserver(CONTEXT_KEY_CONTROLS_DISABLED,
          this.disableControls_);

      cr.ui.List.decorate(this.deviceList_);
      this.deviceList_.selectionModel = new cr.ui.ListSingleSelectionModel();

      this.gaiaHost_ = new cr.login.GaiaAuthHost(this.gaiaFrame_);

      this.deviceSelectionChangedCallback_ =
          this.deviceSelectionChanged_.bind(this);
    },

    pageChanged_: function(newPage, oldPage) {
      this.throbber_.hidden = [PAGE_DEVICES_DISCOVERY,
                               PAGE_DEVICE_SELECT,
                               PAGE_ESTABLISHING_CONNECTION,
                               PAGE_HOST_UPDATE,
                               PAGE_HOST_CONNECTION_LOST,
                               PAGE_HOST_ENROLLMENT].indexOf(newPage) == -1;
      this.togglePage_(newPage);
      if (newPage == PAGE_DEVICE_SELECT) {
        this.addContextObserver(CONTEXT_KEY_DEVICES, this.setDeviceList_);
        this.addContextObserver(CONTEXT_KEY_SELECTED_DEVICE,
            this.setSelectedDevice_);
        this.setDeviceList_(this.context.get(CONTEXT_KEY_DEVICES));
        this.deviceList_.addEventListener('change',
            this.deviceSelectionChangedCallback_);
      } else if (oldPage == PAGE_DEVICE_SELECT) {
        this.removeContextObserver(this.setDeviceList_);
        this.removeContextObserver(this.setSelectedDevice_);
        this.deviceList_.removeEventListener('change',
            this.deviceSelectionChangedCallback_);
      }

      if (newPage == PAGE_CODE_CONFIRMATION) {
        // TODO(dzhioev): replace with i18n pattern.
        this.confirmationCodeLabel_.textContent =
            this.context.get(CONTEXT_KEY_CONFIRMATION_CODE) + '?';
      }

      if (newPage == PAGE_AUTHENTICATION) {
        this.gaiaHost_.load(cr.login.GaiaAuthHost.AuthMode.DEFAULT,
                            {},
                            this.onAuthCompleted_.bind(this));
      }

      if (newPage == PAGE_HOST_ENROLLMENT) {
        this.domainNameLabel_.textContent =
            this.context.get(CONTEXT_KEY_ENROLLMENT_DOMAIN);
      }

      this.pageNameLabel_.textContent = '<<<< ' + newPage + ' >>>>';
    },

    togglePage_: function(newPage) {
      PAGE_NAMES.forEach(function(pageName) {
        this.pages_[pageName].hidden = (pageName !== newPage);
      }, this);
    },

    setDeviceList_: function(deviceList) {
      this.deviceList_.removeEventListener('change',
          this.deviceSelectionChangedCallback_);

      this.deviceList_.dataModel = new cr.ui.ArrayDataModel(deviceList);
      this.setSelectedDevice_(this.context.get(CONTEXT_KEY_SELECTED_DEVICE));

      this.deviceList_.addEventListener('change',
          this.deviceSelectionChangedCallback_);
    },

    setSelectedDevice_: function(selectedDevice) {
      this.deviceList_.selectedItem = selectedDevice;
    },

    deviceSelectionChanged_: function() {
      var item = this.deviceList_.selectedItem;
      this.context.set(CONTEXT_KEY_SELECTED_DEVICE, item ? item : '');
      this.commitContextChanges();
    },

    disableControls_: function(disable) {
      this.querySelectorAll('button').forEach(function(button) {
        button.disabled = disable;
      });
      this.context.set(CONTEXT_KEY_CONTROLS_DISABLED, disable);
      this.commitContextChanges();
    },

    onAuthCompleted_: function(credentials) {
      this.context.set(CONTEXT_KEY_ACCOUNT_ID, credentials.email);
      this.commitContextChanges();
      this.send(login.Screen.CALLBACK_USER_ACTED, ACTION_ENROLL);
    }
  };
});

