// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enumeration mapping all possible controlled-by values for exceptions to
 * icons.
 * @enum {string}
 */
var iconControlledBy = {
  'extension': 'cr:extension',
  'HostedApp': 'cr:extension',
  'platform_app': 'cr:extension',
  'policy' : 'cr:domain',
};

/**
 * @fileoverview
 * 'site-list' shows a list of Allowed and Blocked sites for a given
 * category.
 */
Polymer({

  is: 'site-list',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
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
      * either 'allow' or 'block', representing which sites are allowed or
      * blocked respectively.
      */
    categorySubtype: {
      type: String,
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
     * Whether to show the 'Clear on exit' action in the action
     * menu.
     */
    showSessionOnlyAction_: Boolean,

    /**
     * Keeps track of the incognito status of the current profile (whether one
     * exists).
     */
    incognitoProfileActive_: {
      type: Boolean,
      value: false,
    },

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
        SESSION_ONLY: 'SessionOnly',
      }
    },
  },

  observers: [
    'configureWidget_(category, categorySubtype, categoryEnabled, allSites)'
  ],

  ready: function() {
    this.addWebUIListener('contentSettingSitePermissionChanged',
        this.siteWithinCategoryChanged_.bind(this));
    this.addWebUIListener('onIncognitoStatusChanged',
        this.onIncognitoStatusChanged_.bind(this));
  },

  /**
   * Called when a site changes permission.
   * @param {string} category The category of the site that changed.
   * @param {string} site The site that changed.
   * @private
   */
  siteWithinCategoryChanged_: function(category, site) {
    if (category == this.category || this.allSites)
      this.configureWidget_();
  },

  onIncognitoStatusChanged_: function(incognitoEnabled) {
    // A change notification is not sent for each site that is deleted during
    // incognito profile destruction. Therefore, we reconfigure the list when
    // the incognito profile is destroyed, except for SESSION_ONLY, which won't
    // have any incognito exceptions.
    if (this.categorySubtype == settings.PermissionValues.SESSION_ONLY)
      return;

    if (this.incognitoProfileActive_)
      this.configureWidget_();  // The incognito profile is being destroyed.

    this.incognitoProfileActive_ = incognitoEnabled;
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
    // Allowed list and Clear on Exit lists are always shown opened by default
    // and All Sites is presented all in one list (nothing closed by default).
    if (this.allSites ||
        this.categorySubtype == settings.PermissionValues.ALLOW ||
        this.categorySubtype == settings.PermissionValues.SESSION_ONLY) {
      this.$.category.opened = true;
      return;
    }

    // Block list should only be shown opened if there is nothing to show in
    // the other lists.
    if (this.category != settings.INVALID_CATEGORY_SUBTYPE) {
      this.browserProxy_.getExceptionList(this.category).then(
        function(exceptionList) {
          var othersExists = exceptionList.some(function(exception) {
            return exception.setting == settings.PermissionValues.ALLOW ||
                exception.setting == settings.PermissionValues.SESSION_ONLY;
          });
          if (othersExists)
            return;
          this.$.category.opened = true;
      }.bind(this));
    } else {
      this.$.category.opened = true;
    }
  },

  /**
   * Returns which icon, if any, should represent the fact that this exception
   * is controlled.
   * @param {!SiteException} item The item from the list we're computing the
   *    icon for.
   * @return {string} The icon to show (or blank, if none).
   */
  computeIconControlledBy_: function(item) {
    if (this.allSites)
      return '';
    return iconControlledBy[item.source] || '';
  },

  /**
   * @param {string} source Where the setting came from.
   * @return {boolean}
   * @private
   */
  shouldShowMenu_: function(source) {
    return !(this.isExceptionControlled_(source) || this.allSites);
  },

  /**
   * Makes sure the visibility is correct for this widget.
   * @private
   */
  updateCategoryVisibility_: function() {
    this.$.category.hidden =
        !this.showSiteList_(this.sites, this.categoryEnabled);
  },

  /**
   * A handler for the Add Site button.
   * @private
   */
  onAddSiteTap_: function() {
    var dialog = document.createElement('add-site-dialog');
    dialog.category = this.category;
    this.shadowRoot.appendChild(dialog);

    dialog.open(this.categorySubtype);

    dialog.addEventListener('close', function() {
      dialog.remove();
    });
  },

  /**
   * Handles the expanding and collapsing of the sites list.
   * @private
   */
  onToggle_: function(e) {
    if (this.$.category.opened)
      this.$.icon.icon = 'cr:expand-less';
    else
      this.$.icon.icon = 'cr:expand-more';
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
      if (settings.ContentSettingsTypes[type] ==
          settings.ContentSettingsTypes.PROTOCOL_HANDLERS ||
          settings.ContentSettingsTypes[type] ==
          settings.ContentSettingsTypes.USB_DEVICES ||
          settings.ContentSettingsTypes[type] ==
          settings.ContentSettingsTypes.ZOOM_LEVELS) {
        // Some categories store their data in a custom way.
        continue;
      }

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
      if (!this.allSites) {
        if (exceptionList[i].setting == settings.PermissionValues.DEFAULT)
          continue;

        if (exceptionList[i].setting != this.categorySubtype)
          continue;
      }

      sites.push(exceptionList[i]);
    }
    return sites;
  },

  /**
   * Converts an unordered site list to an ordered array, sorted by site name
   * then protocol and de-duped (by origin).
   * @param {!Array<SiteException>} sites A list of sites to sort and de-dupe.
   * @return {!Array<SiteException>} Sorted and de-duped list.
   * @private
   */
  toSiteArray_: function(sites) {
    var self = this;
    sites.sort(function(a, b) {
      var url1 = self.toUrl(a.origin);
      var url2 = self.toUrl(b.origin);
      var comparison = url1.host.localeCompare(url2.host);
      if (comparison == 0) {
        comparison = url1.protocol.localeCompare(url2.protocol);
        if (comparison == 0) {
          comparison = url1.port.localeCompare(url2.port);
          if (comparison == 0) {
            // Compare hosts for the embedding origins.
            var host1 = self.toUrl(a.embeddingOrigin);
            var host2 = self.toUrl(b.embeddingOrigin);
            host1 = (host1 == null) ? '' : host1.host;
            host2 = (host2 == null) ? '' : host2.host;
            return host1.localeCompare(host2);
          }
        }
      }
      return comparison;
    });
    var results = /** @type {!Array<SiteException>} */([]);
    var lastOrigin = '';
    var lastEmbeddingOrigin = '';
    for (var i = 0; i < sites.length; ++i) {
      /** @type {!SiteException} */
      var siteException = this.expandSiteException(sites[i]);

      // The All Sites category can contain duplicates (from other categories).
      if (siteException.originForDisplay == lastOrigin &&
          siteException.embeddingOriginForDisplay == lastEmbeddingOrigin) {
        continue;
      }

      results.push(siteException);
      lastOrigin = siteException.originForDisplay;
      lastEmbeddingOrigin = siteException.embeddingOriginForDisplay;
    }
    return results;
  },

  /**
   * Setup the values to use for the action menu.
   * @private
   */
  setUpActionMenu_: function() {
    this.showAllowAction_ =
        this.categorySubtype != settings.PermissionValues.ALLOW;
    this.showBlockAction_ =
        this.categorySubtype != settings.PermissionValues.BLOCK;
    this.showSessionOnlyAction_ =
        this.categorySubtype != settings.PermissionValues.SESSION_ONLY &&
        this.category == settings.ContentSettingsTypes.COOKIES;
  },

  /**
   * Whether to show the Session Only menu item for a given site.
   * @param {SiteException} site The site in question.
   * @return {boolean} Whether to show the menu item.
   */
  showSessionOnlyActionForSite_: function(site) {
    // It makes no sense to show "clear on exit" for exceptions that only apply
    // to incognito. It gives the impression that they might under some
    // circumstances not be cleared on exit, which isn't true.
    if (site.incognito)
      return false;

    return this.showSessionOnlyAction_;
  },

  /**
   * A handler for selecting a site (by clicking on the origin).
   * @private
   */
  onOriginTap_: function(event) {
    this.selectedSite = event.model.item;
    settings.navigateTo(settings.Route.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + this.selectedSite.origin));
  },

  /**
   * A handler for activating one of the menu action items.
   * @param {!{model: !{item: !{origin: string}}}} event
   * @param {string} action The permission to set (Allow, Block, SessionOnly,
   *     etc).
   * @private
   */
  onActionMenuActivate_: function(event, action) {
    var origin = event.model.item.origin;
    var incognito = event.model.item.incognito;
    var embeddingOrigin = event.model.item.embeddingOrigin;
    if (action == settings.PermissionValues.DEFAULT) {
      this.browserProxy.resetCategoryPermissionForOrigin(
          origin, embeddingOrigin, this.category, incognito);
    } else {
      this.browserProxy.setCategoryPermissionForOrigin(
          origin, embeddingOrigin, this.category, action, incognito);
    }
  },

  /** @private */
  onAllowTap_: function(event) {
    this.onActionMenuActivate_(event, settings.PermissionValues.ALLOW);
  },

  /** @private */
  onBlockTap_: function(event) {
    this.onActionMenuActivate_(event, settings.PermissionValues.BLOCK);
  },

  /** @private */
  onSessionOnlyTap_: function(event) {
    this.onActionMenuActivate_(event, settings.PermissionValues.SESSION_ONLY);
  },

  /** @private */
  onResetTap_: function(event) {
    this.onActionMenuActivate_(event, settings.PermissionValues.DEFAULT);
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
    var title = '';
    if (this.categorySubtype == settings.PermissionValues.ALLOW) {
      title = loadTimeData.getString(
          toggleState ? 'siteSettingsAllow' : 'siteSettingsExceptions');
    } else if (this.categorySubtype == settings.PermissionValues.BLOCK) {
      title = loadTimeData.getString('siteSettingsBlock');
    } else if (this.categorySubtype == settings.PermissionValues.SESSION_ONLY) {
      title = loadTimeData.getString('siteSettingsSessionOnly');
    } else {
      return title;
    }
    return loadTimeData.getStringF('titleAndCount', title, siteList.length);
  },

  /**
   * Returns the appropriate site description to display. This can, for example,
   * be blank, an 'embedded on <site>' or 'Current incognito session' (or a
   * mix of the last two).
   * @param {SiteException} item The site exception entry.
   * @return {string} The site description.
   */
  computeSiteDescription_: function(item) {
    if (item.incognito && item.embeddingOriginForDisplay.length > 0) {
      return loadTimeData.getStringF('embeddedIncognitoSite',
          item.embeddingOriginForDisplay);
    }

    if (item.incognito)
      return loadTimeData.getString('incognitoSite');
    return item.embeddingOriginForDisplay;
  },

  /**
   * Returns true if this widget is showing the Allow list.
   * @private
   */
  isAllowList_: function() {
    return this.categorySubtype == settings.PermissionValues.ALLOW;
  },

  /**
   * Returns true if this widget is showing the Session Only list.
   * @private
   */
  isSessionOnlyList_: function() {
    return this.categorySubtype == settings.PermissionValues.SESSION_ONLY;
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
    // The Block list is only shown when the category is set to Allow since it
    // is redundant to also list all the sites that are blocked.
    if (this.isAllowList_())
      return true;

    if (this.isSessionOnlyList_())
      return siteList.length > 0;

    return toggleState;
  },
});
