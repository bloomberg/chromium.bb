// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-details' show the details (permissions and usage) for a given origin
 * under Site Settings.
 */
Polymer({
  is: 'site-details',

  behaviors: [SiteSettingsBehavior, settings.RouteObserverBehavior],

  properties: {
    /**
     * The origin that this widget is showing details for.
     * @private
     */
    origin: {
      type: String,
      observer: 'onOriginChanged_',
    },

    /**
     * The amount of data stored for the origin.
     * @private
     */
    storedData_: {
      type: String,
      value: '',
    },

    /**
     * The type of storage for the origin.
     * @private
     */
    storageType_: Number,
  },

  listeners: {
    'usage-deleted': 'onUsageDeleted_',
  },

  /** @override */
  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    var site = settings.getQueryParameters().get('site');
    if (!site)
      return;
    this.origin = site;
  },

  /**
   * Handler for when the origin changes.
   * @private
   */
  onOriginChanged_: function() {
    this.$.usageApi.fetchUsageTotal(this.toUrl(this.origin).hostname);

    var siteDetailsPermissions =
        /** @type{!NodeList<!SiteDetailsPermissionElement>} */
        (this.root.querySelectorAll('site-details-permission'));

    this.browserProxy.getOriginPermissions(this.origin, this.getCategoryList_())
        .then((exceptionList) => {
          exceptionList.forEach((exception, i) => {
            // |exceptionList| should be in the same order as the category list,
            // which is in the same order as |siteDetailsPermissions|.
            siteDetailsPermissions[i].site =
                /** @type {!RawSiteException} */ (exception);
          });
        });
  },

  /** @private */
  onCloseDialog_: function() {
    this.$.confirmDeleteDialog.close();
  },

  /**
   * Confirms the deletion of storage for a site.
   * @param {!Event} e
   * @private
   */
  onConfirmClearSettings_: function(e) {
    e.preventDefault();
    this.$.confirmDeleteDialog.showModal();
  },

  /**
   * Clears all data stored for the current origin.
   * @private
   */
  onClearStorage_: function() {
    this.$.usageApi.clearUsage(this.toUrl(this.origin).href, this.storageType_);
  },

  /**
   * Called when usage has been deleted for an origin.
   * @param {!{detail: !{origin: string}}} event
   * @private
   */
  onUsageDeleted_: function(event) {
    if (event.detail.origin == this.toUrl(this.origin).href)
      this.storedData_ = '';
  },

  /**
   * Resets all permissions and clears all data stored for the current origin.
   * @private
   */
  onClearAndReset_: function() {
    this.browserProxy.setOriginPermissions(
        this.origin, this.getCategoryList_(), settings.ContentSetting.DEFAULT);

    if (this.storedData_ != '')
      this.onClearStorage_();

    this.$.confirmDeleteDialog.close();
  },

  /**
   * Returns list of categories for each permission displayed in <site-details>.
   * @return {!Array<string>}
   * @private
   */
  getCategoryList_: function() {
    return Array.prototype.map.call(
        this.root.querySelectorAll('site-details-permission'), (element) => {
          return element.category;
        });
  },
});
