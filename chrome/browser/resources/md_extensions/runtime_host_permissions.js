// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  const RuntimeHostPermissions = Polymer({
    is: 'extensions-runtime-host-permissions',

    properties: {
      /**
       * The underlying permissions data.
       * @type {chrome.developerPrivate.Permissions}
       */
      permissions: Object,

      /** @private */
      itemId: String,

      /** @type {!extensions.ItemDelegate} */
      delegate: Object,

      /**
       * Whether the dialog to add a new host permission is shown.
       * @private
       */
      showHostsDialog_: Boolean,

      /**
       * Proxying the enum to be used easily by the html template.
       * @private
       */
      HostAccess_: {
        type: Object,
        value: chrome.developerPrivate.HostAccess,
      },
    },

    /**
     * @param {!Event} event
     * @private
     */
    onHostAccessChange_: function(event) {
      const select = /** @type {!HTMLSelectElement} */ (event.target);
      const access =
          /** @type {chrome.developerPrivate.HostAccess} */ (select.value);
      this.delegate.setItemHostAccess(this.itemId, access);
      // Force the UI to update (in order to potentially hide or show the
      // specific runtime hosts).
      // TODO(devlin): Perhaps this should be handled by the backend updating
      // and sending an onItemStateChanged event?
      this.set('permissions.hostAccess', access);
    },

    /**
     * @return {boolean}
     * @private
     */
    showSpecificSites_: function() {
      return this.permissions &&
          this.permissions.hostAccess ==
          chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES;
    },

    /** @private */
    onAddHostClick_: function() {
      this.showHostsDialog_ = true;
    },

    /** @private */
    onHostsDialogClosed_: function() {
      this.showHostsDialog_ = false;
      cr.ui.focusWithoutInk(assert(this.$$('#add-host')));
    },
  });

  return {RuntimeHostPermissions: RuntimeHostPermissions};
});
