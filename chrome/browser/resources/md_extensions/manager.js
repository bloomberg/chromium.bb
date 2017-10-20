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
      /** @type {extensions.Toolbar} */
      toolbar: Object,

      // This is not typed because it implements multiple interfaces, and is
      // passed to different elements as different types.
      delegate: Object,

      isGuest_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('isGuest');
        },
      },

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

      /** @private */
      showDrawer_: Boolean,

      /** @private */
      showLoadErrorDialog_: Boolean,

      /** @private */
      showOptionsDialog_: Boolean,

      /** @private */
      showPackDialog_: Boolean,

      // <if expr="chromeos">
      /** @private */
      kioskEnabled_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      showKioskDialog_: {
        type: Boolean,
        value: false,
      },
      // </if>
    },

    /**
     * The current page being shown. Default to null, and initPage will figure
     * out the initial page based on url.
     * @private {?PageState}
     */
    currentPage_: null,

    /**
     * The ID of the listener on |extensions.navigation|. Stored so that the
     * listener can be removed when this element is detached (happens in tests).
     * @private {?number}
     */
    navigationListener_: null,

    /** @override */
    created: function() {
      this.readyPromiseResolver = new PromiseResolver();
    },

    /** @override */
    ready: function() {
      this.toolbar =
          /** @type {extensions.Toolbar} */ (this.$$('extensions-toolbar'));
      this.readyPromiseResolver.resolve();

      // <if expr="chromeos">
      extensions.KioskBrowserProxyImpl.getInstance()
          .initializeKioskAppSettings()
          .then(params => {
            this.kioskEnabled_ = params.kioskEnabled;
          });
      // </if>
    },

    /** @override */
    attached: function() {
      document.documentElement.classList.remove('loading');

      this.navigationListener_ = extensions.navigation.addListener(newPage => {
        this.changePage_(newPage);
      });
    },

    /** @override */
    detached: function() {
      assert(extensions.navigation.removeListener(this.navigationListener_));
      this.navigationListener_ = null;
    },

    get keyboardShortcuts() {
      return this.$['keyboard-shortcuts'];
    },

    get errorPage() {
      return this.$['error-page'];
    },

    /**
     * Initializes the page to reflect what's specified in the url so that if
     * the user visits chrome://extensions/?id=..., we land on the proper page.
     */
    initPage: function() {
      this.didInitPage_ = true;
      this.changePage_(extensions.navigation.getCurrentPage());
    },

    /**
     * @param {!CustomEvent} event
     * @private
     */
    onFilterChanged_: function(event) {
      if (this.currentPage_.page !== Page.LIST)
        extensions.navigation.navigateTo({page: Page.LIST});
      this.filter = /** @type {string} */ (event.detail);
    },

    /** @private */
    onMenuButtonTap_: function() {
      this.showDrawer_ = true;
      this.async(() => {
        this.$$('#drawer').openDrawer();
      });
    },

    /**
     * @param {chrome.developerPrivate.ExtensionInfo} item
     * @return {string} The ID of the list that the item belongs in.
     * @private
     */
    getListId_: function(item) {
      const ExtensionType = chrome.developerPrivate.ExtensionType;
      switch (item.type) {
        case ExtensionType.HOSTED_APP:
        case ExtensionType.LEGACY_PACKAGED_APP:
        case ExtensionType.PLATFORM_APP:
          return 'apps';
        case ExtensionType.EXTENSION:
        case ExtensionType.SHARED_MODULE:
          return 'extensions';
        case ExtensionType.THEME:
          assertNotReached(
              'Don\'t send themes to the chrome://extensions page');
          break;
      }
      assertNotReached();
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
      const listId = this.getListId_(item);
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
      const listId = this.getListId_(item);
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
      const listId = this.getListId_(item);
      const index = this.getIndexInList_(listId, item.id);
      // We should never try and remove a non-existent item.
      assert(index >= 0);
      this.splice(listId, index, 1);
    },

    /**
     * @param {!chrome.developerPrivate.LoadError} loadError
     */
    showLoadError: function(loadError) {
      this.showLoadErrorDialog_ = true;
      this.async(() => {
        const dialog = this.$$('#load-error');
        dialog.loadError = loadError;
        dialog.show();
      });
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
     * @private
     */
    changePage_: function(newPage) {
      const drawer = this.$$('#drawer');
      if (drawer && drawer.open) {
        drawer.closeDrawer();
        this.showDrawer_ = false;
      }

      const optionsDialog = this.$$('#options-dialog');
      if (optionsDialog && optionsDialog.open) {
        optionsDialog.close();
        this.showOptionsDialog_ = false;
      }

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
      } else {
        /** @type {extensions.ViewManager} */ (this.$.viewManager)
            .animateCurrentView('fade-in');
      }

      if (newPage.subpage) {
        assert(newPage.subpage == Dialog.OPTIONS);
        assert(newPage.extensionId);
        this.showOptionsDialog_ = true;
        this.async(() => {
          this.$$('#options-dialog').show(data);
        });
      }

      this.currentPage_ = newPage;
    },

    /** @private */
    onDrawerClose_: function() {
      this.showDrawer_ = false;
    },

    /** @private */
    onLoadErrorDialogClose_: function() {
      this.showLoadErrorDialog_ = false;
    },

    /** @private */
    onOptionsDialogClose_: function() {
      this.showOptionsDialog_ = false;
    },

    /** @private */
    onPackTap_: function() {
      this.showPackDialog_ = true;
      this.async(() => {
        this.$$('#pack-dialog').show();
      });
    },

    /** @private */
    onPackDialogClose_: function() {
      this.showPackDialog_ = false;
    },

    // <if expr="chromeos">
    /** @private */
    onKioskTap_: function() {
      this.showKioskDialog_ = true;
    },

    onKioskDialogClose_: function() {
      this.showKioskDialog_ = false;
    },
    // </if>
  });

  return {Manager: Manager};
});
