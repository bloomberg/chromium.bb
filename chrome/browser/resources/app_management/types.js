// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for App Management.
 */

/**
 * Maps app ids to Apps.
 * @typedef {!Object<string, appManagement.mojom.App>}
 */
let AppMap;

/**
 * @typedef {{
 *   apps: AppMap,
 * }}
 */
let AppManagementPageState;
