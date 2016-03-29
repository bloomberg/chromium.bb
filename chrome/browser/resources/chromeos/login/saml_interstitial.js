// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'saml-interstitial',

  properties: {
    domain: {
      type: String,
      observer: 'onDomainChanged_'
    },

    showDomainMessages_: {
      type: Boolean,
      value: false
    },
  },
  onDomainChanged_: function() {
    this.$.message.content =
      loadTimeData.getStringF('samlInterstitialMessage', this.domain);
    this.showDomainMessages_ = !!this.domain.length;
  },
  onSamlPageNextClicked_: function() {
    this.fire('samlPageNextClicked');
  },
  onSamlPageChangeAccountClicked_: function() {
    this.fire('samlPageChangeAccountClicked');
  },
});
