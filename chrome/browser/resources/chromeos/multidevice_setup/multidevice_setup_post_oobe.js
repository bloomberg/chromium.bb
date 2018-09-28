// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * MultiDevice setup flow which is shown after OOBE has completed.
 */
Polymer({
  is: 'multidevice-setup-post-oobe',

  properties: {
    /** @private {!multidevice_setup.MultiDeviceSetupDelegate} */
    delegate_: Object,
  },

  /** @override */
  attached: function() {
    this.delegate_ = new multidevice_setup.PostOobeDelegate();
  },

  /** @private */
  onExitRequested_: function() {
    chrome.send('dialogClose');
  },
});
