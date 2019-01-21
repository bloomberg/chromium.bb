// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-router',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /** @private {string} */
    path_: String,

    /** @private {Object} */
    queryParams_: Object,

    /** @private {string} */
    query_: {
      type: String,
      observer: 'onQueryChanged_',
    },

    /** @private {string} */
    urlQuery_: {
      type: String,
      observer: 'onUrlQueryChanged_',
    },

    /** @private {PageType} */
    currentPageType_: {
      type: Number,
    },

    /** @private {?string} */
    selectedAppId_: {
      type: String,
    },
  },

  observers: [
    'onUrlChanged_(path_, queryParams_)',
    'onStateChanged_(currentPageType_, selectedAppId_)',
  ],

  attached: function() {
    this.watch('currentPageType_', function(state) {
      return state.currentPage.pageType;
    });
    this.watch('selectedAppId_', function(state) {
      return state.currentPage.selectedAppId;
    });
    this.updateFromStore();
  },

  /**
   * @param {?string} current Current value of the query.
   * @param {?string} previous Previous value of the query.
   * @private
   */
  onQueryChanged_: function(current, previous) {
    if (previous !== undefined) {
      this.urlQuery_ = this.query_;
    }
  },

  /** @private */
  onUrlQueryChanged_: function() {
    this.query_ = this.urlQuery_;
  },

  /** @private */
  onStateChanged_: function() {
    this.debounce('publishUrl', this.publishUrl_);
  },

  /** @private */
  publishUrl_: function() {
    this.publishQueryParams_();
    this.publishPath_();
  },

  /** @private */
  publishQueryParams_: function() {
    const newId = this.selectedAppId_;

    this.queryParams_.id = newId;
    if (!newId) {
      delete this.queryParams_.id;
    }

    this.queryParams_ = Object.assign({}, this.queryParams_);
  },

  /** @private */
  publishPath_: function() {
    let path = '';
    if (this.currentPageType_ === PageType.DETAIL) {
      path = 'detail';
    } else if (this.currentPageType_ === PageType.NOTIFICATIONS) {
      path = 'notifications';
    }

    this.path_ = '/' + path;
  },

  /** @private */
  onUrlChanged_: function() {
    this.debounce('parseUrl', this.parseUrl_);
  },

  /** @private */
  parseUrl_: function() {
    const newId = this.queryParams_.id;

    const pageFromUrl = this.path_.substr(1).split('/')[0];
    let newPage = PageType.MAIN;
    if (pageFromUrl === 'detail') {
      newPage = PageType.DETAIL;
    } else if (pageFromUrl === 'notifications') {
      newPage = PageType.NOTIFICATIONS;
    } else {
      newPage = PageType.MAIN;
    }

    if (newPage === PageType.DETAIL) {
      this.dispatch(app_management.actions.changePage(PageType.DETAIL, newId));
    } else {
      this.dispatch(app_management.actions.changePage(newPage));
    }
  },
});
