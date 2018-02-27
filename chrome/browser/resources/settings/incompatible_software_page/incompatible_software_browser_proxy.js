// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the Incompatible Software section to
 * interact with the browser.
 */

cr.exportPath('settings');

/**
 * All possible actions to take on am incompatible software.
 *
 * Must be kept in sync with BlacklistMessageType in
 * chrome/browser/conflicts/proto/module_list.proto
 * @readonly
 * @enum {number}
 */
settings.ActionTypes = {
  UNINSTALL: 0,
  MORE_INFO: 1,
  UPGRADE: 2,
};

/**
 * @typedef {{
 *   name: string,
 *   actionType: {settings.ActionTypes},
 *   actionUrl: string,
 * }}
 */
settings.IncompatibleSoftware;

cr.define('settings', function() {
  /** @interface */
  class IncompatibleSoftwareBrowserProxy {
    /**
     * Get the list of incompatible software.
     * @return {!Promise<!Array<!settings.IncompatibleSoftware>>}
     */
    requestIncompatibleSoftwareList() {}

    /**
     * Launches the Apps & Features page that allows uninstalling 'programName'.
     * @param {string} programName
     */
    startProgramUninstallation(programName) {}

    /**
     * Opens the specified URL in a new tab.
     * @param {!string} url
     */
    openURL(url) {}
  }

  /** @implements {settings.IncompatibleSoftwareBrowserProxy} */
  class IncompatibleSoftwareBrowserProxyImpl {
    /** @override */
    requestIncompatibleSoftwareList() {
      return cr.sendWithPromise('requestIncompatibleSoftwareList');
    }

    /** @override */
    startProgramUninstallation(programName) {
      chrome.send('startProgramUninstallation', [programName]);
    }

    /** @override */
    openURL(url) {
      window.open(url);
    }
  }

  cr.addSingletonGetter(IncompatibleSoftwareBrowserProxyImpl);

  return {
    IncompatibleSoftwareBrowserProxy: IncompatibleSoftwareBrowserProxy,
    IncompatibleSoftwareBrowserProxyImpl: IncompatibleSoftwareBrowserProxyImpl,
  };
});
