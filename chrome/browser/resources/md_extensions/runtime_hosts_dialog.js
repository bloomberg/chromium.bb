// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  const RuntimeHostsDialog = Polymer({
    is: 'extensions-runtime-hosts-dialog',

    properties: {
      /** @type {!extensions.ItemDelegate} */
      delegate: Object,

      /** @type {string} */
      itemId: String,

      /**
       * The site to add an exception for.
       * @private
       */
      site_: String,

      /**
       * Whether the currently-entered input is valid.
       * @private
       */
      inputInvalid_: {
        type: Boolean,
        value: false,
      },
    },

    /** @override */
    attached: function() {
      this.$.dialog.showModal();
    },

    /**
     * Validates that the pattern entered is valid.
     * @private
     */
    validate_: function() {
      // If input is empty, disable the action button, but don't show the red
      // invalid message.
      if (this.site_.trim().length == 0) {
        this.inputInvalid_ = false;
        return;
      }

      let valid = true;
      try {
        // Check if the input parses as a valid URL. JS URL parsing isn't
        // great, but it's quick and easy and better than nothing. If it's still
        // not valid, the API will throw an error and the embedding element will
        // call setInputInvalid().
        let testUrl = new URL(this.site_);
      } catch (e) {
        valid = false;
      }

      this.inputInvalid_ = !valid;
    },

    /**
     * @return {boolean}
     * @private
     */
    computeAddButtonDisabled_: function() {
      return this.inputInvalid_ || this.site_.trim().length == 0;
    },

    /** @private */
    onCancelTap_: function() {
      this.$.dialog.cancel();
    },

    /**
     * The tap handler for the Add [Site] button (adds the pattern and closes
     * the dialog).
     * @private
     */
    onAddTap_: function() {
      this.delegate.addRuntimeHostPermission(this.itemId, this.site_)
          .then(
              () => {
                this.$.dialog.close();
              },
              () => {
                this.inputInvalid_ = true;
              });
    },
  });

  return {RuntimeHostsDialog: RuntimeHostsDialog};
});
