// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'tts-subpage' is the collapsible section containing
 * text-to-speech settings.
 */

Polymer({
  is: 'settings-tts-subpage',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },
  },

});
