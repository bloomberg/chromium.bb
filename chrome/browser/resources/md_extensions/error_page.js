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

  /**
   * Given 3 strings, this function returns the correct one for the type of
   * error that |item| is.
   * @param {!ManifestError|!RuntimeError} item
   * @param {string} log
   * @param {string} warn
   * @param {string} error
   * @return {string}
   * @private
   */
  function getErrorSeverityText_(item, log, warn, error) {
    if (item.type == chrome.developerPrivate.ErrorType.RUNTIME) {
      switch (item.severity) {
        case chrome.developerPrivate.ErrorLevel.LOG:
          return log;
        case chrome.developerPrivate.ErrorLevel.WARN:
          return warn;
        case chrome.developerPrivate.ErrorLevel.ERROR:
          return error;
      }
      assertNotReached();
    }
    assert(item.type == chrome.developerPrivate.ErrorType.MANIFEST);
    return warn;
  }

  const ErrorPage = Polymer({
    is: 'extensions-error-page',

    behaviors: [CrContainerShadowBehavior],

    properties: {
      /** @type {!chrome.developerPrivate.ExtensionInfo|undefined} */
      data: Object,

      /** @type {!extensions.ErrorPageDelegate|undefined} */
      delegate: Object,

      /** @private {!Array<!(ManifestError|RuntimeError)>} */
      entries_: Array,

      /** @private {?chrome.developerPrivate.RequestFileSourceResponse} */
      code_: Object,

      /**
       * Index into |entries_|.
       * @private
       */
      selectedEntry_: {
        type: Number,
        observer: 'onSelectedErrorChanged_',
      },

      /** @private {?chrome.developerPrivate.StackFrame}*/
      selectedStackFrame_: {
        type: Object,
        value: function() {
          return null;
        },
      },
    },

    observers: [
      'observeDataChanges_(data.*)',
    ],

    /** @override */
    ready: function() {
      cr.ui.FocusOutlineManager.forDocument(document);
    },

    /** @return {!ManifestError|!RuntimeError} */
    getSelectedError: function() {
      return this.entries_[this.selectedEntry_];
    },

    /**
     * @param {!ManifestError|!RuntimeError} error
     * @param {string} unknown
     * @return {string}
     * @private
     */
    getContextUrl_: function(error, unknown) {
      return error.contextUrl ? getRelativeUrl(error.contextUrl, error) :
                                unknown;
    },

    /**
     * Watches for changes to |data| in order to fetch the corresponding
     * file source.
     * @private
     */
    observeDataChanges_: function() {
      const errors = this.data.manifestErrors.concat(this.data.runtimeErrors);
      this.entries_ = errors;
      this.selectedEntry_ = -1;  // This also help reset code-section content.
      if (this.entries_.length)
        this.selectedEntry_ = 0;
    },

    /** @private */
    onCloseButtonTap_: function() {
      extensions.navigation.navigateTo({page: Page.LIST});
    },

    /** @private */
    onClearAllTap_: function() {
      const ids = this.entries_.map(entry => entry.id);
      this.delegate.deleteErrors(this.data.id, ids);
    },

    /**
     * @param {!ManifestError|!RuntimeError} error
     * @return {string}
     * @private
     */
    computeErrorIcon_: function(error) {
      // Do not i18n these strings, they're CSS classes.
      return getErrorSeverityText_(error, 'info', 'warning', 'error');
    },

    /**
     * @param {!ManifestError|!RuntimeError} error
     * @return {string}
     * @private
     */
    computeErrorTypeLabel_: function(error) {
      return getErrorSeverityText_(
          error, loadTimeData.getString('logLevel'),
          loadTimeData.getString('warnLevel'),
          loadTimeData.getString('errorLevel'));
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
      this.code_ = null;

      if (this.selectedEntry_ < 0)
        return;

      const error = this.getSelectedError();
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
      this.delegate.requestFileSource(args).then(code => this.code_ = code);
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
      let description = getRelativeUrl(frame.url, this.getSelectedError()) +
          ':' + frame.lineNumber;

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
     * @param {!chrome.developerPrivate.StackFrame} frame
     * @return {number}
     * @private
     */
    getStackFrameTabIndex_: function(frame) {
      return frame == this.selectedStackFrame_ ? 0 : -1;
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
     * @param {!chrome.developerPrivate.StackFrame} frame
     * @private
     */
    updateSelected_: function(frame) {
      this.selectedStackFrame_ = assert(frame);

      const selectedError = this.getSelectedError();
      this.delegate
          .requestFileSource({
            extensionId: selectedError.extensionId,
            message: selectedError.message,
            pathSuffix: getRelativeUrl(frame.url, selectedError),
            lineNumber: frame.lineNumber,
          })
          .then(code => this.code_ = code);
    },

    /**
     * @param {!Event} e
     * @private
     */
    onStackFrameTap_: function(e) {
      const frame = /** @type {!{model:Object}} */ (e).model.item;
      this.updateSelected_(frame);
    },

    /**
     * @param {!Event} e
     * @private
     */
    onStackKeydown_: function(e) {
      let direction = 0;

      if (e.key == 'ArrowDown')
        direction = 1;
      else if (e.key == 'ArrowUp')
        direction = -1;
      else
        return;

      e.preventDefault();

      let list = e.target.parentElement.querySelectorAll('li');

      for (let i = 0; i < list.length; ++i) {
        if (list[i].classList.contains('selected')) {
          let polymerEvent = /** @type {!{model: !Object}} */ (e);
          let frame = polymerEvent.model.item.stackTrace[i + direction];
          if (frame) {
            this.updateSelected_(frame);
            list[i + direction].focus();  // Preserve focus.
          }
          return;
        }
      }
    },

    /** @private */
    onDevToolButtonTap_: function() {
      const selectedError = this.getSelectedError();
      // This guarantees renderProcessId and renderViewId.
      assert(selectedError.type == chrome.developerPrivate.ErrorType.RUNTIME);
      assert(this.selectedStackFrame_);

      this.delegate.openDevTools({
        renderProcessId: selectedError.renderProcessId,
        renderViewId: selectedError.renderViewId,
        url: this.selectedStackFrame_.url,
        lineNumber: this.selectedStackFrame_.lineNumber || 0,
        columnNumber: this.selectedStackFrame_.columnNumber || 0,
      });
    },

    /**
     * Computes the class name for the error item depending on whether its
     * the currently selected error.
     * @param {number} index
     * @return {string}
     * @private
     */
    computeErrorClass_: function(index) {
      return index == this.selectedEntry_ ? 'selected' : '';
    },

    /** @private */
    iconName_: function(index) {
      return index == this.selectedEntry_ ? 'icon-expand-less' :
                                            'icon-expand-more';
    },

    /**
     * Determine if the iron-collapse should be opened (expanded).
     * @param {number} index
     * @return {boolean}
     * @private
     */
    isOpened_: function(index) {
      return index == this.selectedEntry_;
    },

    /**
     * @param {!{type: string, code: string, model: !{index: number}}} e
     * @private
     */
    onErrorItemAction_: function(e) {
      if (e.type == 'keydown' && !((e.code == 'Space' || e.code == 'Enter')))
        return;

      this.selectedEntry_ =
          this.selectedEntry_ == e.model.index ? -1 : e.model.index;
    },
  });

  return {
    ErrorPage: ErrorPage,
    ErrorPageDelegate: ErrorPageDelegate,
  };
});
