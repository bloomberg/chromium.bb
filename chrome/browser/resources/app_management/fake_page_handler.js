// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.dispatch_ = {};

cr.define('app_management', function() {
  /*
   * TODO(rekanorman): Should implement appManagement.mojom.PageHandlerInterface
   * once backend permissions are implemented.
   */
  class FakePageHandler {
    /**
     * @param {string} id
     * @param {Object=} config
     * @return {!App}
     */
    static createApp(id, config) {
      const permissionMap = {
        [TestPermissionTypeEnum.NOTIFICATIONS]: false,
        [TestPermissionTypeEnum.LOCATION]: false,
        [TestPermissionTypeEnum.CAMERA]: false,
        [TestPermissionTypeEnum.MICROPHONE]: false,
      };

      const app = {
        id: id,
        type: apps.mojom.AppType.kWeb,
        title: 'App Title',
        version: '5.1',
        size: '9.0MB',
        isPinned: apps.mojom.OptionalBool.kUnknown,
        permissions: permissionMap,
      };

      if (config) {
        Object.assign(app, config);
      }

      return app;
    }

    /**
     * TODO(rekanorman): Change type to appManagement.mojom.PageProxy once
     *   the App struct has a permissions field.
     * @param {appManagement.mojom.PageCallbackRouter} page
     */
    constructor(page) {
      /**
       * TODO(rekanorman): Change type to appManagement.mojom.PageProxy once
       *   the App struct has a permissions field.
       * @type {Object}
       */
      this.page = page;

      /** @type {!Array<App>} */
      this.apps_ = [];
    }

    getApps() {
      return Promise.resolve({apps: this.apps_});
    }

    /**
     * @param {!Array<App>} appList
     */
    setApps(appList) {
      this.apps_ = appList;
    }

    /**
     * @param {string} appId
     * @param {TestPermissionType} permissionType
     * @param {PermissionValue} newPermissionValue
     */
    setPermission(appId, permissionType, newPermissionValue) {
      const app = app_management.Store.getInstance().data.apps[appId];
      const newPermissions = Object.assign({}, app.permissions);
      newPermissions[permissionType] = newPermissionValue;
      const newApp = Object.assign({}, app, {permissions: newPermissions});
      this.page.onAppChanged.dispatch_(newApp);
    }

    /**
     * @param {string} appId
     */
    uninstall(appId) {
      this.page.onAppRemoved.dispatch_(appId);
    }
  }

  return {FakePageHandler: FakePageHandler};
});
