// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-metadata-view',

  properties: {
    /** @type {App} */
    app: {
      type: Object,
    },
  },

  /**
   * @param {App} app
   * @return bool
   * @private
   */
  pinToShelfToggleVisible_: function(app) {
    return !(app.isPinned === OptionalBool.kUnknown);
  },

  /**
   * Returns a bool representation of the app's isPinned value, used to
   * determine the position of the "Pin to Shelf" toggle.
   * @param {App} app
   * @return bool
   * @private
   */
  isPinned_: function(app) {
    return app.isPinned === OptionalBool.kTrue;
  },

  togglePinned_: function() {
    assert(this.app);

    let newPinnedValue;

    switch (this.app.isPinned) {
      case OptionalBool.kFalse:
        newPinnedValue = OptionalBool.kTrue;
        break;
      case OptionalBool.kTrue:
        newPinnedValue = OptionalBool.kFalse;
        break;
      default:
        assertNotReached();
    }

    app_management.BrowserProxy.getInstance().handler.setPinned(
        this.app.id, newPinnedValue);
  },

  /**
   * @param {App} app
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
   * @param {App} app
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
