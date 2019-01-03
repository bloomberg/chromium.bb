// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for App Management.
 */

/**
 * TODO(rekanorman): Remove permissions field once backend permissions are
 *   implemented.
 * @typedef {appManagement.mojom.App | {permissions: PermissionMap}}
 */
let App;

/**
 * Maps app ids to Apps.
 * @typedef {!Object<string, App>}
 */
let AppMap;

/**
 * @typedef {{
 *   pageType: PageType,
 *   selectedAppId: ?string,
 * }}
 */
let Page;

/**
 * @typedef {{
 *   apps: AppMap,
 *   currentPage: Page,
 * }}
 */
let AppManagementPageState;

/**
 * TODO(rekanorman): Remove once backend permissions are implemented.
 * @typedef {appManagement.mojom.TestPermissionType}
 */
let TestPermissionType;

/**
 * TODO(rekanorman): Remove once backend permissions are implemented.
 * @typedef {boolean}
 */
let PermissionValue;

/**
 * TODO(rekanorman): Remove once backend permissions are implemented.
 * @typedef {Object<TestPermissionType, PermissionValue>}
 */
let PermissionMap;

/**
 * TODO(rekanorman): Remove once backend permissions are implemented.
 * @typedef {appManagement.mojom.PageHandlerInterface |
 * app_management.FakePageHandler}
 */
let PageHandlerInterface;
