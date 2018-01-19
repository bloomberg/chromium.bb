// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {extensions.KioskBrowserProxy} */
class TestKioskBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'initializeKioskAppSettings',
      'getKioskAppSettings',
      'addKioskApp',
      'disableKioskAutoLaunch',
      'enableKioskAutoLaunch',
      'removeKioskApp',
      'setDisableBailoutShortcut',
    ]);
  }

  /** @override */
  initializeKioskAppSettings() {
    this.methodCalled('initializeKioskAppSettings');
    return Promise.resolve({
      kioskEnabled: true,
      autoLaunchEnabled: false
    });
  }

  /** @override */
  getKioskAppSettings() {
    this.methodCalled('getKioskAppSettings');
    return Promise.resolve({
      apps: [],
      disableBailout: false,
      hasAutoLaunchApp: false
    });
  }

  /** @override */
  addKioskApp(appId) {
    this.methodCalled('addKioskApp', appId);
  }

  /** @override */
  disableKioskAutoLaunch(appId) {
    this.methodCalled('disableKioskAutoLaunch', appId);
  }

  /** @override */
  enableKioskAutoLaunch(appId) {
    this.methodCalled('enableKioskAutoLaunch', appId);
  }

  /** @override */
  removeKioskApp(appId) {
    this.methodCalled('removeKioskApp', appId);
  }

  /** @override */
  setDisableBailoutShortcut(disableBailout) {
    this.methodCalled('setDisableBailoutShortcut', disableBailout);
  }
}