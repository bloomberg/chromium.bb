// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-enrollment-success-with-domain',
  behaviors: [I18nBehavior],
  properties: {
    /**
     * Domain the device was enrolled to.
     */
    enrollmentDomain: {
      type: String,
      value: '',
      reflectToAttribute: true,
    },

    /**
     * Name of the device that was enrolled.
     */
    deviceName: {
      type: String,
      value: 'Chromebook',
    },
  },

  localizedText_: function(locale, device, domain) {
    return this.i18nAdvanced(
        'oauthEnrollAbeSuccessDomain', {substitutions: [device, domain]});
  }
});
