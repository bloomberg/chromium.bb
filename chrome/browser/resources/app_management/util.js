// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions for the App Management page.
 */

cr.define('app_management.util', function() {
  /**
   * @return {!AppManagementPageState}
   */
  function createEmptyState() {
    return {
      apps: {},
      currentPage: {
        pageType: PageType.MAIN,
        selectedAppId: null,
      },
      search: {
        term: null,
        results: null,
      },
    };
  }

  /**
   * @param {!Array<App>} apps
   * @return {!AppManagementPageState}
   */
  function createInitialState(apps) {
    const initialState = createEmptyState();

    for (const app of apps) {
      initialState.apps[app.id] = app;
    }

    return initialState;
  }

  /**
   * @param {number} permissionId
   * @param {!PermissionValueType} valueType
   * @param {number} value
   * @return {!Permission}
   */
  function createPermission(permissionId, valueType, value) {
    return {
      permissionId: permissionId,
      valueType: valueType,
      value: value,
    };
  }

  /**
   * @param {App} app
   * @return {string}
   */
  function getAppIcon(app) {
    return `chrome://app-icon/${app.id}/128`;
  }

  /**
   * @return {!Array<!Array<App>>}
   */
  function splitByNotificationPermission() {
    const apps = Object.values(app_management.Store.getInstance().data.apps);
    const notificationsAllowed = [];
    const notificationsBlocked = [];

    for (const app of apps) {
      switch (notificationsValue(app)) {
        case OptionalBool.kUnknown:
          break;
        case OptionalBool.kFalse:
          notificationsBlocked.push(app);
          break;
        case OptionalBool.kTrue:
          notificationsAllowed.push(app);
          break;
        default:
          assertNotReached();
      }
    }

    return [notificationsAllowed, notificationsBlocked];
  }


  /**
   * @param {App} app
   * @return {OptionalBool}
   */
  function notificationsValue(app) {
    const permissionType = notificationsPermissionType(app);

    if (!permissionType) {
      return OptionalBool.kUnknown;
    }

    if (getPermissionValueBool(app, permissionType)) {
      return OptionalBool.kTrue;
    } else {
      return OptionalBool.kFalse;
    }
  }

  /**
   * Returns a string corresponding to the notifications value of the
   * appropriate permission type enum, based on the type of the app.
   * Returns null if the app type doesn't have a notifications permission.
   * @param {App} app
   * @return {?string}
   */
  function notificationsPermissionType(app) {
    switch (app.type) {
      case AppType.kWeb:
        return 'CONTENT_SETTINGS_TYPE_NOTIFICATIONS';
      // TODO(rekanorman): Add another case once notifications permissions
      // are implemented for ARC.
      default:
        return null;
    }
  }

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {boolean}
   */
  function getPermissionValueBool(app, permissionType) {
    const permission = getPermission(app, permissionType);
    assert(permission);

    switch (permission.valueType) {
      case PermissionValueType.kBool:
        return permission.value === Bool.kTrue;
      case PermissionValueType.kTriState:
        return permission.value === TriState.kAllow;
      default:
        assertNotReached();
    }
  }

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {Permission}
   */
  function getPermission(app, permissionType) {
    return app.permissions[permissionTypeHandle(app, permissionType)];
  }

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {number}
   */
  function permissionTypeHandle(app, permissionType) {
    switch (app.type) {
      case AppType.kWeb:
        return PwaPermissionType[permissionType];
      case AppType.kArc:
        return ArcPermissionType[permissionType];
      default:
        assertNotReached();
    }
  }

  /**
   * @param {AppManagementPageState} state
   * @return {?App}
   */
  function getSelectedApp(state) {
    const selectedAppId = state.currentPage.selectedAppId;
    return selectedAppId ? state.apps[selectedAppId] : null;
  }

  return {
    createEmptyState: createEmptyState,
    createInitialState: createInitialState,
    createPermission: createPermission,
    getAppIcon: getAppIcon,
    getPermission: getPermission,
    getPermissionValueBool: getPermissionValueBool,
    getSelectedApp: getSelectedApp,
    notificationsPermissionType: notificationsPermissionType,
    notificationsValue: notificationsValue,
    permissionTypeHandle: permissionTypeHandle,
    splitByNotificationPermission: splitByNotificationPermission,
  };
});
