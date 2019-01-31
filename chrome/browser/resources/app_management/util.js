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
   * Returns true if the current page is notification view.
   * @param {AppManagementPageState} state
   * @return {boolean}
   */
  function isNotificationsViewSelected(state) {
    return state.currentPage.pageType === PageType.NOTIFICATIONS;
  }

  /**
   * @return {!Array<Array<App>>}
   */
  function splitByNotificationPermission() {
    const apps = Object.values(app_management.Store.getInstance().data.apps);
    const notificationsAllowed = [];
    const notificationsBlocked = [];
    for (const app of apps) {
      // Chrome app's notification permission cannot be changed and therefore
      // are not included in notification view
      if (app.type === AppType.kExtension) {
        continue;
      }
      // TODO(rekanorman) Branch on different app type in the future.
      if (getPermissionValueBool(app, 'CONTENT_SETTINGS_TYPE_NOTIFICATIONS')) {
        notificationsAllowed.push(app);
      } else {
        notificationsBlocked.push(app);
      }
    }
    return [notificationsAllowed, notificationsBlocked];
  }

  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {boolean}
   */
  function getPermissionValueBool(app, permissionType) {
    if (getPermissionValueType(app) === PermissionValueType.kBool &&
        getPermissionValue(app, permissionType) === Bool.kTrue) {
      return true;
    }
    if (getPermissionValueType(app) === PermissionValueType.kTriState &&
        getPermissionValue(app, permissionType) === TriState.kAllow) {
      return true;
    }
    return false;
  }

  /**
   * @param {App} app
   * @return {number}
   * @private
   */
  function getPermissionValueType(app) {
    // TODO(rekanorman): Change to a suitable conditional statement once the
    // PermissionValueType corresponding to each AppType is known.
    return PermissionValueType.kTriState;
  }


  /**
   * @param {App} app
   * @param {string} permissionType
   * @return {?number}
   * @private
   */
  function getPermissionValue(app, permissionType) {
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
    return /** @type Object<number, Permission> */ (
               app.permissions)[PwaPermissionType[permissionType]]
        .value;
  }

  /**
   * If there is no selected app, returns undefined so that Polymer bindings
   * won't be updated.
   * @param {AppManagementPageState} state
   * @return {App|undefined}
   */
  function getSelectedApp(state) {
    const selectedAppId = state.currentPage.selectedAppId;
    if (selectedAppId) {
      return state.apps[selectedAppId];
    }
  }

  return {
    createEmptyState: createEmptyState,
    createInitialState: createInitialState,
    createPermission: createPermission,
    getAppIcon: getAppIcon,
    getPermissionValue: getPermissionValue,
    getPermissionValueBool: getPermissionValueBool,
    getPermissionValueType: getPermissionValueType,
    getSelectedApp: getSelectedApp,
    isNotificationsViewSelected: isNotificationsViewSelected,
    splitByNotificationPermission: splitByNotificationPermission,
  };
});
