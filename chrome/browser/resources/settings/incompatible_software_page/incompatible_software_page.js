// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-incompatible-software-page' is the settings subpage containing
 * the list of incompatible software.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-incompatible-software-page">
 *      </settings-incompatible-software-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 */

Polymer({
  is: 'settings-incompatible-software-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Indicates if the current user has administrator rights.
     * @private
     */
    hasAdminRights_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('hasAdminRights');
      },
    },

    /**
     * The list of all the incompatible software.
     * @private {Array<settings.IncompatibleSoftware>}
     */
    softwareList_: Array,
  },

  /** @override */
  ready: function() {
    settings.IncompatibleSoftwareBrowserProxyImpl.getInstance()
        .requestIncompatibleSoftwareList()
        .then(list => {
          this.softwareList_ = list;
        });
  },
});
