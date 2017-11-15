// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  /** An extensions.Service implementation to be used in tests. */
  class TestService extends TestBrowserProxy {
    constructor() {
      super([
        'getExtensionsInfo',
        'getProfileConfiguration',
        'loadUnpacked',
        'retryLoadUnpacked',
        'setProfileInDevMode',
        'setShortcutHandlingSuspended',
        'updateAllExtensions',
        'updateExtensionCommand',
      ]);

      this.itemStateChangedTarget = new FakeChromeEvent();
      this.profileStateChangedTarget = new FakeChromeEvent();
    }

    /** @override */
    getProfileConfiguration() {
      this.methodCalled('getProfileConfiguration');
      return Promise.resolve({inDeveloperMode: false});
    }

    /** @override */
    getItemStateChangedTarget() {
      return this.itemStateChangedTarget;
    }

    /** @override */
    getProfileStateChangedTarget() {
      return this.profileStateChangedTarget;
    }

    /** @override */
    getExtensionsInfo() {
      this.methodCalled('getExtensionsInfo');
      return Promise.resolve([]);
    }

    /** @override */
    setShortcutHandlingSuspended(enable) {
      this.methodCalled('setShortcutHandlingSuspended', enable);
    }

    /** @override */
    updateExtensionCommand(item, commandName, keybinding) {
      this.methodCalled(
          'updateExtensionCommand', [item, commandName, keybinding]);
    }

    /** @override */
    loadUnpacked() {
      this.methodCalled('loadUnpacked');
      return Promise.resolve();
    }

    /** @override */
    retryLoadUnpacked(guid) {
      this.methodCalled('retryLoadUnpacked', guid);
      return Promise.resolve();
    }

    /** @override */
    setProfileInDevMode(inDevMode) {
      this.methodCalled('setProfileInDevMode', inDevMode);
    }

    /** @override */
    updateAllExtensions() {
      this.methodCalled('updateAllExtensions');
    }
  }

  return {
    TestService: TestService,
  };
});
