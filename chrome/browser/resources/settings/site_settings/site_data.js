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
  },

  ready: function() {
    this.addWebUIListener('loadChildren', this.loadChildren_.bind(this));
    this.addWebUIListener('onTreeItemRemoved',
        this.onTreeItemRemoved_.bind(this));
    this.treeNodes_ = new settings.CookieTreeNode(null);
    // Start the initial request.
    this.browserProxy.reloadCookies();
    this.requests_ = 1;
  },

  loadChildren_: function(list) {
    var parentId = list[0];
    var data = list[1];

    if (parentId == null) {
      this.treeNodes_.addChildNodes(this.treeNodes_, data);
    } else {
      this.treeNodes_.populateChildNodes(parentId, this.treeNodes_, data);
    }

    for (var i = 0; i < data.length; ++i) {
      var prefix = parentId == null ? '' : parentId + ', ';
      if (data[i].hasChildren) {
        this.requests_ += 1;
        this.browserProxy.loadCookieChildren(prefix + data[i].id);
      }
    }

    if (--this.requests_ == 0)
      this.sites = this.treeNodes_.getSummaryList();
  },

  /**
   * Called when an item is removed.
   */
  onTreeItemRemoved_: function(args) {
    this.treeNodes_.removeByParentId(args[0], args[1], args[2]);
    this.sites = this.treeNodes_.getSummaryList();
  },

  /**
   * @param {!{model: !{item: !{title: string, id: string}}}} event
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
