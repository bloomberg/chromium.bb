// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-data' handles showing the local storage summary list for all sites.
 */

/**
 * TODO(dbeam): upstream to polymer externs?
 * @constructor
 * @extends {Event}
 */
function DomRepeatEvent() {}

/** @type {?} */
DomRepeatEvent.prototype.model;

Polymer({
  is: 'site-data',

  behaviors: [
    CookieTreeBehavior,
    I18nBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * The current filter applied to the cookie data list.
     */
    filter: {
      observer: 'onSearchChanged_',
      notify: true,
      type: String,
      value: '',
    },

    /** @type {!Map<string, string>} */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
    },
  },

  /**
   * Reload cookies when the site data page is visited.
   *
   * settings.RouteObserverBehavior
   * @param {!settings.Route} currentRoute
   * @protected
   */
  currentRouteChanged: function(currentRoute) {
    if (currentRoute == settings.routes.SITE_SETTINGS_SITE_DATA) {
      this.loadCookies();
    }
  },

  /**
   * Returns the icon to use for a given site.
   * @param {string} url The url of the site to fetch the icon for.
   * @return {string} Value for background-image style.
   * @private
   */
  favicon_: function(url) {
    return cr.icon.getFavicon(url);
  },

  /**
   * @param {!Map<string, string>} newConfig
   * @param {?Map<string, string>} oldConfig
   * @private
   */
  focusConfigChanged_: function(newConfig, oldConfig) {
    // focusConfig is set only once on the parent, so this observer should only
    // fire once.
    assert(!oldConfig);

    // Populate the |focusConfig| map of the parent <settings-animated-pages>
    // element, with additional entries that correspond to subpage trigger
    // elements residing in this element's Shadow DOM.
    if (settings.routes.SITE_SETTINGS_DATA_DETAILS) {
      this.focusConfig.set(
          settings.routes.SITE_SETTINGS_DATA_DETAILS.path,
          '* /deep/ #filter /deep/ #searchInput');
    }
  },

  /**
   * A filter function for the list.
   * @param {!CookieDataSummaryItem} item The item to possibly filter out.
   * @return {boolean} Whether to show the item.
   * @private
   */
  showItem_: function(item) {
    if (this.filter.length == 0)
      return true;
    return item.site.indexOf(this.filter) > -1;
  },

  /** @private */
  onSearchChanged_: function() {
    this.$.list.render();
  },

  /**
   * @return {boolean} Whether to show the multiple site remove button.
   * @private
   */
  isRemoveButtonVisible_: function(sites, renderedItemCount) {
    return renderedItemCount != 0;
  },

  /**
   * Returns the string to use for the Remove label.
   * @return {string} filter The current filter string.
   * @private
   */
  computeRemoveLabel_: function(filter) {
    if (filter.length == 0)
      return loadTimeData.getString('siteSettingsCookieRemoveAll');
    return loadTimeData.getString('siteSettingsCookieRemoveAllShown');
  },

  /** @private */
  onCloseDialog_: function() {
    this.$.confirmDeleteDialog.close();
  },

  /** @private */
  onConfirmDeleteDialogClosed_: function() {
    cr.ui.focusWithoutInk(assert(this.$.removeShowingSites));
  },

  /**
   * Shows a dialog to confirm the deletion of multiple sites.
   * @param {!Event} e
   * @private
   */
  onRemoveShowingSitesTap_: function(e) {
    e.preventDefault();
    this.$.confirmDeleteDialog.showModal();
  },

  /**
   * Called when deletion for all showing sites has been confirmed.
   * @private
   */
  onConfirmDelete_: function() {
    this.$.confirmDeleteDialog.close();

    if (this.filter.length == 0) {
      this.removeAllCookies();
    } else {
      var items = this.$.list.items;
      for (var i = 0; i < items.length; ++i) {
        if (this.showItem_(items[i]))
          this.browserProxy_.removeCookie(items[i].id);
      }
      // We just deleted all items found by the filter, let's reset the filter.
      this.fire('clear-subpage-search');
    }
  },

  /**
   * Deletes all site data for a given site.
   * @param {!DomRepeatEvent} e
   * @private
   */
  onRemoveSiteTap_: function(e) {
    e.stopPropagation();
    this.browserProxy_.removeCookie(e.model.item.id);
  },

  /**
   * @param {!{model: !{item: CookieDataSummaryItem}}} event
   * @private
   */
  onSiteTap_: function(event) {
    settings.navigateTo(
        settings.routes.SITE_SETTINGS_DATA_DETAILS,
        new URLSearchParams('site=' + event.model.item.site));
  },
});
