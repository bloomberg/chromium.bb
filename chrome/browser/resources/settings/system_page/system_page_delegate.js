// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles interprocess communcation for the system page. */

cr.define('settings', function() {
  /** @interface */
  function SystemPageDelegate() {}

  SystemPageDelegate.prototype = {
    /** Allows the user to change native system proxy settings. */
    changeProxySettings: assertNotReached,
  };

  /**
   * @constructor
   * @implements {settings.SystemPageDelegate}
   */
  function SystemPageDelegateImpl() {}

  SystemPageDelegateImpl.prototype = {
    /** @override */
    changeProxySettings: function() {
      chrome.send('changeProxySettings');
    },
  };

  return {
    SystemPageDelegate: SystemPageDelegate,
    SystemPageDelegateImpl: SystemPageDelegateImpl,
  };
});
