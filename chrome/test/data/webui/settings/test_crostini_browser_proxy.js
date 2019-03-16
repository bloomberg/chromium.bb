// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.CrostiniBrowserProxy} */
class TestCrostiniBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestCrostiniInstallerView',
      'requestRemoveCrostini',
      'getCrostiniSharedPathsDisplayText',
      'getCrostiniSharedUsbDevices',
      'setCrostiniUsbDeviceShared',
      'removeCrostiniSharedPath',
      'exportCrostiniContainer',
      'importCrostiniContainer',
    ]);
    this.enabled = false;
    this.sharedPaths = ['path1', 'path2'];
    this.sharedUsbDevices = [];
  }

  /** @override */
  requestCrostiniInstallerView() {
    this.methodCalled('requestCrostiniInstallerView');
    this.enabled = true;
  }

  /** override */
  requestRemoveCrostini() {
    this.methodCalled('requestRemoveCrostini');
    this.enabled = false;
  }

  /** override */
  getCrostiniSharedPathsDisplayText(paths) {
    this.methodCalled('getCrostiniSharedPathsDisplayText');
    return Promise.resolve(paths.map(path => path + '-displayText'));
  }

  /** @override */
  getCrostiniSharedUsbDevices() {
    this.methodCalled('getCrostiniSharedUsbDevices');
    return Promise.resolve(this.sharedUsbDevices);
  }

  /** @override */
  setCrostiniUsbDeviceShared(guid, shared) {
    this.methodCalled('setCrostiniUsbDeviceShared', [guid, shared]);
  }

  /** override */
  removeCrostiniSharedPath(path) {
    this.sharedPaths = this.sharedPaths.filter(p => p !== path);
  }
  /** @override */
  requestCrostiniInstallerStatus() {
    cr.webUIListenerCallback('crostini-installer-status-changed', false);
  }

  /** override */
  exportCrostiniContainer() {
    this.methodCalled('exportCrostiniContainer');
  }

  /** override */
  importCrostiniContainer() {
    this.methodCalled('importCrostiniContainer');
  }
}
