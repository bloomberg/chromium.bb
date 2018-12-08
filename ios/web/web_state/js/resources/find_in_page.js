// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('__crWeb.findInPage');

goog.require('__crWeb.base');

/**
 * Based on code from the Google iOS app.
 *
 * @fileoverview A find in page tool. It can:
 *   1. Search for given string in the DOM, and highlight them in yellow color;
 *   2. Allow users to navigate through all match results, and highlight the
 * selected one in orange color;
 */

(function() {
/**
 * Namespace for this file.
 */
__gCrWeb.findInPage = {};

 // Store common namespace object in a global __gCrWeb object referenced by a
 // string, so it does not get renamed by closure compiler during the
 // minification.
 __gCrWeb['findInPage'] = __gCrWeb.findInPage;

})();
