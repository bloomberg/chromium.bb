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
  };

  var ErrorPage = Polymer({
    is: 'extensions-error-page',

    behaviors: [Polymer.NeonAnimatableBehavior],

    properties: {
      /** @type {!chrome.developerPrivate.ExtensionInfo|undefined} */
      data: Object,

      /** @type {!extensions.ErrorPageDelegate|undefined} */
      delegate: Object,
    },

    ready: function() {
      /** @type {!extensions.AnimationHelper} */
      this.animationHelper = new extensions.AnimationHelper(this, this.$.main);
      this.animationHelper.setEntryAnimation(extensions.Animation.FADE_IN);
      this.animationHelper.setExitAnimation(extensions.Animation.SCALE_DOWN);
      this.sharedElements = {hero: this.$.main};
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
     * @param {!{model:Object}} e
     * @private
     */
    onDeleteErrorTap_: function(e) {
      this.delegate.deleteErrors(this.data.id, [e.model.item.id]);
    },
  });

  return {
    ErrorPage: ErrorPage,
    ErrorPageDelegate: ErrorPageDelegate,
  };
});
