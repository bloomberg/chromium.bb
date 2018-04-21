// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @polymerBehavior */
const ButtonNavigationBehaviorImpl = {
  properties: {
    /**
     *  ID for forward button label, which must be translated for display.
     *
     *  Undefined if the visible page has no forward-navigation button.
     *
     *  @type {string|undefined}
     */
    forwardButtonTextId: String,

    /**
     *  ID for backward button label, which must be translated for display.
     *
     *  Undefined if the visible page has no backward-navigation button.
     *
     *  @type {string|undefined}
     */
    backwardButtonTextId: String,

    /**
     *  Translated text to display on the forward-naviation button.
     *
     *  Undefined if the visible page has no forward-navigation button.
     *
     *  @type {string|undefined}
     */
    forwardButtonText: {
      type: String,
      computed: 'computeButtonText_(forwardButtonTextId)',
    },

    /**
     *  Translated text to display on the backward-naviation button.
     *
     *  Undefined if the visible page has no backward-navigation button.
     *
     *  @type {string|undefined}
     */
    backwardButtonText: {
      type: String,
      computed: 'computeButtonText_(backwardButtonTextId)',
    },
  },

  /**
   * @param {string} buttonTextId Key for the localized string to appear on a
   *     button.
   * @return {string|undefined} The localized string corresponding to the key
   *     buttonTextId. Return value is undefined if buttonTextId is not a key
   *     for any localized string. Note: this includes the case in which
   *     buttonTextId is undefined.
   * @private
   */
  computeButtonText_(buttonTextId) {
    return this.i18nExists(buttonTextId) ? this.i18n(buttonTextId) : undefined;
  },
};

/** @polymerBehavior */
const ButtonNavigationBehavior = [
  I18nBehavior,
  ButtonNavigationBehaviorImpl,
];
