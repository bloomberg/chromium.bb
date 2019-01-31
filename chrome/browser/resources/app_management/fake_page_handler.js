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
     * @param {Object=} optConfig
     * @return {!Permission}
     */
    static createPermission(permissionId, optConfig) {
      // Changing to kAllow to test notifications sublabel collapsibility, as it
      // assumes all apps have notification permission.
      const permission = app_management.util.createPermission(
          permissionId, PermissionValueType.kTriState, TriState.kAllow);

      if (optConfig) {
        Object.assign(permission, optConfig);
      }

      return permission;
    }

    /**
     * @param {string} id
     * @param {Object=} optConfig
     * @return {!App}
     */
    static createApp(id, optConfig) {
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
        description: '',
        version: '5.1',
        size: '9.0MB',
        isPinned: apps.mojom.OptionalBool.kFalse,
        permissions: permissions,
      };

      if (optConfig) {
        Object.assign(app, optConfig);
      }

      return app;
    }

    /**
     * @param {appManagement.mojom.PageProxy} page
     */
    constructor(page) {
      /** @type {appManagement.mojom.PageProxy} */
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

    /**
     * @param {string} id
     * @param {Object=} optConfig
     */
    async addApp(id, optConfig) {
      this.page.onAppAdded(FakePageHandler.createApp(id, optConfig));
      await this.flushForTesting();
    }

    /**
     * Takes an app id and an object mapping app fields to the values they
     * should be changed to, and dispatches an action to carry out these
     * changes.
     * @param {string} id
     * @param {Object} changes
     */
    async changeApp(id, changes) {
      this.page.onAppChanged(FakePageHandler.createApp(id, changes));
      await this.flushForTesting();
    }

    async flushForTesting() {
      await this.page.flushForTesting();
    }
  }

  return {FakePageHandler: FakePageHandler};
});
