// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-metadata-view',

  properties: {
    /** @type {appManagement.mojom.App} */
    app: {
      type: Object,
    },
  },

  /**
   * @return {string?}
   * @private
   */
  versionString_: function(app) {
    if (!app.version) {
      return null;
    }

    return loadTimeData.getStringF('version', assert(app.version));
  },

  /**
   * @return {string?}
   * @private
   */
  sizeString_: function(app) {
    if (!app.size) {
      return null;
    }

    return loadTimeData.getStringF('size', assert(app.size));
  },

});
