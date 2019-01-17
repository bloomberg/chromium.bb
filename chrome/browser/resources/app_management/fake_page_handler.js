// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('app_management', function() {
  /**
   * @implements {appManagement.mojom.PageHandlerInterface}
   */
  class FakePageHandler {
    /**
     * @param {number} permissionId
     * @param {Object=} config
     * @return {!Permission}
     */
    static createPermission(permissionId, config) {
      const permission = app_management.util.createPermission(
          permissionId, PermissionValueType.kTriState, TriState.kBlock);

      if (config) {
        Object.assign(permission, config);
      }

      return permission;
    }

    /**
     * @param {string} id
     * @param {Object=} config
     * @return {!App}
     */
    static createApp(id, config) {
      const permissionIds = [
        PwaPermissionType.CONTENT_SETTINGS_TYPE_GEOLOCATION,
        PwaPermissionType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
        PwaPermissionType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
        PwaPermissionType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
      ];

      const permissions = {};

      for (const type of permissionIds) {
        permissions[type] = FakePageHandler.createPermission(type);
      }

      const app = {
        id: id,
        type: apps.mojom.AppType.kWeb,
        title: 'App Title',
        version: '5.1',
        size: '9.0MB',
        isPinned: apps.mojom.OptionalBool.kUnknown,
        permissions: permissions,
      };

      if (config) {
        Object.assign(app, config);
      }

      return app;
    }

    /**
     * @param {appManagement.mojom.PageInterface} page
     */
    constructor(page) {
      /** @type {appManagement.mojom.PageInterface} */
      this.page = page;

      /** @type {!Array<App>} */
      this.apps_ = [];
    }

    async getApps() {
      return {apps: this.apps_};
    }

    /**
     * @param {string} appId
     * @return {!Promise}
     */
    async getExtensionAppPermissionMessages(appId) {
      return [];
    }
    /**
     * @param {!Array<App>} appList
     */
    setApps(appList) {
      this.apps_ = appList;
    }

    /**
     * @param {string} appId
     * @param {OptionalBool} pinnedValue
     */
    setPinned(appId, pinnedValue) {
      const app = app_management.Store.getInstance().data.apps[appId];

      const newApp =
          /** @type {App} */ (Object.assign({}, app, {isPinned: pinnedValue}));
      this.page.onAppChanged(newApp);
    }

    /**
     * @param {string} appId
     * @param {Permission} permission
     */
    setPermission(appId, permission) {
      const app = app_management.Store.getInstance().data.apps[appId];

      // Check that the app had a previous value for the given permission
      assert(app.permissions[permission.permissionId]);

      const newPermissions = Object.assign({}, app.permissions);
      newPermissions[permission.permissionId] = permission;
      const newApp = /** @type {App} */ (
          Object.assign({}, app, {permissions: newPermissions}));
      this.page.onAppChanged(newApp);
    }

    /**
     * @param {string} appId
     */
    uninstall(appId) {
      this.page.onAppRemoved(appId);
    }

    /**
     * @param {string} appId
     */
    openNativeSettings(appId) {}
  }

  return {FakePageHandler: FakePageHandler};
});
