// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @typedef {chrome.developerPrivate.ManifestError} */
let ManifestError;
/** @typedef {chrome.developerPrivate.RuntimeError} */
let RuntimeError;

cr.define('extensions', function() {
  'use strict';

  /** @interface */
  const ErrorPageDelegate = function() {};

  ErrorPageDelegate.prototype = {
    /**
     * @param {string} extensionId
     * @param {!Array<number>=} opt_errorIds
     * @param {chrome.developerPrivate.ErrorType=} opt_type
     */
    deleteErrors: assertNotReached,

    /**
     * @param {chrome.developerPrivate.RequestFileSourceProperties} args
     * @return {!Promise<!chrome.developerPrivate.RequestFileSourceResponse>}
     */
    requestFileSource: assertNotReached,
  };

  const ErrorPage = Polymer({
    is: 'extensions-error-page',

    properties: {
      /** @type {!chrome.developerPrivate.ExtensionInfo|undefined} */
      data: Object,

      /** @type {!extensions.ErrorPageDelegate|undefined} */
      delegate: Object,

      /** @private {?(ManifestError|RuntimeError)} */
      selectedError_: Object,
    },

    observers: [
      'observeDataChanges_(data)',
      'onSelectedErrorChanged_(selectedError_)',
    ],

    /**
     * Watches for changes to |data| in order to fetch the corresponding
     * file source.
     * @private
     */
    observeDataChanges_: function() {
      assert(this.data);
      const e = this.data.manifestErrors[0] || this.data.runtimeErrors[0];
      if (e)
        this.selectedError_ = e;
    },

    /**
     * @return {!Array<!(ManifestError|RuntimeError)>}
     * @private
     */
    calculateShownItems_: function() {
      return this.data.manifestErrors.concat(this.data.runtimeErrors);
    },

    /** @private */
    onCloseButtonTap_: function() {
      extensions.navigation.navigateTo({
        page: Page.LIST,
        type: extensions.getItemListType(
            /** @type {!chrome.developerPrivate.ExtensionInfo} */ (this.data))
      });
    },

    /**
     * @param {!ManifestError|!RuntimeError} error
     * @return {string}
     * @private
     */
    computeErrorIconClass_: function(error) {
      if (error.type == chrome.developerPrivate.ErrorType.RUNTIME) {
        switch (error.severity) {
          case chrome.developerPrivate.ErrorLevel.LOG:
            return 'icon-severity-info';
          case chrome.developerPrivate.ErrorLevel.WARN:
            return 'icon-severity-warning';
          case chrome.developerPrivate.ErrorLevel.ERROR:
            return 'icon-severity-fatal';
        }
        assertNotReached();
      }
      assert(error.type == chrome.developerPrivate.ErrorType.MANIFEST);
      return 'icon-severity-warning';
    },

    /**
     * @param {!Event} event
     * @private
     */
    onDeleteErrorTap_: function(event) {
      // TODO(devlin): It would be cleaner if we could cast this to a
      // PolymerDomRepeatEvent-type thing, but that doesn't exist yet.
      const e = /** @type {!{model:Object}} */ (event);
      this.delegate.deleteErrors(this.data.id, [e.model.item.id]);
    },

    /**
     * Fetches the source for the selected error and populates the code section.
     * @private
     */
    onSelectedErrorChanged_: function() {
      const error = this.selectedError_;
      const args = {
        extensionId: error.extensionId,
        message: error.message,
      };
      switch (error.type) {
        case chrome.developerPrivate.ErrorType.MANIFEST:
          args.pathSuffix = error.source;
          args.manifestKey = error.manifestKey;
          args.manifestSpecific = error.manifestSpecific;
          break;
        case chrome.developerPrivate.ErrorType.RUNTIME:
          // slice(1) because pathname starts with a /.
          args.pathSuffix = new URL(error.source).pathname.slice(1);
          args.lineNumber = error.stackTrace && error.stackTrace[0] ?
              error.stackTrace[0].lineNumber :
              0;
          break;
      }
      this.delegate.requestFileSource(args).then(code => {
        this.$['code-section'].code = code;
      });
    },

    /**
     * Computes the class name for the error item depending on whether its
     * the currently selected error.
     * @param {!RuntimeError|!ManifestError} selectedError
     * @param {!RuntimeError|!ManifestError} error
     * @return {string}
     * @private
     */
    computeErrorClass_: function(selectedError, error) {
      return selectedError == error ? 'error-item selected' : 'error-item';
    },

    /**
     * Causes the given error to become the currently-selected error.
     * @param {!RuntimeError|!ManifestError} error
     * @private
     */
    selectError_: function(error) {
      this.selectedError_ = error;
    },

    /**
     * @param {!{model: !{item: (!RuntimeError|!ManifestError)}}} e
     * @private
     */
    onErrorItemTap_: function(e) {
      this.selectError_(e.model.item);
    },

    /**
     * @param {!{model: !{item: (!RuntimeError|!ManifestError)}}} e
     * @private
     */
    onErrorItemKeydown_: function(e) {
      if (e.key == ' ' || e.key == 'Enter')
        this.selectError_(e.model.item);
    },
  });

  return {
    ErrorPage: ErrorPage,
    ErrorPageDelegate: ErrorPageDelegate,
  };
});
