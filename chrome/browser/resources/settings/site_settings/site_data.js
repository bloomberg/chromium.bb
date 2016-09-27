// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-data' handles showing the local storage summary list for all sites.
 */

Polymer({
  is: 'site-data',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * A summary list of all sites and how many entities each contain.
     * @type {Array<CookieDataSummaryItem>}
     */
    sites: Array,

    /**
     * The cookie tree with the details needed to display individual sites and
     * their contained data.
     * @type {!settings.CookieTreeNode}
     */
    treeNodes_: Object,

    /**
     * Keeps track of how many outstanding requests for more data there are.
     */
    requests_: Number,

    /**
     * The current filter applied to the cookie data list.
     */
    filterString_: {
      type: String,
      value: '',
    }
  },

  ready: function() {
    this.addWebUIListener('onTreeItemRemoved',
        this.onTreeItemRemoved_.bind(this));
    this.treeNodes_ = new settings.CookieTreeNode(null);
    // Start the initial request.
    this.reloadCookies_();
  },

  /**
   * Reloads the whole cookie list.
   * @private
   */
  reloadCookies_: function() {
    this.browserProxy.reloadCookies().then(function(list) {
      this.loadChildren_(list);
    }.bind(this));
  },

  /**
   * A filter function for the list.
   * @param {!CookieDataSummaryItem} item The item to possibly filter out.
   * @return {!boolean} Whether to show the item.
   * @private
   */
  showItem_: function(item) {
    if (this.filterString_.length == 0)
      return true;
    return item.site.indexOf(this.filterString_) > -1;
  },

  /** @private */
  onSearchChanged_: function(e) {
    this.filterString_ = e.detail;
    this.$.list.render();
  },

  /** @private */
  isRemoveButtonVisible_: function(sites, renderedItemCount) {
    return renderedItemCount != 0;
  },

  /**
   * Returns the string to use for the Remove label.
   * @return {!string} filterString The current filter string.
   * @private
   */
  computeRemoveLabel_: function(filterString) {
    if (filterString.length == 0)
      return loadTimeData.getString('siteSettingsCookieRemoveAll');
    return loadTimeData.getString('siteSettingsCookieRemoveAllShown');
  },

  /**
   * Called when the cookie list is ready to be shown.
   * @param {!CookieList} list The cookie list to show.
   * @private
   */
  loadChildren_: function(list) {
    var parentId = list.id;
    var data = list.children;

    if (parentId == null) {
      // New root being added, clear the list and add the nodes.
      this.sites = [];
      this.requests_ = 0;
      this.treeNodes_.addChildNodes(this.treeNodes_, data);
    } else {
      this.treeNodes_.populateChildNodes(parentId, this.treeNodes_, data);
    }

    for (var i = 0; i < data.length; ++i) {
      var prefix = parentId == null ? '' : parentId + ', ';
      if (data[i].hasChildren) {
        ++this.requests_;
        this.browserProxy.loadCookieChildren(
            prefix + data[i].id).then(function(list) {
          --this.requests_;
          this.loadChildren_(list);
        }.bind(this));
      }
    }

    if (this.requests_ == 0)
      this.sites = this.treeNodes_.getSummaryList();

    // If this reaches below zero then we're forgetting to increase the
    // outstanding request count and the summary list won't be updated at the
    // end.
    assert(this.requests_ >= 0);
  },

  /**
   * Called when a single item has been removed (not during delete all).
   * @param {!CookieRemovePacket} args The details about what to remove.
   */
  onTreeItemRemoved_: function(args) {
    this.treeNodes_.removeByParentId(args.id, args.start, args.count);
    this.sites = this.treeNodes_.getSummaryList();
  },

  /**
   * Deletes all site data for a given site.
   * @param {!{model: !{item: CookieDataSummaryItem}}} event
   * @private
   */
  onDeleteSite_: function(event) {
    this.browserProxy.removeCookie(event.model.item.id);
  },

  /**
   * Deletes site data for multiple sites.
   * @private
   */
  onDeleteMultipleSites_: function() {
    if (this.filterString_.length == 0) {
      this.browserProxy.removeAllCookies().then(function(list) {
        this.loadChildren_(list);
      }.bind(this));
    } else {
      var items = this.$.list.items;
      for (var i = 0; i < items.length; ++i) {
        if (this.showItem_(items[i]))
          this.browserProxy.removeCookie(items[i].id);
      }
    }
  },

  /**
   * @param {!{model: !{item: CookieDataSummaryItem}}} event
   * @private
   */
  onSiteTap_: function(event) {
    var dialog = document.createElement('site-data-details-dialog');
    dialog.category = this.category;
    this.shadowRoot.appendChild(dialog);

    var node = this.treeNodes_.fetchNodeById(event.model.item.id, false);
    dialog.open(node);

    dialog.addEventListener('close', function(event) {
      dialog.remove();
    });
  },
});
