// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-permission-toggle',

  properties: {
    /**
     * @type {App}
     */
    app: Object,

    /**
     * TODO(rekanorman) Change the annotation when app type can be correctly
     * sent.
     * A string version of the permission type, corresponding to a value of
     * the PwaPermissionType enum.
     * @type {string}
     */
    permissionType: String,
  },

  /**
   * @param {String} appId
   * @param {string} permissionType
   * @return {boolean}
   */
  getPermissionValueBool_: function(app, permissionType) {
    return app_management.util.getPermissionValueBool(app, permissionType);
  },

  /**
   * @private
   */
  togglePermission_: function() {
    /** @type {!Permission} */
    let newPermission;

    switch (app_management.util.getPermissionValueType(this.app)) {
      case PermissionValueType.kBool:
        newPermission =
            this.getNewPermissionBoolean_(this.app, this.permissionType);
        break;
      case PermissionValueType.kTriState:
        newPermission =
            this.getNewPermissionTriState_(this.app, this.permissionType);
        break;
      default:
        assertNotReached();
    }

    app_management.BrowserProxy.getInstance().handler.setPermission(
        this.app.id, newPermission);
  },

  /**
   * @private
   * @param {App} app
   * @param {string} permissionType
   * @return {!Permission}
   */
  getNewPermissionBoolean_: function(app, permissionType) {
    /** @type {number} */
    let newPermissionValue;

    switch (app_management.util.getPermissionValue(app, permissionType)) {
      case Bool.kFalse:
        newPermissionValue = Bool.kTrue;
        break;
      case Bool.kTrue:
        newPermissionValue = Bool.kFalse;
        break;
      default:
        assertNotReached();
    }

    return app_management.util.createPermission(
        PwaPermissionType[permissionType], PermissionValueType.kBool,
        newPermissionValue);
  },

  /**
   * @param {App} app
   * @param {string} permissionType
   * @private
   * @return {!Permission}
   */
  getNewPermissionTriState_: function(app, permissionType) {
    let newPermissionValue;

    switch (app_management.util.getPermissionValue(app, permissionType)) {
      case TriState.kBlock:
        newPermissionValue = TriState.kAllow;
        break;
      case TriState.kAsk:
        newPermissionValue = TriState.kAllow;
        break;
      case TriState.kAllow:
        // TODO(rekanorman): Eventually TriState.kAsk, but currently changing a
        // permission to kAsk then opening the site settings page for the app
        // produces the error:
        // "Only extensions or enterprise policy can change the setting to ASK."
        newPermissionValue = TriState.kBlock;
        break;
      default:
        assertNotReached();
    }

    return app_management.util.createPermission(
        PwaPermissionType[permissionType], PermissionValueType.kTriState,
        newPermissionValue);
  },
});
