// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview host pairing screen implementation.
 */

login.createScreen('HostPairingScreen', 'host-pairing', function() {
  'use strict';

  // Keep these constants synced with corresponding constants in
  // host_pairing_screen_actor.{h,cc}.
  /** @const */ var CONTEXT_KEY_PAGE = 'page';
  /** @const */ var CONTEXT_KEY_DEVICE_NAME = 'deviceName';
  /** @const */ var CONTEXT_KEY_CONFIRMATION_CODE = 'code';
  /** @const */ var CONTEXT_KEY_ENROLLMENT_DOMAIN = 'enrollmentDomain';
  /** @const */ var CONTEXT_KEY_UPDATE_PROGRESS = 'updateProgress';

  /** @const */ var PAGE_WELCOME = 'welcome';
  /** @const */ var PAGE_CODE_CONFIRMATION = 'code-confirmation';
  /** @const */ var PAGE_UPDATE = 'update';
  /** @const */ var PAGE_ENROLLMENT_INTRODUCTION = 'enrollment-introduction';
  /** @const */ var PAGE_ENROLLMENT = 'enrollment';
  /** @const */ var PAGE_ENROLLMENT_ERROR = 'enrollment-error';
  /** @const */ var PAGE_PAIRING_DONE = 'pairing-done';

  /** @const */ var PAGE_NAMES = [
      PAGE_WELCOME,
      PAGE_CODE_CONFIRMATION,
      PAGE_UPDATE,
      PAGE_ENROLLMENT_INTRODUCTION,
      PAGE_ENROLLMENT,
      PAGE_ENROLLMENT_ERROR,
      PAGE_PAIRING_DONE];

  return {
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

      this.addContextObserver(CONTEXT_KEY_PAGE, this.pageChanged_);
    },

    pageChanged_: function(newPage, oldPage) {
      this.pageNameLabel_.textContent = '<<<< ' + newPage + ' >>>>';
      this.deviceNameLabel_.textContent =
          this.context.get(CONTEXT_KEY_DEVICE_NAME);

      if (newPage == PAGE_CODE_CONFIRMATION)
        this.confirmationCodeLabel_.textContent =
            this.context.get(CONTEXT_KEY_CONFIRMATION_CODE);

      if (newPage == PAGE_UPDATE) {
        this.setUpdateProgress_(this.context.get(CONTEXT_KEY_UPDATE_PROGRESS));
        this.addContextObserver(CONTEXT_KEY_UPDATE_PROGRESS,
            this.setUpdateProgress_);
      } else if (oldPage == PAGE_UPDATE) {
        this.removeContextObserver(this.setUpdateProgress_);
      }

      if (newPage == PAGE_ENROLLMENT)
        this.domainNameLabel_.textContent =
            this.context.get(CONTEXT_KEY_ENROLLMENT_DOMAIN);

      this.togglePage_(newPage);
    },

    togglePage_: function(newPage) {
      PAGE_NAMES.forEach(function(pageName) {
        this.pages_[pageName].hidden = (pageName !== newPage);
      }, this);
    },

    setUpdateProgress_: function(progress) {
      this.updateProgressBar_.value = progress;
    }
  };
});

