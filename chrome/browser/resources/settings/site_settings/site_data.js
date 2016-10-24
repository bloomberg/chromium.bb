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
     * @private
     */
    treeNodes_: Object,

    /**
     * The current filter applied to the cookie data list.
     * @private
     */
    filterString_: {
      type: String,
      value: '',
    },

    /** @private */
    confirmationDeleteMsg_: String,

    /** @private */
    idToDelete_: String,
  },

  /** @override */
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
   * @return {boolean} Whether to show the item.
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
   * @return {string} filterString The current filter string.
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
    var loadChildrenRecurse = function(list) {
      var parentId = list.id;
      var children = list.children;
      var prefix = '';
      if (parentId !== null) {
        this.treeNodes_.populateChildNodes(parentId, this.treeNodes_, children);
        prefix = parentId + ', ';
      }
      var promises = [];
      for (let child of children) {
        if (child.hasChildren) {
          promises.push(this.browserProxy.loadCookieChildren(
              prefix + child.id).then(loadChildrenRecurse.bind(this)));
        }
      }
      return Promise.all(promises);
    }.bind(this);

    // New root being added, clear the list and add the nodes.
    this.sites = [];
    this.treeNodes_.addChildNodes(this.treeNodes_, list.children);
    loadChildrenRecurse(list).then(function() {
      this.sites = this.treeNodes_.getSummaryList();
    }.bind(this));
  },

  /**
   * Called when a single item has been removed (not during delete all).
   * @param {!CookieRemovePacket} args The details about what to remove.
   * @private
   */
  onTreeItemRemoved_: function(args) {
    this.treeNodes_.removeByParentId(args.id, args.start, args.count);
    this.sites = this.treeNodes_.getSummaryList();
  },

  /** @private */
  onCloseDialog_: function() {
    this.$.confirmDeleteDialog.close();
  },

  /**
   * Shows a dialog to confirm the deletion of multiple sites.
   * @private
   */
  onConfirmDeleteMultipleSites_: function() {
    this.idToDelete_ = '';  // Delete all.
    this.confirmationDeleteMsg_ = loadTimeData.getString(
        'siteSettingsCookieRemoveMultipleConfirmation');
    this.$.confirmDeleteDialog.showModal();
  },

  /**
   * Called when deletion for a single/multiple sites has been confirmed.
   * @private
   */
  onConfirmDelete_: function() {
    if (this.idToDelete_ != '')
      this.onDeleteSite_();
    else
      this.onDeleteMultipleSites_();
    this.$.confirmDeleteDialog.close();
  },

  /**
   * Deletes all site data for a given site.
   * @private
   */
  onDeleteSite_: function() {
    this.browserProxy.removeCookie(this.idToDelete_);
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
