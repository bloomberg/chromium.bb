// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-app',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * @private {boolean}
     */
    mainViewSelected_: Boolean,

    /**
     * @private {boolean}
     */
    pwaPermissionViewSelected_: Boolean,
  },

  /** @override */
  attached: function() {
    this.watch('mainViewSelected_', function(state) {
      return state.currentPage.pageType == PageType.MAIN;
    });

    this.watch('pwaPermissionViewSelected_', function(state) {
      return state.currentPage.pageType == PageType.DETAIL;
    });
  },
});
