// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-main-view',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /**
     * List of all apps.
     * @private {Array<App>}
     */
    apps_: {
      type: Array,
      value: () => [],
      observer: 'onAppsChanged_',
    },

    /**
     * List of apps displayed before expanding the app list.
     * @private {Array<App>}
     */
    displayedApps_: {
      type: Array,
      value: () => [],
    },

    /**
     * List of apps displayed after expanding app list.
     * @private {Array<App>}
     */
    collapsedApps_: {
      type: Array,
      value: () => [],
    },

    /**
     * @private {boolean}
     */
    listExpanded_: {
      type: Boolean,
      value: false,
    },

    /**
     * List of apps with the notification permission.
     * @private {Array<App>}
     */
    notificationApps_: {
      type: Array,
      value: () => [],
      observer: 'getNotificationSublabel_',
    },
  },

  attached: function() {
    this.watch('apps_', function(state) {
      return Object.values(state.apps);
    });
    this.updateFromStore();
  },

  /**
   * @param {number} numApps
   * @param {boolean} listExpanded
   * @return {string}
   * @private
   */
  moreAppsString_: function(numApps, listExpanded) {
    return listExpanded ?
        loadTimeData.getString('lessApps') :
        loadTimeData.getStringF(
            'moreApps', numApps - NUMBER_OF_APPS_DISPLAYED_DEFAULT);
  },

  /**
   * @private
   */
  toggleListExpanded_: function() {
    this.listExpanded_ = !this.listExpanded_;
    this.onAppsChanged_();
  },

  /**
   * @private
   */
  onAppsChanged_: function() {
    this.$['expander-row'].hidden =
        this.apps_.length <= NUMBER_OF_APPS_DISPLAYED_DEFAULT;
    this.displayedApps_ = this.apps_.slice(0, NUMBER_OF_APPS_DISPLAYED_DEFAULT);
    this.collapsedApps_ =
        this.apps_.slice(NUMBER_OF_APPS_DISPLAYED_DEFAULT, this.apps_.length);
    this.notificationApps_ = this.apps_.slice();
  },

  /**
   * @param {boolean} listExpanded
   * @return {string}
   * @private
   */
  getCollapsedIcon_: function(listExpanded) {
    return listExpanded ? 'cr:expand-less' : 'cr:expand-more';
  },

  /** @private */
  onClickNotificationSublabel_: function() {
    this.dispatch(app_management.actions.changePage(PageType.NOTIFICATIONS));
  },

  /**
   * Show a string with apps' |title|(s) previewed into a label, with each
   * title ellipsised if too long.
   * @private
   */
  getNotificationSublabelPieces_: async function() {
    const /** @type {string} */ label = await cr.sendWithPromise(
        'getPluralString', 'appListPreview', this.notificationApps_.length);

    const substitutions = [];
    for (let i = 0;
         i < APP_LIST_PREVIEW_APP_TITLES && i < this.notificationApps_.length;
         i++) {
      substitutions.push(this.notificationApps_[i].title);
    }

    // Add X more apps if the length is more than APP_LIST_PREVIEW_APP_TITLES.
    if (this.notificationApps_.length >= APP_LIST_PREVIEW_APP_TITLES + 1) {
      substitutions.push(
          this.notificationApps_.length - APP_LIST_PREVIEW_APP_TITLES);
    }
    // Only APP_LIST_PREVIEW_APP_TITLES of apps' titles get ellipsised
    // if too long. the element after that is "X other apps"
    const placeholder = APP_LIST_PREVIEW_APP_TITLES + 1;
    const pieces =
        loadTimeData.getSubstitutedStringPieces(label, ...substitutions)
            .map(function(p) {
              // Make the titles of app collapsible but make the number in the
              // "X other app(s)" part non-collapsible.
              p.collapsible = !!p.arg && p.arg != '$' + placeholder;
              return p;
            });
    return pieces;
  },

  /**
   * Create <span> for each app title previewed,
   * making certain text fragments collapsible.
   */
  getNotificationSublabel_: async function() {
    const pieces = await this.getNotificationSublabelPieces_();
    // Create <span> for each app title previewed,
    // making certain text fragments collapsible.
    const textContainer = this.$['notifications-sublabel'];
    textContainer.textContent = '';
    for (const p of pieces) {
      if (p.value.length == 0) {
        return;
      }

      const span = document.createElement('span');
      span.textContent = p.value;
      if (p.collapsible) {
        span.classList.add('collapsible');
      }

      textContainer.appendChild(span);
    }
  },
});
