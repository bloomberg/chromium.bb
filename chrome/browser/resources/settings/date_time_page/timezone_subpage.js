// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'timezone-subpage' is the collapsible section containing
 * time zone settings.
 */
(function() {
'use strict';

Polymer({
  is: 'timezone-subpage',

  behaviors: [PrefsBehavior],

  properties: {
    /**
     * This is <timezone-selector> parameter.
     */
    activeTimeZoneDisplayName: {
      type: String,
      notify: true,
    },

    /**
     * The effective time zone auto-detect enabled/disabled status.
     */
    timeZoneAutoDetect: Boolean,

    /**
     * settings.TimeZoneAutoDetectMethod values.
     * @private {!Object<settings.TimeZoneAutoDetectMethod, number>}
     */
    timezoneAutodetectMethodValues_: Object,

  },

  attached: function() {
    this.timezoneAutodetectMethodValues_ = settings.TimeZoneAutoDetectMethod;
  },
});
})();
