// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-item',

  properties: {
    /** @type {appManagement.mojom.App} */
    app: {
      type: Object,
    },
  },

  /**
   * @param {appManagement.mojom.App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_: function(app) {
    return `chrome://extension-icon/${app.id}/128/1`;
  },

});
