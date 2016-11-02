// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

/**
 * 'site-data-details-subpage' Display cookie contents.
 */
Polymer({
  is: 'site-data-details-subpage',

  behaviors: [settings.RouteObserverBehavior, CookieTreeBehavior],

  properties: {
    /**
     * The cookie entries for the given site.
     * @type {!Array<!CookieDataItem>}
     * @private
     */
    entries_: Array,

    /** Set the page title on the settings-subpage parent. */
    pageTitle: {
      type: String,
      notify: true,
    },

    /**
     * The site to show details for.
     * @type {!settings.CookieTreeNode}
     * @private
     */
    site_: Object,
  },

  listeners: {'cookie-tree-changed': 'onCookiesLoaded_'},

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    if (settings.getCurrentRoute() != settings.Route.SITE_SETTINGS_DATA_DETAILS)
      return;
    this.siteTitle_ = settings.getQueryParameters().get('site');
    if (!this.siteTitle_)
      return;
    this.loadCookies().then(this.onCookiesLoaded_.bind(this));
    this.pageTitle = this.siteTitle_;
  },

  /**
   * @return {!Array<!CookieDataForDisplay>}
   * @private
   */
  getCookieNodes_: function(cookie) {
    var node = this.rootCookieNode.fetchNodeById(cookie.id, true);
    if (!node)
      return [];
    return getCookieData(node.data);
  },

  /**
   * settings.RouteObserverBehavior
   * @private
   */
  onCookiesLoaded_: function() {
    var node = this.rootCookieNode.fetchNodeBySite(this.siteTitle_);
    if (node) {
      this.site_ = node;
      this.entries_ = this.site_.getCookieList();
    } else {
      this.entries_ = [];
    }
  },

  /**
   * Recursively look up a node path for a leaf node with a given id.
   * @param {!settings.CookieTreeNode} node The node to start with.
   * @param {string} currentPath The path constructed so far.
   * @param {string} targetId The id of the target leaf node to look for.
   * @return {string} The path of the node returned (or blank if not found).
   * @private
   */
  nodePath_: function(node, currentPath, targetId) {
    if (node.data.id == targetId)
      return currentPath;

    for (var i = 0; i < node.children_.length; ++i) {
      var child = node.children_[i];
      var path = this.nodePath_(
          child, currentPath + ',' + child.data.id, targetId);
      if (path.length > 0)
        return path;
    }
    return '';
  },

  /**
   * A handler for when the user opts to remove a single cookie.
   * @param {!Event} item
   * @return {string}
   * @private
   */
  getEntryDescription_: function(item) {
    // Frequently there are multiple cookies per site. To avoid showing a list
    // of '1 cookie', '1 cookie', ... etc, it is better to show the title of the
    // cookie to differentiate them.
    if (item.data.type == 'cookie')
      return item.title;
    return getCookieDataCategoryText(item.data.type, item.data.totalUsage);
  },

  /**
   * A handler for when the user opts to remove a single cookie.
   * @param {!Event} event
   * @private
   */
  onRemove_: function(event) {
    this.browserProxy.removeCookie(this.nodePath_(
        this.site_, this.site_.data.id, event.currentTarget.dataset.id));
  },

  /**
   * A handler for when the user opts to remove all cookies.
   */
  removeAll: function() {
    this.browserProxy.removeCookie(this.site_.data.id);
  },
});

})();
