// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'tts-subpage' is the collapsible section containing
 * text-to-speech settings.
 */

Polymer({
  is: 'settings-google-tts-engine-subpage',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * List of tts voices.
     * @private {!Array<
     *     !{language: string,
     *       name: string
     *      }>}
     */
    voiceList_: Array
  },

  /** @override */
  ready: function() {
    this.addWebUIListener(
        'google-voice-data-updated', this.onVoicesUpdated_.bind(this));
    chrome.send('getGoogleTtsVoiceData');
  },

  /** @private */
  onVoicesUpdated_: function(voices) {
    this.voiceList_ = voices;
  },

});
