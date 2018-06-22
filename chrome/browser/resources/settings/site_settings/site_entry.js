// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-entry' is an element representing a single eTLD+1 site entity.
 */

Polymer({
  is: 'site-entry',

  behaviors: [SiteSettingsBehavior],

  properties: {
    /**
     * An object representing a group of sites with the same eTLD+1.
     * @typedef {!SiteGroup}
     */
    siteGroup: {
      type: Object,
      observer: 'onSiteGroupChanged_',
    },

    /**
     * The name to display beside the icon. If grouped_() is true, it will be
     * the eTLD+1 for all the origins, otherwise, it will match the origin more
     * closely in an appropriate site representation.
     * @typedef {!string}
     * @private
     */
    displayName_: String,
  },

  /**
   * Whether the list of origins displayed in this site-entry is a group of
   * eTLD+1 origins or not.
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @return {boolean}
   * @private
   */
  grouped_: function(siteGroup) {
    return siteGroup.origins.length != 1;
  },

  /**
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @private
   */
  onSiteGroupChanged_: function(siteGroup) {
    // TODO(https://crbug.com/835712): Present the origin in a user-friendly
    // site representation when ungrouped.
    this.displayName_ =
        this.grouped_(siteGroup) ? siteGroup.etldPlus1 : siteGroup.origins[0];
  },

  /**
   * Get an appropriate favicon that represents this group of eTLD+1 sites as a
   * whole.
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @return {string} CSS to apply to show the appropriate favicon.
   * @private
   */
  getSiteGroupIcon_: function(siteGroup) {
    // TODO(https://crbug.com/835712): Implement heuristic for finding a good
    // favicon.
    return this.computeSiteIcon(siteGroup.origins[0]);
  },

  /**
   * Navigates to the corresponding Site Details page for the given origin.
   * @param {string} origin The origin to navigate to the Site Details page for
   * it.
   * @private
   */
  navigateToSiteDetails_: function(origin) {
    settings.navigateTo(
        settings.routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + origin));
  },

  /**
   * A handler for selecting a site (by clicking on the origin).
   * @param {!{model: !{index: !number}}} e
   * @private
   */
  onOriginTap_: function(e) {
    this.navigateToSiteDetails_(this.siteGroup.origins[e.model.index]);
  },

  /**
   * Toggles open and closed the list of origins if there is more than one, and
   * directly navigates to Site Details if there is only one site.
   * @param {!Object} e
   * @private
   */
  toggleCollapsible_: function(e) {
    // Individual origins don't expand - just go straight to Site Details.
    if (!this.grouped_(this.siteGroup)) {
      this.navigateToSiteDetails_(this.siteGroup.origins[0]);
      return;
    }

    let collapseChild =
        /** @type {IronCollapseElement} */ (this.$.collapseChild);
    collapseChild.toggle();
    this.$.toggleButton.setAttribute('aria-expanded', collapseChild.opened);
    this.$.expandIcon.toggleClass('icon-expand-more');
    this.$.expandIcon.toggleClass('icon-expand-less');
  },

  /**
   * Retrieves the overflow menu.
   * @private
   */
  getOverflowMenu_: function() {
    let menu = /** @type {?CrActionMenuElement} */ (this.$.menu.getIfExists());
    if (!menu)
      menu = /** @type {!CrActionMenuElement} */ (this.$.menu.get());
    return menu;
  },

  /**
   * Opens the overflow menu at event target.
   * @param {!{target: Element}} e
   * @private
   */
  showOverflowMenu_: function(e) {
    this.getOverflowMenu_().showAt(e.target);
  },

  /** @private */
  onCloseDialog_: function(e) {
    e.target.closest('cr-dialog').close();
    this.getOverflowMenu_().close();
  },

  /**
   * Confirms the resetting of all content settings for an origin.
   * @param {!{target: !Element}} e
   * @private
   */
  onConfirmResetSettings_: function(e) {
    e.preventDefault();
    this.$.confirmResetSettings.showModal();
  },

  /**
   * Resets all permissions for all origins listed in |siteGroup.origins|.
   * @param {!Event} e
   * @private
   */
  onResetSettings_: function(e) {
    const contentSettingsTypes = this.getCategoryList();
    for (let i = 0; i < this.siteGroup.origins.length; ++i) {
      const origin = this.siteGroup.origins[i];
      this.browserProxy.setOriginPermissions(
          origin, contentSettingsTypes, settings.ContentSetting.DEFAULT);
      if (contentSettingsTypes.includes(settings.ContentSettingsTypes.PLUGINS))
        this.browserProxy.clearFlashPref(origin);
    }
    this.onCloseDialog_(e);
  },

  /**
   * Formats the |label| string with |name|, using $<num> as markers.
   * @param {string} label
   * @param {string} name
   * @return {string}
   * @private
   */
  getFormatString_: function(label, name) {
    return loadTimeData.substituteString(label, name);
  },
});
