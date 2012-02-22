// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('help', function() {
  /**
   * MoreInfoOverlay class
   * @constructor
   */
  function MoreInfoOverlay() {
  }

  cr.addSingletonGetter(MoreInfoOverlay);

  MoreInfoOverlay.prototype = {
    /**
     * Initialize the page.
     */
    initializePage: function() {
      var overlay = $('overlay');
      cr.ui.overlay.setupOverlay(overlay);
      overlay.addEventListener('closeOverlay', this.handleDismiss_.bind(this));

      var self = this;
      $('channel-changer').onchange = function(event) {
        self.channelSelectOnChanged_(event.target.value);
      }

      $('more-info-confirm').onclick = this.handleDismiss_.bind(this);
    },

    /**
     * Handles a click on the dismiss button.
     * @param {Event} e
     */
    handleDismiss_: function(e) {
      help.HelpPage.showOverlay(null);
    },

    channelSelectOnChanged_: function(value) {
      help.HelpPage.setReleaseChannel(value);
    },
  };

  // Export
  return {
    MoreInfoOverlay: MoreInfoOverlay
  };
});
