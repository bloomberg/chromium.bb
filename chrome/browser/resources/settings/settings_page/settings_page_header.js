// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-page-header' shows a basic heading with a 'core-icon'.
 *
 * Example:
 *
 *    <cr-settings-page-header page="{{page}}">
 *    </cr-settings-page-header>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-page-header
 */
Polymer('cr-settings-page-header', {
  publish: {
    /**
     * Page to show a header for.
     *
     * @attribute page
     * @type Object
     * @default null
     */
    page: null,
  },
});
