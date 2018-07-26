// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'all-sites' is the polymer element for showing the list of all sites under
 * Site Settings.
 */
Polymer({
  is: 'all-sites',

  behaviors: [
    SiteSettingsBehavior,
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
    settings.GlobalScrollTargetBehavior,
  ],

  properties: {
    /**
     * Array of sites to display in the widget, grouped into their eTLD+1s.
     * @type {!Array<!SiteGroup>}
     */
    siteGroupList: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Needed by GlobalScrollTargetBehavior.
     * @override
     */
    subpageRoute: {
      type: Object,
      value: settings.routes.SITE_SETTINGS_ALL,
      readOnly: true,
    },

    /**
     * The search query entered into the All Sites search textbox. Used to
     * filter the All Sites list.
     * @private
     */
    searchQuery_: {
      type: String,
      value: '',
    },

    /**
     * All possible sort methods.
     * @type {Object}
     * @private
     */
    sortMethods_: {
      type: Object,
      value: function() {
        return {
          name: 'name',
          mostVisited: 'most-visited',
          storage: 'data-stored',
        };
      },
      readOnly: true,
    },

    /**
     * Stores the last selected item in the All Sites list.
     * @type {?{item: !SiteGroup, index: number}}
     * @private
     */
    selectedItem_: Object,

    /**
     * Used to determine focus between settings pages.
     * @type {!Map<string, (string|Function)>}
     */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
    },
  },

  /** @override */
  ready: function() {
    this.browserProxy_ =
        settings.SiteSettingsPrefsBrowserProxyImpl.getInstance();
    this.addWebUIListener(
        'contentSettingSitePermissionChanged', this.populateList_.bind(this));
    this.addEventListener(
        'site-entry-resized', this.resizeListIfScrollTargetActive_.bind(this));
    this.addEventListener(
        'site-entry-selected',
        (/** @type {!{detail: !{item: !SiteGroup, index: number}}} */ e) => {
          this.selectedItem_ = e.detail;
        });

    this.populateList_();
  },

  /** @override */
  attached: function() {
    // Set scrollOffset so the iron-list scrolling accounts for the space the
    // title takes.
    Polymer.RenderStatus.afterNextRender(this, () => {
      this.$.allSitesList.scrollOffset = this.$.allSitesList.offsetTop;
    });
  },

  /**
   * Retrieves a list of all known sites with site details.
   * @private
   */
  populateList_: function() {
    /** @type {!Array<settings.ContentSettingsTypes>} */
    const contentTypes = this.getCategoryList();
    // Make sure to include cookies, because All Sites handles data storage +
    // cookies as well as regular settings.ContentSettingsTypes.
    if (!contentTypes.includes(settings.ContentSettingsTypes.COOKIES))
      contentTypes.push(settings.ContentSettingsTypes.COOKIES);

    this.browserProxy_.getAllSites(contentTypes).then((response) => {
      this.siteGroupList = this.sortSiteGroupList_(response);
    });
  },

  /**
   * Filters |this.siteGroupList| with the given search query text.
   * @param {!Array<!SiteGroup>} siteGroupList The list of sites to filter.
   * @param {string} searchQuery The filter text.
   * @return {!Array<!SiteGroup>}
   * @private
   */
  filterPopulatedList_: function(siteGroupList, searchQuery) {
    if (searchQuery.length == 0)
      return siteGroupList;

    return siteGroupList.filter((siteGroup) => {
      return siteGroup.origins.find(origin => {
        return origin.includes(searchQuery);
      });
    });
  },

  /**
   * Sorts the given SiteGroup list with the currently selected sort method.
   * @param {!Array<!SiteGroup>} siteGroupList The list of sites to sort.
   * @return {!Array<!SiteGroup>}
   * @private
   */
  sortSiteGroupList_: function(siteGroupList) {
    const sortMethod = this.$.sortMethod.value;
    if (sortMethod == this.sortMethods_.name)
      siteGroupList.sort(this.nameComparator_);
    return siteGroupList;
  },

  nameComparator_: function(siteGroup1, siteGroup2) {
    return siteGroup1.etldPlus1.localeCompare(siteGroup2.etldPlus1);
  },

  /**
   * Called when the input text in the search textbox is updated.
   * @private
   */
  onSearchChanged_: function() {
    const searchElement = /** @type {SettingsSubpageSearchElement} */ (
        this.$$('settings-subpage-search'));
    this.searchQuery_ = searchElement.getSearchInput().value.toLowerCase();
  },

  /**
   * Called when the user chooses a different sort method to the default.
   * @private
   */
  onSortMethodChanged_: function() {
    this.siteGroupList = this.sortSiteGroupList_(this.siteGroupList);
    // Force the iron-list to rerender its items, as the order has changed.
    this.$.allSitesList.fire('iron-resize');
  },

  /**
   * Called when a list item changes its size, and thus the positions and sizes
   * of the items in the entire list also need updating.
   * @private
   */
  resizeListIfScrollTargetActive_: function() {
    if (settings.getCurrentRoute() == this.subpageRoute)
      this.$.allSitesList.fire('iron-resize');
  },

  /**
   * @param {!Map<string, (string|Function)>} newConfig
   * @param {?Map<string, (string|Function)>} oldConfig
   * @private
   */
  focusConfigChanged_: function(newConfig, oldConfig) {
    // focusConfig is set only once on the parent, so this observer should only
    // fire once.
    assert(!oldConfig);

    if (!settings.routes.SITE_SETTINGS_ALL)
      return;

    const onNavigatedTo = () => {
      this.async(() => {
        if (this.selectedItem_ == null || this.siteGroupList.length == 0)
          return;

        // Focus the site-entry to ensure the iron-list renders it, otherwise
        // the query selector will not be able to find it. Note the index is
        // used here instead of the item, in case the item was already removed.
        const index = Math.max(
            0, Math.min(this.selectedItem_.index, this.siteGroupList.length));
        this.$.allSitesList.focusItem(index);
        this.selectedItem_ = null;
      });
    };

    this.focusConfig.set(
        settings.routes.SITE_SETTINGS_SITE_DETAILS.path, onNavigatedTo);
  },
});
