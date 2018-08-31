// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  // A RegExp to roughly match acceptable patterns entered by the user.
  // exec'ing() this RegExp will match the following groups:
  // 0: Full matched string.
  // 1: Scheme + scheme separator (e.g., 'https://').
  // 2: Scheme only (e.g., 'https').
  // 3: Match subdomains ('*.').
  // 4: Hostname (e.g., 'example.com').
  // 5: Port, including ':' separator (e.g., ':80').
  // 6: Path, include '/' separator (e.g., '/*').
  const patternRegExp = new RegExp(
      '^' +
      // Scheme; optional.
      '((http|https|\\*)://)?' +
      // Include subdomains specifier; optional.
      '(\\*\\.)?' +
      // Hostname, required.
      '([a-z0-9\\.-]+\\.[a-z0-9]+)' +
      // Port, optional.
      '(:[0-9]+)?' +
      // Path, optional but if present must be '/' or '/*'.
      '(\\/\\*|\\/)?' +
      '$');

  function getPatternFromSite(site) {
    let res = patternRegExp.exec(site);
    assert(res);
    let scheme = res[1] || '*://';
    let host = (res[3] || '') + res[4];
    let port = res[5] || '';
    let path = '/*';
    return scheme + host + port + path;
  }

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
      if (this.currentSite !== null && this.currentSite !== undefined) {
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

      let valid = patternRegExp.test(this.site_);
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
      return this.inputInvalid_ || this.site_ === undefined ||
          this.site_.trim().length == 0;
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
      let pattern = getPatternFromSite(this.site_);
      this.delegate.addRuntimeHostPermission(this.itemId, pattern)
          .then(
              () => {
                this.$.dialog.close();
              },
              () => {
                this.inputInvalid_ = true;
              });
    },
  });

  return {
    RuntimeHostsDialog: RuntimeHostsDialog,
    getPatternFromSite: getPatternFromSite
  };
});
