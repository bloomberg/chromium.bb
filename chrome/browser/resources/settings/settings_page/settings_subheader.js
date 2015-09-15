// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-subheader' shows a subheader for subpages. This header contains
 * the subpage title and a back icon. The back icon fires an event which
 * is caught by settings-animated-pages, so it requires no separate handling.
 *
 * Examples:
 *
 *    <settings-subheader i18n-values="page-title:internetPageTitle">
 *    </settings-subheader>
 *
 * @group Chrome Settings Elements
 * @element settings-subheader
 */
Polymer({
  is: 'settings-subheader',

  /** @private */
  onTapBack_: function() {
    // Event is caught by settings-animated-pages.
    this.fire('subpage-back');
  },
});
