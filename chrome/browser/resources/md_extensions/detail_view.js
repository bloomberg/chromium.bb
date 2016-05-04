// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  var DetailView = Polymer({
    is: 'extensions-detail-view',

    behaviors: [Polymer.NeonAnimatableBehavior, I18nBehavior],

    properties: {
      animationConfig: {
        type: Object,
        /** @suppress {checkTypes} Compiler doesn't recognize this.$.main. */
        value: function() {
          return {
            entry: [{
              name: 'hero-animation',
              id: 'hero',
              toPage: this
            }],
            exit: [{
              name: 'scale-down-animation',
              node: this.$.main,
              transformOrigin: '50% 50%',
              axis: 'y'
            }],
          };
        },
      },

      /**
       * The underlying ExtensionInfo for the details being displayed.
       * @type {chrome.developerPrivate.ExtensionInfo}
       */
      data: {
        type: Object,
      }
    },

    ready: function() {
      this.sharedElements = {hero: this.$.main};
    },

    /** @private */
    onCloseButtonClick_: function() {
      this.fire('close');
    },

    /**
     * @return {boolean}
     * @private
     */
    hasDependentExtensions_: function() {
      return this.data.dependentExtensions.length > 0;
    },

    /**
     * @return {boolean}
     * @private
     */
    shouldShowOptionsSection_: function() {
      // TODO(devlin): Also include run on all urls, file access, etc.
      return this.data.incognitoAccess.isEnabled;
    },
  });

  return {DetailView: DetailView};
});
