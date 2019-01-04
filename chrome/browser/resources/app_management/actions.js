// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Module for functions which produce action objects. These are
 * listed in one place to document available actions and their parameters.
 */

cr.define('app_management.actions', function() {
  /**
   * @param {App} app
   */
  function addApp(app) {
    return {
      name: 'add-app',
      app: app,
    };
  }

  /**
   * @param {App} update
   */
  function changeApp(update) {
    return {
      name: 'change-app',
      update: update,
    };
  }

  /**
   * @param {string} id
   */
  function removeApp(id) {
    return {
      name: 'remove-app',
      id: id,
    };
  }

  /**
   * @param {PageType} pageType
   * @param {string=} id
   */
  function changePage(pageType, id) {
    if (pageType == PageType.DETAIL && !id) {
      console.warn(
          'Tried to load app detail page without providing an app id.');
    }

    return {
      name: 'change-page',
      pageType: pageType,
      id: id,
    };
  }

  return {
    addApp: addApp,
    changeApp: changeApp,
    removeApp: removeApp,
    changePage: changePage,
  };
});
