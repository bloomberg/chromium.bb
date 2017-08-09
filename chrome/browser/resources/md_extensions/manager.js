// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * Compares two extensions to determine which should come first in the list.
   * @param {chrome.developerPrivate.ExtensionInfo} a
   * @param {chrome.developerPrivate.ExtensionInfo} b
   * @return {number}
   */
  const compareExtensions = function(a, b) {
    function compare(x, y) {
      return x < y ? -1 : (x > y ? 1 : 0);
    }
    function compareLocation(x, y) {
      if (x.location == y.location)
        return 0;
      if (x.location == chrome.developerPrivate.Location.UNPACKED)
        return -1;
      if (y.location == chrome.developerPrivate.Location.UNPACKED)
        return 1;
      return 0;
    }
    return compareLocation(a, b) ||
        compare(a.name.toLowerCase(), b.name.toLowerCase()) ||
        compare(a.id, b.id);
  };

  const Manager = Polymer({
    is: 'extensions-manager',

    behaviors: [I18nBehavior],

    properties: {
      /** @type {extensions.Sidebar} */
      sidebar: Object,

      /** @type {extensions.Toolbar} */
      toolbar: Object,

      // This is not typed because it implements multiple interfaces, and is
      // passed to different elements as different types.
      delegate: Object,

      inDevMode: {
        type: Boolean,
        value: false,
      },

      filter: {
        type: String,
        value: '',
      },

      /**
       * The item currently displayed in the error subpage. We use a separate
       * item for different pages (rather than a single subpageItem_ property)
       * so that hidden subpages don't update when an item updates. That is, we
       * don't want the details view subpage to update when the item shown in
       * the errors page updates, and vice versa.
       * @private {!chrome.developerPrivate.ExtensionInfo|undefined}
       */
      errorPageItem_: Object,

      /**
       * The item currently displayed in the details view subpage. See also
       * errorPageItem_.
       * @private {!chrome.developerPrivate.ExtensionInfo|undefined}
       */
      detailViewItem_: Object,

      /**
       * The helper object to maintain page state.
       * @private {!extensions.NavigationHelper}
       */
      navigationHelper_: Object,

      /** @type {!Array<!chrome.developerPrivate.ExtensionInfo>} */
      extensions: {
        type: Array,
        value: function() {
          return [];
        },
      },

      /** @type {!Array<!chrome.developerPrivate.ExtensionInfo>} */
      apps: {
        type: Array,
        value: function() {
          return [];
        },
      },

      /**
       * Prevents page content from showing before data is first loaded.
       * @private
       */
      didInitPage_: {
        type: Boolean,
        value: false,
      },
    },

    listeners: {
      'items-list.extension-item-show-details': 'onShouldShowItemDetails_',
      'items-list.extension-item-show-errors': 'onShouldShowItemErrors_',
    },

    /**
     * The current page being shown. Default to null, and initPage will figure
     * out the initial page based on url.
     * @private {?PageState}
     */
    currentPage_: null,

    created: function() {
      this.readyPromiseResolver = new PromiseResolver();
    },

    ready: function() {
      /** @type {extensions.Sidebar} */
      this.sidebar =
          /** @type {extensions.Sidebar} */ (this.$$('extensions-sidebar'));
      this.toolbar =
          /** @type {extensions.Toolbar} */ (this.$$('extensions-toolbar'));
      this.listHelper_ = new ListHelper(this);
      this.sidebar.setListDelegate(this.listHelper_);
      this.readyPromiseResolver.resolve();
      this.navigationHelper_ = new extensions.NavigationHelper(newPage => {
        this.changePage(newPage, true);
      });
      this.optionsDialog.addEventListener('close', () => {
        // We update the page when the options dialog closes, but only if we're
        // still on the details page. We could be on a different page if the
        // user hit back while the options dialog was visible; in that case, the
        // new page is already correct.
        if (this.currentPage_ && this.currentPage_.page == Page.DETAILS) {
          // This will update the currentPage_ and the NavigationHelper; since
          // the active page is already the details page, no main page
          // transition occurs.
          this.changePage(
              {page: Page.DETAILS, extensionId: this.currentPage_.extensionId});
        }
      });
    },

    get keyboardShortcuts() {
      return this.$['keyboard-shortcuts'];
    },

    get packDialog() {
      return this.$['pack-dialog'];
    },

    get loadError() {
      return this.$['load-error'];
    },

    get optionsDialog() {
      return this.$['options-dialog'];
    },

    get errorPage() {
      return this.$['error-page'];
    },

    /**
     * Shows the details view for a given item.
     * @param {!chrome.developerPrivate.ExtensionInfo} data
     */
    showItemDetails: function(data) {
      this.changePage({page: Page.DETAILS, extensionId: data.id});
    },

    /**
     * Initializes the page to reflect what's specified in the url so that if
     * the user visits chrome://extensions/?id=..., we land on the proper page.
     */
    initPage: function() {
      this.didInitPage_ = true;
      this.changePage(this.navigationHelper_.getCurrentPage(), true);
    },

    /**
     * @param {!CustomEvent} event
     * @private
     */
    onFilterChanged_: function(event) {
      this.filter = /** @type {string} */ (event.detail);
    },

    /** @private */
    onMenuButtonTap_: function() {
      this.$.drawer.toggle();
    },

    /**
     * @param {chrome.developerPrivate.ExtensionType} type The type of item.
     * @return {string} The ID of the list that the item belongs in.
     * @private
     */
    getListId_: function(type) {
      let listId;
      const ExtensionType = chrome.developerPrivate.ExtensionType;
      switch (type) {
        case ExtensionType.HOSTED_APP:
        case ExtensionType.LEGACY_PACKAGED_APP:
        case ExtensionType.PLATFORM_APP:
          listId = 'apps';
          break;
        case ExtensionType.EXTENSION:
        case ExtensionType.SHARED_MODULE:
          listId = 'extensions';
          break;
        case ExtensionType.THEME:
          assertNotReached(
              'Don\'t send themes to the chrome://extensions page');
          break;
      }
      assert(listId);
      return listId;
    },

    /**
     * @param {string} listId The list to look for the item in.
     * @param {string} itemId The id of the item to look for.
     * @return {number} The index of the item in the list, or -1 if not found.
     * @private
     */
    getIndexInList_: function(listId, itemId) {
      return this[listId].findIndex(function(item) {
        return item.id == itemId;
      });
    },

    /**
     * @return {?chrome.developerPrivate.ExtensionInfo}
     * @private
     */
    getData_: function(id) {
      return this.extensions[this.getIndexInList_('extensions', id)] ||
          this.apps[this.getIndexInList_('apps', id)];
    },

    /**
     * Creates and adds a new extensions-item element to the list, inserting it
     * into its sorted position in the relevant section.
     * @param {!chrome.developerPrivate.ExtensionInfo} item The extension
     *     the new element is representing.
     */
    addItem: function(item) {
      const listId = this.getListId_(item.type);
      // We should never try and add an existing item.
      assert(this.getIndexInList_(listId, item.id) == -1);
      let insertBeforeChild = this[listId].findIndex(function(listEl) {
        return compareExtensions(listEl, item) > 0;
      });
      if (insertBeforeChild == -1)
        insertBeforeChild = this[listId].length;
      this.splice(listId, insertBeforeChild, 0, item);
    },

    /**
     * @param {!chrome.developerPrivate.ExtensionInfo} item The data for the
     *     item to update.
     */
    updateItem: function(item) {
      const listId = this.getListId_(item.type);
      const index = this.getIndexInList_(listId, item.id);
      // We should never try and update a non-existent item.
      assert(index >= 0);
      this.set([listId, index], item);

      // Update the subpage if it is open and displaying the item. If it's not
      // open, we don't update the data even if it's displaying that item. We'll
      // set the item correctly before opening the page. It's a little weird
      // that the DOM will have stale data, but there's no point in causing the
      // extra work.
      if (this.detailViewItem_ && this.detailViewItem_.id == item.id &&
          this.currentPage_.page == Page.DETAILS) {
        this.detailViewItem_ = item;
      } else if (
          this.errorPageItem_ && this.errorPageItem_.id == item.id &&
          this.currentPage_.page == Page.ERRORS) {
        this.errorPageItem_ = item;
      }
    },

    /**
     * @param {!chrome.developerPrivate.ExtensionInfo} item The data for the
     *     item to remove.
     */
    removeItem: function(item) {
      const listId = this.getListId_(item.type);
      const index = this.getIndexInList_(listId, item.id);
      // We should never try and remove a non-existent item.
      assert(index >= 0);
      this.splice(listId, index, 1);
    },

    /**
     * @param {Page} page
     * @return {!(extensions.KeyboardShortcuts |
     *            extensions.DetailView |
     *            extensions.ItemList)}
     * @private
     */
    getPage_: function(page) {
      switch (page) {
        case Page.LIST:
          return this.$['items-list'];
        case Page.DETAILS:
          return this.$['details-view'];
        case Page.SHORTCUTS:
          return this.$['keyboard-shortcuts'];
        case Page.ERRORS:
          return this.$['error-page'];
      }
      assertNotReached();
    },

    /**
     * Changes the active page selection.
     * @param {PageState} newPage
     * @param {boolean=} isSilent If true, does not notify the navigation helper
     *     of the change.
     */
    changePage: function(newPage, isSilent) {
      if (this.currentPage_ && this.currentPage_.page == newPage.page &&
          this.currentPage_.subpage == newPage.subpage &&
          this.currentPage_.extensionId == newPage.extensionId) {
        return;
      }

      this.$.drawer.closeDrawer();
      if (this.optionsDialog.open)
        this.optionsDialog.close();

      const fromPage = this.currentPage_ ? this.currentPage_.page : null;
      const toPage = newPage.page;
      let data;
      if (newPage.extensionId)
        data = assert(this.getData_(newPage.extensionId));

      if (toPage == Page.DETAILS)
        this.detailViewItem_ = assert(data);
      else if (toPage == Page.ERRORS)
        this.errorPageItem_ = assert(data);

      if (fromPage != toPage) {
        /** @type {extensions.ViewManager} */ (this.$.viewManager)
            .switchView(toPage);
      }

      if (newPage.subpage) {
        assert(newPage.subpage == Dialog.OPTIONS);
        assert(newPage.extensionId);
        this.optionsDialog.show(data);
      }

      this.currentPage_ = newPage;

      if (!isSilent)
        this.navigationHelper_.updateHistory(newPage);
    },

    /**
     * Handles the event for the user clicking on a details button.
     * @param {!CustomEvent} e
     * @private
     */
    onShouldShowItemDetails_: function(e) {
      this.showItemDetails(e.detail.data);
    },

    /**
     * Handles the event for the user clicking on the errors button.
     * @param {!CustomEvent} e
     * @private
     */
    onShouldShowItemErrors_: function(e) {
      this.changePage({page: Page.ERRORS, id: e.detail.data.id});
    },

    /** @private */
    onDetailsViewClose_: function() {
      // Note: we don't reset detailViewItem_ here because doing so just causes
      // extra work for the data-bound details view.
      this.changePage({page: Page.LIST});
    },

    /** @private */
    onErrorPageClose_: function() {
      // Note: we don't reset errorPageItem_ here because doing so just causes
      // extra work for the data-bound error page.
      this.changePage({page: Page.LIST});
    },

    /** @private */
    onPackTap_: function() {
      this.$['pack-dialog'].show();
    }
  });

  /** @implements {extensions.SidebarListDelegate} */
  class ListHelper {
    /** @param {extensions.Manager} manager */
    constructor(manager) {
      this.manager_ = manager;
    }

    /** @override */
    showType(type) {
      let items;
      switch (type) {
        case extensions.ShowingType.EXTENSIONS:
          items = this.manager_.extensions;
          break;
        case extensions.ShowingType.APPS:
          items = this.manager_.apps;
          break;
      }

      this.manager_.$ /* hack */['items-list'].set('items', assert(items));
      this.manager_.changePage({page: Page.LIST});
    }

    /** @override */
    showKeyboardShortcuts() {
      this.manager_.changePage({page: Page.SHORTCUTS});
    }
  }

  return {Manager: Manager};
});
