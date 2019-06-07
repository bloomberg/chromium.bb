// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-search-page' contains assistant settings.
 */
Polymer({
  is: 'os-settings-search-page',

  behaviors: [I18nBehavior],

  properties: {
    prefs: Object,

    /** @type {?Map<string, string>} */
    focusConfig_: Object,
  },

  /** @override */
  ready: function() {
    assert(loadTimeData.getBoolean('isAssistantAllowed'));
    this.focusConfig_ = new Map();
    if (settings.routes.GOOGLE_ASSISTANT) {
      this.focusConfig_.set(
          settings.routes.GOOGLE_ASSISTANT.path,
          '#assistantSubpageTrigger .subpage-arrow');
    }
  },

  /** @private */
  onGoogleAssistantTap_: function() {
    settings.navigateTo(settings.routes.GOOGLE_ASSISTANT);
  },

  /**
   * @param {boolean} toggleValue
   * @return {string}
   * @private
   */
  getAssistantEnabledDisabledLabel_: function(toggleValue) {
    return this.i18n(
        toggleValue ? 'searchGoogleAssistantEnabled' :
                      'searchGoogleAssistantDisabled');
  },
});
