// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /** @interface */
  function LoadErrorDelegate() {}

  LoadErrorDelegate.prototype = {
    /** Attempts to load the previously-attempted unpacked extension. */
    retryLoadUnpacked: assertNotReached,
  };

  var LoadError = Polymer({
    is: 'extensions-load-error',
    properties: {
      /** @type {extensions.LoadErrorDelegate} */
      delegate: Object,

      error: String,

      filePath: String,
    },

    show: function() {
      this.$$('dialog').showModal();
    },

    close: function() {
      this.$$('dialog').close();
    },

    /** @private */
    onRetryTap_: function() {
      this.delegate.retryLoadUnpacked();
      this.close();
    },
  });

  return {LoadError: LoadError,
          LoadErrorDelegate: LoadErrorDelegate};
});
