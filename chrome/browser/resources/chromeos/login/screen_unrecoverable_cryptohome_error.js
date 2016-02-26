// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

login.createScreen('UnrecoverableCryptohomeErrorScreen',
                   'unrecoverable-cryptohome-error', function() {
  return {
    EXTERNAL_API: [
      'show'
    ],

    /** @override */
    decorate: function() {
      this.card_ = $('unrecoverable-cryptohome-error-card');
      this.throbber_ = $('unrecoverable-cryptohome-error-busy');

      this.card_.addEventListener('done', function(e) {
        this.setLoading_(true);
        if (e.detail.shouldSendFeedback) {
          chrome.send('sendFeedbackAndResyncUserData');
        } else {
          chrome.send('resyncUserData');
        }
      }.bind(this));
    },

    /**
     * Sets whether to show the loading throbber.
     * @param {boolean} loading
     */
    setLoading_: function(loading) {
      this.card_.hidden = loading;
      this.throbber_.hidden = !loading;
    },

    /**
     * Show password changed screen.
     */
    show: function() {
      this.setLoading_(false);

      Oobe.getInstance().headerHidden = true;
      Oobe.showScreen({id: SCREEN_UNRECOVERABLE_CRYPTOHOME_ERROR});
    }
  };
});
