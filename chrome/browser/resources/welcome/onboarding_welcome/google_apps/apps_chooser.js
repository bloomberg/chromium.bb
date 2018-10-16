// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('nuxGoogleApps');

/**
 * @typedef {{
 *   id: number,
 *   name: string,
 *   icon: string,
 *   url: string,
 *   selected: boolean,
 * }}
 */
nuxGoogleApps.AppItem;

/**
 * @typedef {{
 *   item: !nuxGoogleApps.AppItem,
 *   set: function(string, boolean):void
 * }}
 */
nuxGoogleApps.AppItemModel;

Polymer({
  is: 'apps-chooser',
  properties: {
    /**
     * @type {!Array<!nuxGoogleApps.AppItem>}
     * @private
     */
    appList_: Array,

    hasAppsSelected: {
      type: Boolean,
      notify: true,
      value: true,
    }
  },

  /** @override */
  ready() {
    nux.NuxGoogleAppsProxyImpl.getInstance().getGoogleAppsList().then(list => {
      this.appList_ = list.map(app => {
        app.selected = true;  // default all items selected.
        return app;
      });
    });
  },

  /**
   * Returns an array of booleans for each selected app.
   * @return {!Array<boolean>}
   */
  getSelectedAppList() {
    return this.appList_.map(a => a.selected);
  },

  /**
   * Handle toggling the apps selected.
   * @param {!{model: !nuxGoogleApps.AppItemModel}} e
   * @private
   */
  onAppClick_: function(e) {
    e.model.set('item.selected', !e.model.item.selected);
    this.hasAppsSelected = this.computeHasAppsSelected_();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAppPointerDown_: function(e) {
    e.currentTarget.classList.remove('keyboard-focused');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAppKeyUp_: function(e) {
    e.currentTarget.classList.add('keyboard-focused');
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasAppsSelected_: function() {
    return this.appList_.some(a => a.selected);
  },
});
