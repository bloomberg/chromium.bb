// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

cr.define('settings', function() {
  /** @interface */
  /* #export */ class ExtensionControlBrowserProxy {
    // TODO(dbeam): should be be returning !Promise<boolean> to indicate whether
    // it succeeded?
    /** @param {string} extensionId */
    disableExtension(extensionId) {}

    /** @param {string} extensionId */
    manageExtension(extensionId) {}
  }

  /**
   * @implements {settings.ExtensionControlBrowserProxy}
   */
  /* #export */ class ExtensionControlBrowserProxyImpl {
    /** @override */
    disableExtension(extensionId) {
      chrome.send('disableExtension', [extensionId]);
    }

    /** @override */
    manageExtension(extensionId) {
      window.open('chrome://extensions?id=' + extensionId);
    }
  }

  cr.addSingletonGetter(ExtensionControlBrowserProxyImpl);

  // #cr_define_end
  return {
    ExtensionControlBrowserProxy: ExtensionControlBrowserProxy,
    ExtensionControlBrowserProxyImpl: ExtensionControlBrowserProxyImpl,
  };
});
