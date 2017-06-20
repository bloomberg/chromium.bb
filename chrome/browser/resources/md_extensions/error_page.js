// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @typedef {chrome.developerPrivate.ManifestError} */
var ManifestError;
/** @typedef {chrome.developerPrivate.RuntimeError} */
var RuntimeError;

cr.define('extensions', function() {
  'use strict';

  /** @interface */
  var ErrorPageDelegate = function() {};

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

  var ErrorPage = Polymer({
    is: 'extensions-error-page',

    behaviors: [Polymer.NeonAnimatableBehavior],

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

    ready: function() {
      /** @type {!extensions.AnimationHelper} */
      this.animationHelper = new extensions.AnimationHelper(this, this.$.main);
      this.animationHelper.setEntryAnimations([extensions.Animation.FADE_IN]);
      this.animationHelper.setExitAnimations([extensions.Animation.SCALE_DOWN]);
      this.sharedElements = {hero: this.$.main};
    },

    /**
     * Watches for changes to |data| in order to fetch the corresponding
     * file source.
     * @private
     */
    observeDataChanges_: function() {
      assert(this.data);
      var e = this.data.manifestErrors[0] || this.data.runtimeErrors[0];
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
      this.fire('close');
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
      var e = /** @type {!{model:Object}} */ (event);
      this.delegate.deleteErrors(this.data.id, [e.model.item.id]);
    },

    /**
     * Fetches the source for the selected error and populates the code section.
     * @private
     */
    onSelectedErrorChanged_: function() {
      var error = this.selectedError_;
      var args = {
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
      this.delegate.requestFileSource(args).then(function(code) {
        this.$['code-section'].code = code;
      }.bind(this));
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
