// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-permission-item',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * The name of the permission, to be displayed to the user.
     * @type {string}
     */
    permissionLabel: String,

    /**
     * A string version of the permission type. Must be a value of the
     * permission type enum corresponding to the AppType of app_.
     * E.g. A value of PwaPermissionType if app_.type === AppType.kWeb.
     * @type {string}
     */
    permissionType: String,

    /**
     * @type {App}
     */
    app_: Object,

    /**
     * @type {string}
     */
    icon: String,
  },

  listeners: {
    'click': 'onClick_',
  },

  attached: function() {
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.updateFromStore();
  },

  /**
   * @private
   */
  onClick_: function(e) {
    e.preventDefault();

    const /** @type {AppManagementPermissionToggleElement} */ toggle =
        this.$['permission-toggle'];
    toggle.togglePermission_();
  },
});
