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
     * Use the string representing the origin or extension name as the page
     * title of the settings-subpage parent.
     */
    pageTitle: {
      type: String,
      notify: true,
    },

    /**
     * The amount of data stored for the origin.
     * @private
     */
    storedData_: {
      type: String,
      value: '',
    },

    /** @private */
    enableSiteSettings_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableSiteSettings');
      },
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
    if (this.enableSiteSettings_)
      this.$.usageApi.fetchUsageTotal(this.toUrl(this.origin).hostname);

    var siteDetailsPermissions =
        /** @type {!NodeList<!SiteDetailsPermissionElement>} */
        (this.root.querySelectorAll('site-details-permission'));

    this.browserProxy.getOriginPermissions(this.origin, this.getCategoryList_())
        .then((exceptionList) => {
          exceptionList.forEach((exception, i) => {
            // |exceptionList| should be in the same order as the category list,
            // which is in the same order as |siteDetailsPermissions|.
            siteDetailsPermissions[i].site = exception;
          });

          // The displayName won't change, so just use the first exception.
          assert(exceptionList.length > 0);
          this.pageTitle = exceptionList[0].displayName;
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
    // Since usage is only shown when "Site Settings" is enabled, don't clear it
    // when it's not shown.
    if (this.enableSiteSettings_)
      this.$.usageApi.clearUsage(
          this.toUrl(this.origin).href, this.storageType_);
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

    this.onCloseDialog_();
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

  /**
   * Checks whether the permission list is standalone or has a heading.
   * @return {string} CSS class applied when the permission list has no heading.
   * @private
   */
  permissionListClass_: function(hasHeading) {
    return hasHeading ? '' : 'without-heading';
  },

  /**
   * Checks whether this site has any usage information to show.
   * @return {boolean} Whether there is any usage information to show (e.g.
   *     disk or battery).
   * @private
   */
  hasUsage_: function(storage) {
    return storage != '';
  },

});
