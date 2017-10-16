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
  class ErrorPageDelegate {
    /**
     * @param {string} extensionId
     * @param {!Array<number>=} errorIds
     * @param {chrome.developerPrivate.ErrorType=} type
     */
    deleteErrors(extensionId, errorIds, type) {}

    /**
     * @param {chrome.developerPrivate.RequestFileSourceProperties} args
     * @return {!Promise<!chrome.developerPrivate.RequestFileSourceResponse>}
     */
    requestFileSource(args) {}

    /**
     * @param {!chrome.developerPrivate.OpenDevToolsProperties} args
     */
    openDevTools(args) {}
  }

  /**
   * Get the URL relative to the main extension url. If the url is
   * unassociated with the extension, this will be the full url.
   * @param {string} url
   * @param {?(ManifestError|RuntimeError)} error
   * @return {string}
   */
  function getRelativeUrl(url, error) {
    const fullUrl = 'chrome-extension://' + error.extensionId + '/';
    return url.startsWith(fullUrl) ? url.substring(fullUrl.length) : url;
  }

  const ErrorPage = Polymer({
    is: 'extensions-error-page',

    behaviors: [CrContainerShadowBehavior],

    properties: {
      /** @type {!chrome.developerPrivate.ExtensionInfo|undefined} */
      data: Object,

      /** @type {!extensions.ErrorPageDelegate|undefined} */
      delegate: Object,

      /** @private {?(ManifestError|RuntimeError)} */
      selectedError_: Object,

      /** @private {?chrome.developerPrivate.StackFrame}*/
      selectedStackFrame_: {
        type: Object,
        value: function() {
          return null;
        },
      },
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
      this.selectedError_ =
          this.data.manifestErrors[0] || this.data.runtimeErrors[0] || null;
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
      extensions.navigation.navigateTo({page: Page.LIST});
    },

    /**
     * @param {!ManifestError|!RuntimeError} error
     * @return {string}
     * @private
     */
    computeErrorIcon_: function(error) {
      if (error.type == chrome.developerPrivate.ErrorType.RUNTIME) {
        switch (error.severity) {
          case chrome.developerPrivate.ErrorLevel.LOG:
            return 'info';
          case chrome.developerPrivate.ErrorLevel.WARN:
            return 'warning';
          case chrome.developerPrivate.ErrorLevel.ERROR:
            return 'error';
        }
        assertNotReached();
      }
      assert(error.type == chrome.developerPrivate.ErrorType.MANIFEST);
      return 'warning';
    },

    /**
     * @param {!Event} e
     * @private
     */
    onDeleteErrorAction_: function(e) {
      if (e.type == 'keydown' && !((e.code == 'Space' || e.code == 'Enter')))
        return;

      this.delegate.deleteErrors(
          this.data.id, [(/** @type {!{model:Object}} */ (e)).model.item.id]);
      e.stopPropagation();
    },

    /**
     * Fetches the source for the selected error and populates the code section.
     * @private
     */
    onSelectedErrorChanged_: function() {
      if (!this.selectedError_) {
        this.$['code-section'].code = null;
        return;
      }

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
          this.selectedStackFrame_ = error.stackTrace && error.stackTrace[0] ?
              error.stackTrace[0] :
              null;
          break;
      }
      this.delegate.requestFileSource(args).then(code => {
        this.$['code-section'].code = code;
      });
    },

    /**
     * @return {boolean}
     * @private
     */
    computeIsRuntimeError_: function(item) {
      return item.type == chrome.developerPrivate.ErrorType.RUNTIME;
    },

    /**
     * The description is a human-readable summation of the frame, in the
     * form "<relative_url>:<line_number> (function)", e.g.
     * "myfile.js:25 (myFunction)".
     * @param {!chrome.developerPrivate.StackFrame} frame
     * @return {string}
     * @private
     */
    getStackTraceLabel_: function(frame) {
      let description = getRelativeUrl(frame.url, this.selectedError_) + ':' +
          frame.lineNumber;

      if (frame.functionName) {
        const functionName = frame.functionName == '(anonymous function)' ?
            loadTimeData.getString('anonymousFunction') :
            frame.functionName;
        description += ' (' + functionName + ')';
      }

      return description;
    },

    /** @private */
    getExpandedClass_: function() {
      return this.stackTraceExpanded_ ? 'expanded' : '';
    },

    /**
     * @param {chrome.developerPrivate.StackFrame} frame
     * @return {string}
     * @private
     */
    getStackFrameClass_: function(frame) {
      return frame == this.selectedStackFrame_ ? 'selected' : '';
    },

    /**
     * This function is used to determine whether or not we want to show a
     * stack frame. We don't want to show code from internal scripts.
     * @param {string} url
     * @return {boolean}
     * @private
     */
    shouldDisplayFrame_: function(url) {
      // All our internal scripts are in the 'extensions::' namespace.
      return !/^extensions::/.test(url);
    },

    /**
     * @param {!Event} e
     * @private
     */
    onStackFrameTap_: function(e) {
      const frame = (/** @type {!{model:Object}} */ (e)).model.item;

      this.selectedStackFrame_ = frame;

      this.delegate
          .requestFileSource({
            extensionId: this.selectedError_.extensionId,
            message: this.selectedError_.message,
            pathSuffix: getRelativeUrl(frame.url, this.selectedError_),
            lineNumber: frame.lineNumber,
          })
          .then(code => {
            this.$['code-section'].code = code;
          });
    },

    /** @private */
    onDevToolButtonTap_: function() {
      // This guarantees renderProcessId and renderViewId.
      assert(
          this.selectedError_.type ==
          chrome.developerPrivate.ErrorType.RUNTIME);
      assert(this.selectedStackFrame_);

      this.delegate.openDevTools({
        renderProcessId: this.selectedError_.renderProcessId,
        renderViewId: this.selectedError_.renderViewId,
        url: this.selectedStackFrame_.url,
        lineNumber: this.selectedStackFrame_.lineNumber || 0,
        columnNumber: this.selectedStackFrame_.columnNumber || 0,
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
      return selectedError == error ? 'selected' : '';
    },

    /**
     * @param {!RuntimeError|!ManifestError} selected
     * @param {!RuntimeError|!ManifestError} current
     * @return {boolean}
     * @private
     */
    isEqual_: function(selected, current) {
      return selected == current;
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
    onErrorItemAction_: function(e) {
      if (e.type == 'keydown' && !((e.code == 'Space' || e.code == 'Enter')))
        return;

      this.selectError_(e.model.item);
    },
  });

  return {
    ErrorPage: ErrorPage,
    ErrorPageDelegate: ErrorPageDelegate,
  };
});
