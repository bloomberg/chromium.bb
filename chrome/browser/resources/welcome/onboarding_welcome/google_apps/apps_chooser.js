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
 *   bookmarkId: ?string,
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

  behaviors: [I18nBehavior],

  properties: {
    /**
     * @type {!Array<!nuxGoogleApps.AppItem>}
     * @private
     */
    appList_: Array,

    bookmarkBarWasShown: Boolean,

    hasAppsSelected: {
      type: Boolean,
      notify: true,
      value: true,
    },
  },

  /** @private {nux.NuxGoogleAppsProxy} */
  appsProxy_: null,

  /** @private {nux.BookmarkProxy} */
  bookmarkProxy_: null,

  /** @override */
  attached: function() {
    Polymer.RenderStatus.afterNextRender(this, () => {
      Polymer.IronA11yAnnouncer.requestAvailability();
    });
  },

  /** @override */
  ready() {
    this.appsProxy_ = nux.NuxGoogleAppsProxyImpl.getInstance();
    this.bookmarkProxy_ = nux.BookmarkProxyImpl.getInstance();
  },

  /** Called when bookmarks should be created for all selected apps. */
  populateAllBookmarks() {
    if (this.appList_) {
      this.appList_.forEach(app => this.updateBookmark(app));
    } else {
      this.appsProxy_.getGoogleAppsList().then(list => {
        this.appList_ = list;
        this.appList_.forEach((app, index) => {
          // Default select first few items.
          app.selected = index < 3;
          this.updateBookmark(app);
        });
        this.updateHasAppsSelected();
        this.fire('iron-announce', {text: this.i18n('bookmarksAdded')});
      });
    }
  },

  /** Called when bookmarks should be removed for all selected apps. */
  removeAllBookmarks() {
    if (!this.appList_)
      return;  // No apps to remove.

    let removedBookmarks = false;
    this.appList_.forEach(app => {
      if (app.selected && app.bookmarkId) {
        // Don't call |updateBookmark| b/c we want to save the selection.
        this.bookmarkProxy_.removeBookmark(app.bookmarkId);
        app.bookmarkId = null;
        removedBookmarks = true;
      }
    });
    // Only update and announce if we removed bookmarks.
    if (removedBookmarks) {
      this.bookmarkProxy_.toggleBookmarkBar(this.bookmarkBarWasShown);
      this.fire('iron-announce', {text: this.i18n('bookmarksRemoved')});
    }
  },

  /**
   * @param {!nuxGoogleApps.AppItem} item
   * @private
   */
  updateBookmark(item) {
    if (item.selected && !item.bookmarkId) {
      this.bookmarkProxy_.toggleBookmarkBar(true);
      this.bookmarkProxy_.addBookmark(
          {
            title: item.name,
            url: item.url,
            parentId: '1',
          },
          result => {
            item.bookmarkId = result.id;
          });
      // Cache bookmark icon.
      this.appsProxy_.cacheBookmarkIcon(item.id);
    } else if (!item.selected && item.bookmarkId) {
      this.bookmarkProxy_.removeBookmark(item.bookmarkId);
      item.bookmarkId = null;
    }
  },

  /**
   * Handle toggling the apps selected.
   * @param {!{model: !nuxGoogleApps.AppItemModel}} e
   * @private
   */
  onAppClick_: function(e) {
    let item = e.model.item;
    e.model.set('item.selected', !item.selected);
    this.updateBookmark(item);
    this.updateHasAppsSelected();
    // Announcements should NOT be in |updateBookmark| because there should be a
    // different utterance when all app bookmarks are added/removed.
    if (item.selected)
      this.fire('iron-announce', {text: this.i18n('bookmarkAdded')});
    else
      this.fire('iron-announce', {text: this.i18n('bookmarkRemoved')});
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
   * Updates the value of hasAppsSelected.
   * @private
   */
  updateHasAppsSelected: function() {
    this.hasAppsSelected = this.appList_ && this.appList_.some(a => a.selected);
    if (!this.hasAppsSelected)
      this.bookmarkProxy_.toggleBookmarkBar(this.bookmarkBarWasShown);
  },
});
