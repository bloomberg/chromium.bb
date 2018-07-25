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
       * The site that this entry is currently managing. Only non-empty if this
       * is for editing an existing entry.
       * @type {?string}
       */
      currentSite: {
        type: String,
        value: null,
      },

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
      if (this.currentSite !== null) {
        this.site_ = this.currentSite;
        this.validate_();
      }
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
     * @return {string}
     * @private
     */
    computeDialogTitle_: function() {
      const stringId = this.currentSite === null ? 'runtimeHostsDialogTitle' :
                                                   'hostPermissionsEdit';
      return loadTimeData.getString(stringId);
    },

    /**
     * @return {boolean}
     * @private
     */
    computeSubmitButtonDisabled_: function() {
      return this.inputInvalid_ || this.site_.trim().length == 0;
    },

    /**
     * @return {string}
     * @private
     */
    computeSubmitButtonLabel_: function() {
      const stringId = this.currentSite === null ? 'add' : 'save';
      return loadTimeData.getString(stringId);
    },

    /** @private */
    onCancelTap_: function() {
      this.$.dialog.cancel();
    },

    /**
     * The tap handler for the submit button (adds the pattern and closes
     * the dialog).
     * @private
     */
    onSubmitTap_: function() {
      if (this.currentSite !== null) {
        // No change in values, so no need to update the delegate.
        if (this.currentSite == this.site_) {
          this.$.dialog.close();
          return;
        }

        // Changing the entry is done through a remove followed by an add.
        this.delegate.removeRuntimeHostPermission(this.itemId, this.currentSite)
            .then(() => {
              this.addPermission_();
            });
        return;
      }

      this.addPermission_();
    },

    /**
     * Adds the runtime host permission through the delegate. If successful,
     * closes the dialog; otherwise displays the invalid input message.
     * @private
     */
    addPermission_: function() {
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
