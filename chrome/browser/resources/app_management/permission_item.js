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
     * the PwaPermissionType enum.
     * @type {string}
     */
    permissionType: String,

    /**
     * @type {App}
     */
    app: Object,

    /**
     * @private {PermissionValueType}
     */
    permissionValueType_: {
      type: Number,
      computed: 'getPermissionValueType_(app)',
    },

    /**
     * The semantics of permissionValue_ depend on permissionValueType_,
     * see chrome/services/app_service/public/mojom/types.mojom
     * @private {?number}
     */
    permissionValue_: {
      type: Number,
      computed: 'getPermissionValue_(app, permissionType)',
    },

    /** @type {string} */
    icon: String,
  },

  /**
   * @param {App} app
   * @return {number}
   * @private
   */
  getPermissionValueType_: function(app) {
    // TODO(rekanorman): Change to a suitable conditional statement once the
    // PermissionValueType corresponding to each AppType is known.
    return PermissionValueType.kTriState;
  },

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {?number}
   * @private
   */
  getPermissionValue_: function(app, permissionType) {
    if (!app) {
      return null;
    }

    // TODO(rekanorman): Remove once permissions are implemented for all
    // app types.
    if (Object.keys(app.permissions).length === 0) {
      return null;
    }

    // TODO(rekanorman): Make generic once permissions are implemented for
    // other app types.
    return app.permissions[PwaPermissionType[permissionType]].value;
  },

  /**
   * Returns a boolean representation of the permission value, which used to
   * determine the position of the permission toggle.
   * @param {PermissionValueType} permissionValueType
   * @param {number} permissionValue
   * @return {boolean}
   * @private
   */
  permissionValueBool_: function(permissionValueType, permissionValue) {
    return (permissionValueType === PermissionValueType.kBool &&
            permissionValue === Bool.kTrue) ||
        (permissionValueType === PermissionValueType.kTriState &&
         permissionValue === TriState.kAllow);
  },

  /**
   * @private
   */
  togglePermission_: function() {
    /** @type {!Permission} */
    let newPermission;

    switch (this.permissionValueType_) {
      case PermissionValueType.kBool:
        newPermission = this.getNewPermissionBoolean_();
        break;
      case PermissionValueType.kTriState:
        newPermission = this.getNewPermissionTriState_();
        break;
      default:
        assertNotReached();
    }

    app_management.BrowserProxy.getInstance().handler.setPermission(
        this.app.id, newPermission);
  },

  /**
   * @private
   * @return {!Permission}
   */
  getNewPermissionBoolean_: function() {
    /** @type {number} */
    let newPermissionValue;

    switch (this.permissionValue_) {
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
        PwaPermissionType[this.permissionType], PermissionValueType.kBool,
        newPermissionValue);
  },

  /**
   * @private
   * @return {!Permission}
   */
  getNewPermissionTriState_: function() {
    let newPermissionValue;

    switch (this.permissionValue_) {
      case TriState.kBlock:
        newPermissionValue = TriState.kAllow;
        break;
      case TriState.kAsk:
        newPermissionValue = TriState.kAllow;
        break;
      case TriState.kAllow:
        newPermissionValue = TriState.kAsk;
        break;
      default:
        assertNotReached();
    }

    return app_management.util.createPermission(
        PwaPermissionType[this.permissionType], PermissionValueType.kTriState,
        newPermissionValue);
  },
});
