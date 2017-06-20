// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /** @interface */
  function LoadErrorDelegate() {}

  LoadErrorDelegate.prototype = {
    /**
     * Attempts to load the previously-attempted unpacked extension.
     * @param {string} retryId
     */
    retryLoadUnpacked: assertNotReached,
  };

  var LoadError = Polymer({
    is: 'extensions-load-error',
    properties: {
      /** @type {extensions.LoadErrorDelegate} */
      delegate: Object,

      /** @type {chrome.developerPrivate.LoadError} */
      loadError: Object,
    },

    observers: [
      'observeLoadErrorChanges_(loadError)',
    ],

    show: function() {
      this.$$('dialog').showModal();
    },

    close: function() {
      this.$$('dialog').close();
    },

    /** @private */
    onRetryTap_: function() {
      this.delegate.retryLoadUnpacked(this.loadError.retryGuid);
      this.close();
    },

    /** @private */
    observeLoadErrorChanges_: function() {
      assert(this.loadError);
      var source = this.loadError.source;
      // CodeSection expects a RequestFileSourceResponse, rather than an
      // ErrorFileSource. Massage into place.
      // TODO(devlin): Make RequestFileSourceResponse use ErrorFileSource.
      /** @type {!chrome.developerPrivate.RequestFileSourceResponse} */
      var codeSectionProperties = {
        beforeHighlight: source ? source.beforeHighlight : '',
        highlight: source ? source.highlight : '',
        afterHighlight: source ? source.afterHighlight : '',
        title: '',
        message: this.loadError.error,
      };

      this.$.code.code = codeSectionProperties;
    },
  });

  return {LoadError: LoadError, LoadErrorDelegate: LoadErrorDelegate};
});
