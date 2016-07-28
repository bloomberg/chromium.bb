// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-data-details-dialog' provides a dialog to show details of site data
 * stored by a given site.
 */
Polymer({
  is: 'site-data-details-dialog',

  behaviors: [SiteSettingsBehavior],

  properties: {
    /**
     * The title of the dialog.
     */
    title_: String,

    /**
     * The site to show details for.
     * @type {!settings.CookieTreeNode}
     * @private
     */
    site_: Object,

    /**
     * The cookie entries for the given site.
     * @type {!Array<!CookieDataItem>}
     * @private
     */
    entries_: Array,

    /**
     * The index of the last selected item.
     */
     lastSelectedIndex_: Number,

     /**
      * Our WebUI listener.
      * @type {?WebUIListener}
      */
     listener_: Object,
  },

  /**
   * Opens the dialog.
   * @param {!settings.CookieTreeNode} site The site to show data for.
   */
  open: function(site) {
    this.site_ = site;
    this.populateDialog_();
    this.listener_ = cr.addWebUIListener(
        'onTreeItemRemoved', this.onTreeItemRemoved_.bind(this));
    this.$.dialog.showModal();
  },

  /**
   * Populates the dialog with the data about the site.
   * @private
   */
  populateDialog_: function() {
    this.title_ = loadTimeData.getStringF('siteSettingsCookieDialog',
                                          this.site_.data_.title);

    this.entries_ = this.site_.getCookieList();
    if (this.entries_.length < 2) {
      // When there's only one item to show, hide the picker and change the
      // 'Remove All' button to read 'Remove' instead.
      this.$.container.hidden = true;
      this.$.clear.textContent =
          loadTimeData.getString('siteSettingsCookieRemove');
    } else {
      this.$.picker.selected = this.entries_[0].id;
      this.lastSelectedIndex_ = 0;
    }

    this.populateItem_(this.entries_[0].id, this.site_);
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
    if (node.data_.id == targetId)
      return currentPath;

    for (var i = 0; i < node.children_.length; ++i) {
      var child = node.children_[i];
      var path = this.nodePath_(
          child, currentPath + ',' + child.data_.id, targetId);
      if (path.length > 0)
        return path;
    }

    return '';
  },

  /**
   * Add the cookie data to the content section of this dialog.
   * @param {string} id The id of the cookie node to display.
   * @param {!settings.CookieTreeNode} site The current site.
   * @private
   */
  populateItem_: function(id, site) {
    // Out with the old...
    var root = this.$.content;
    while (root.lastChild) {
      root.removeChild(root.lastChild);
    }

    // In with the new...
    var node = site.fetchNodeById(id, true);
    if (node)
      site.addCookieData(root, node);
  },

  /** @private */
  onTreeItemRemoved_: function(args) {
    this.entries_ = this.site_.getCookieList();
    if (args[0] == this.site_.data_.id || this.entries_.length == 0) {
      this.$.dialog.close();
      return;
    }

    if (this.entries_.length <= this.lastSelectedIndex_)
      this.lastSelectedIndex_ = this.entries_.length - 1;
    var selectedId = this.entries_[this.lastSelectedIndex_].id;
    this.$.picker.selected = selectedId;
    this.populateItem_(selectedId, this.site_);
  },

  /**
   * A handler for when the user changes the dropdown box (switches cookies).
   * @private
   */
  onItemSelected_: function(event) {
    this.populateItem_(event.detail.selected, this.site_);

    // Store the index of what was selected so we can re-select the next value
    // when things get deleted.
    for (var i = 0; i < this.entries_.length; ++i) {
      if (this.entries_[i].data.id == event.detail.selected) {
        this.lastSelectedIndex_ = i;
        break;
      }
    }
  },

  /**
   * A handler for when the user opts to remove a single cookie.
   * @private
   */
  onRemove_: function(event) {
    this.browserProxy.removeCookie(this.nodePath_(
        this.site_, this.site_.data_.id, this.$.picker.selected));
  },

  /**
   * A handler for when the user opts to remove all cookies.
   * @private
   */
  onRemoveAll_: function(event) {
    cr.removeWebUIListener(this.listener_);
    this.browserProxy.removeCookie(this.site_.data_.id);
    this.$.dialog.close();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.cancel();
  },
});
