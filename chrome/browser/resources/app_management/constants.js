// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The number of apps displayed in app list in the main view before expanding.
 * @const {number}
 */
const NUMBER_OF_APPS_DISPLAYED_DEFAULT = 4;

/**
 * The maximum number of apps' titles previewed in notification sublabel.
 * @const {number}
 */
const APP_LIST_PREVIEW_APP_TITLES = 3;

/**
 * Enumeration of the different subpage types within the app management page.
 * @enum {number}
 * @const
 */
const PageType = {
  MAIN: 0,
  DETAIL: 1,
};

const TestPermissionTypeEnum = appManagement.mojom.TestPermissionType;
