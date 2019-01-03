// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-permission-item',

  properties: {
    /**
     * The name of the permission, to be displayed to the user.
     * @type {string}
     */
    permissionLabel: String,

    /**
     * A string version of the permission type, corresponding to a value of
     * the TestPermissionType enum.
     * @type {string}
     */
    permissionType: String,

    /**
     * @type {App}
     */
    app: Object,

    /**
     * @private {PermissionValue}
     */
    permissionValue_: {
      type: Boolean,
      computed: 'getPermissionValue_(app, permissionType)',
    },
  },

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {PermissionValue}
   * @private
   */
  getPermissionValue_: function(app, permissionType) {
    if (!app) {
      return false;
    }
    return app.permissions[TestPermissionTypeEnum[permissionType]];
  },

  /**
   * @private
   */
  togglePermission_: function(e) {
    const newPermissionValue = !this.permissionValue_;
    const permissionType = TestPermissionTypeEnum[this.permissionType];

    app_management.BrowserProxy.getInstance().handler.setPermission(
        this.app.id, permissionType, newPermissionValue);
  },
});
