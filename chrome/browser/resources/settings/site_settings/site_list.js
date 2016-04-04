// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-site-list' shows a list of Allowed and Blocked sites for a given
 * category.
 */
Polymer({

  is: 'settings-site-list',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * The site that was selected by the user in the dropdown list.
     * @type {SiteException}
     */
    selectedSite: {
      type: Object,
      notify: true,
    },

    /**
     * Array of sites to display in the widget.
     * @type {!Array<SiteException>}
     */
    sites: {
      type: Array,
      value: function() { return []; },
    },

    /**
     * Whether this list is for the All Sites category.
     */
    allSites: {
      type: Boolean,
      value: false,
    },

    /**
      * The type of category this widget is displaying data for. Normally
      * either ALLOW or BLOCK, representing which sites are allowed or blocked
      * respectively.
      */
    categorySubtype: {
      type: Number,
      value: settings.INVALID_CATEGORY_SUBTYPE,
    },

    /**
     * Represents the state of the main toggle shown for the category. For
     * example, the Location category can be set to Block/Ask so false, in that
     * case, represents Block and true represents Ask.
     */
    categoryEnabled: {
      type: Boolean,
      value: true,
    },

    /**
     * Whether to show the Allow action in the action menu.
     */
    showAllowAction_: Boolean,

    /**
     * Whether to show the Block action in the action menu.
     */
    showBlockAction_: Boolean,

    /**
     * All possible actions in the action menu.
     */
    actions_: {
      readOnly: true,
      type: Object,
      values: {
        ALLOW: 'Allow',
        BLOCK: 'Block',
        RESET: 'Reset',
      }
    },

    i18n_: {
      readOnly: true,
      type: Object,
      value: function() {
        return {
          allowAction: loadTimeData.getString('siteSettingsActionAllow'),
          blockAction: loadTimeData.getString('siteSettingsActionBlock'),
          resetAction: loadTimeData.getString('siteSettingsActionReset'),
        };
      },
    },
  },

  observers: [
    'configureWidget_(category, categorySubtype, categoryEnabled, allSites)'
  ],

  ready: function() {
    this.addWebUIListener('contentSettingSitePermissionChanged',
        this.siteWithinCategoryChanged_.bind(this));
  },

  /**
   * Called when a site changes permission.
   * @param {number} category The category of the site that changed.
   * @param {string} site The site that changed.
   * @private
   */
  siteWithinCategoryChanged_: function(category, site) {
    if (category == this.category)
      this.configureWidget_();
  },

  /**
   * Configures the action menu, visibility of the widget and shows the list.
   * @private
   */
  configureWidget_: function() {
    // The observer for All Sites fires before the attached/ready event, so
    // initialize this here.
    if (this.browserProxy_ === undefined) {
      this.browserProxy_ =
          settings.SiteSettingsPrefsBrowserProxyImpl.getInstance();
    }

    this.setUpActionMenu_();
    this.ensureOpened_();
    this.populateList_();
  },

  /**
   * Ensures the widget is |opened| when needed when displayed initially.
   * @private
   */
  ensureOpened_: function() {
    // Allowed list is always shown opened by default and All Sites is presented
    // all in one list (nothing closed by default).
    if (this.allSites ||
        this.categorySubtype == settings.PermissionValues.ALLOW) {
      this.$.category.opened = true;
      return;
    }

    // Block list should only be shown opened if there is nothing to show in
    // the allowed list.
    if (this.category != settings.INVALID_CATEGORY_SUBTYPE) {
      this.browserProxy_.getExceptionList(this.category).then(
        function(exceptionList) {
          var allowExists = exceptionList.some(function(exception) {
            return exception.setting == settings.PermissionStringValues.ALLOW;
          });
          if (allowExists)
            return;
          this.$.category.opened = true;
      }.bind(this));
    } else {
      this.$.category.opened = true;
    }
  },

  /**
   * Makes sure the visibility is correct for this widget (e.g. hidden if the
   * block list is empty).
   * @private
   */
  updateCategoryVisibility_: function() {
    this.$.category.hidden =
        !this.showSiteList_(this.sites, this.categoryEnabled);
  },

  /**
   * Handles the expanding and collapsing of the sites list.
   * @private
   */
  onToggle_: function(e) {
    if (this.$.category.opened)
      this.$.icon.icon = 'icons:expand-less';
    else
      this.$.icon.icon = 'icons:expand-more';
  },

  /**
   * Populate the sites list for display.
   * @private
   */
  populateList_: function() {
    if (this.allSites) {
      this.getAllSitesList_().then(function(lists) {
        this.processExceptions_(lists);
      }.bind(this));
    } else {
      this.browserProxy_.getExceptionList(this.category).then(
        function(exceptionList) {
          this.processExceptions_([exceptionList]);
      }.bind(this));
    }
  },

  /**
   * Process the exception list returned from the native layer.
   * @param {!Array<!Array<SiteException>>} data List of sites (exceptions) to
   *     process.
   * @private
   */
  processExceptions_: function(data) {
    var sites = [];
    for (var i = 0; i < data.length; ++i)
      sites = this.appendSiteList_(sites, data[i]);
    this.sites = this.toSiteArray_(sites);
    this.updateCategoryVisibility_();
  },

  /**
   * Retrieves a list of all known sites (any category/setting).
   * @return {!Promise}
   * @private
   */
  getAllSitesList_: function() {
    var promiseList = [];
    for (var type in settings.ContentSettingsTypes) {
      promiseList.push(
          this.browserProxy_.getExceptionList(
              settings.ContentSettingsTypes[type]));
    }

    return Promise.all(promiseList);
  },

  /**
   * Appends to |list| the sites for a given category and subtype.
   * @param {!Array<SiteException>} sites The site list to add to.
   * @param {!Array<SiteException>} exceptionList List of sites (exceptions) to
   *     add.
   * @return {!Array<SiteException>} The list of sites.
   * @private
   */
  appendSiteList_: function(sites, exceptionList) {
    for (var i = 0; i < exceptionList.length; ++i) {
      if (this.category != settings.ALL_SITES) {
        // Filter out 'Block' values if this list is handling 'Allow' items.
        if (exceptionList[i].setting == settings.PermissionStringValues.BLOCK &&
            this.categorySubtype != settings.PermissionValues.BLOCK) {
          continue;
        }
        // Filter out 'Allow' values if this list is handling 'Block' items.
        if (exceptionList[i].setting == settings.PermissionStringValues.ALLOW &&
            this.categorySubtype != settings.PermissionValues.ALLOW) {
          continue;
        }
      }

      sites.push(exceptionList[i]);
    }
    return sites;
  },

  /**
   * Ensures the URL has a scheme (assumes http if omitted).
   */
  ensureUrlHasScheme_: function(url) {
    if (url.length == 0) return url;
    return url.indexOf('://') != -1 ? url : 'http://' + url;
  },

  /**
   * Converts an unordered site list to an ordered array, sorted by site name
   * then protocol and de-duped (by origin).
   * @param {!Array<SiteException>} sites A list of sites to sort and de-dup.
   * @private
   */
  toSiteArray_: function(sites) {
    var self = this;
    sites.sort(function(a, b) {
      // TODO(finnur): Hmm, it would probably be better to ensure scheme on the
      //     JS/C++ boundary.
      var originA = self.ensureUrlHasScheme_(a.origin);
      var originB = self.ensureUrlHasScheme_(b.origin);
      var embeddingOriginA = self.ensureUrlHasScheme_(a.embeddingOrigin);
      var embeddingOriginB = self.ensureUrlHasScheme_(b.embeddingOrigin);
      var url1 = new URL(originA);
      var url2 = new URL(originB);
      var embeddingUrl1 = embeddingOriginA.length == 0 ? '' :
          new URL(embeddingOriginA);
      var embeddingUrl2 = embeddingOriginB.length == 0 ? '' :
          new URL(embeddingOriginB);
      var comparison = url1.host.localeCompare(url2.host);
      if (comparison == 0) {
        comparison = url1.protocol.localeCompare(url2.protocol);
        if (comparison == 0) {
          comparison = url1.port.localeCompare(url2.port);
          if (comparison == 0)
            return embeddingUrl1.host.localeCompare(embeddingUrl2.host);
        }
      }
      return comparison;
    });
    var results = [];
    var lastOrigin = '';
    var lastEmbeddingOrigin = '';
    for (var i = 0; i < sites.length; ++i) {
      var origin = sites[i].origin;
      var embeddingOrigin = sites[i].embeddingOrigin;

      // The All Sites category can contain duplicates (from other categories).
      if (origin == lastOrigin && embeddingOrigin == lastEmbeddingOrigin)
        continue;

      var embeddingOriginForDisplay = '';
      if (embeddingOrigin != '*' && origin != embeddingOrigin)
        embeddingOriginForDisplay = embeddingOrigin;

      results.push({
         origin: origin,
         embeddingOrigin: embeddingOrigin,
         embeddingOriginForDisplay: embeddingOriginForDisplay,
      });

      lastOrigin = origin;
      lastEmbeddingOrigin = embeddingOrigin;
    }
    return results;
  },

  /**
   * Setup the values to use for the action menu.
   * @private
   */
  setUpActionMenu_: function() {
    this.showAllowAction_ =
        this.categorySubtype == settings.PermissionValues.BLOCK;
    this.showBlockAction_ =
        this.categorySubtype == settings.PermissionValues.ALLOW &&
        this.category != settings.ContentSettingsTypes.FULLSCREEN;
  },

  /**
   * A handler for selecting a site (by clicking on the origin).
   * @private
   */
  onOriginTap_: function(event) {
    this.selectedSite = event.model.item;
    var categorySelected =
        this.allSites ?
        'all-sites' :
        'site-settings-category-' + this.computeCategoryTextId(this.category);
    this.currentRoute = {
      page: this.currentRoute.page,
      section: 'privacy',
      subpage: ['site-settings', categorySelected, 'site-details'],
    };
  },

  /**
   * A handler for activating one of the menu action items.
   * @param {!{model: !{item: !{origin: string}},
   *           detail: !{item: !{textContent: string}}}} event
   * @private
   */
  onActionMenuIronActivate_: function(event) {
    var origin = event.model.item.origin;
    var embeddingOrigin = event.model.item.embeddingOrigin;
    var action = event.detail.item.textContent;
    if (action == this.i18n_.resetAction) {
      this.resetCategoryPermissionForOrigin(
          origin, embeddingOrigin, this.category);
    } else {
      var value = (action == this.i18n_.allowAction) ?
          settings.PermissionValues.ALLOW :
          settings.PermissionValues.BLOCK;
      this.setCategoryPermissionForOrigin(
          origin, embeddingOrigin, value, this.category);
    }
  },

  /**
   * Returns the appropriate header value for display.
   * @param {Array<string>} siteList The list of all sites to display for this
   *     category subtype.
   * @param {boolean} toggleState The state of the global toggle for this
   *     category.
   * @private
   */
  computeSiteListHeader_: function(siteList, toggleState) {
    if (this.categorySubtype == settings.PermissionValues.ALLOW) {
      return loadTimeData.getStringF(
          'titleAndCount',
          loadTimeData.getString(
              toggleState ? 'siteSettingsAllow' : 'siteSettingsExceptions'),
          siteList.length);
    } else {
      return loadTimeData.getStringF(
          'titleAndCount',
          loadTimeData.getString('siteSettingsBlock'),
          siteList.length);
    }
  },

  /**
   * Returns true if this widget is showing the allow list.
   * @private
   */
  isAllowList_: function() {
    return this.categorySubtype == settings.PermissionValues.ALLOW;
  },

  /**
   * Returns whether to show the site list.
   * @param {Array} siteList The list of all sites to display for this category
   *     subtype.
   * @param {boolean} toggleState The state of the global toggle for this
   *     category.
   * @private
   */
  showSiteList_: function(siteList, toggleState) {
    if (siteList.length == 0)
      return false;

    // The Block list is only shown when the category is set to Allow since it
    // is redundant to also list all the sites that are blocked.
    if (this.isAllowList_())
      return true;

    return toggleState;
  },

  /**
   * Returns the icon to use for a given site.
   * @param {string} url The url of the site to fetch the icon for.
   * @private
   */
  computeSiteIcon_: function(url) {
    // TODO(finnur): For now, we're returning a placeholder image for each site
    // but the actual favicon for each site will need to be returned.
    return 'communication:message';
  },
});
