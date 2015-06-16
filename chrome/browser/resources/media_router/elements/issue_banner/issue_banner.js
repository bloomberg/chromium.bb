// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This Polymer element is used to show information about issues related
// to casting.
Polymer({
  is: 'issue-banner',

  properties: {
    /**
     * The issue to show.
     * @type {?media_router.Issue}
     */
    issue: {
      type: Object,
      value: null,
    },
  },

  /**
   * @param {?media_router.Issue} issue
   * @return {boolean} Whether or not to hide the blocking issue UI.
   * @private
   */
  computeIsBlockingIssueHidden_: function(issue) {
    return !issue || !issue.isBlocking;
  },

  /**
   * Returns true to hide the non-blocking issue UI, false to show it.
   *
   * @param {?media_router.Issue} issue
   * @private
   */
  computeIsNonBlockingIssueHidden_: function(issue) {
    return !issue || issue.isBlocking;
  },

  /**
   * @param {?media_router.Issue} issue
   * @return {boolean} Whether or not to hide the non-blocking issue UI.
   * @private
   */
  computeOptionalActionHidden_: function(issue) {
    return !issue || !issue.optActionText;
  },

  /**
   * Fires an issue-action-click event.
   *
   * @param {number} actionType The type of issue action.
   * @private
   */
  fireIssueActionClick_: function(actionType) {
    this.fire('issue-action-click', {
      id: this.issue.id,
      actionType: actionType,
      helpPageId: this.issue.helpPageId,
    });
  },

  /**
   * Called when a default issue action is clicked.
   *
   * @param {!Event} event The event object.
   * @private
   */
  onClickDefaultAction_: function(event) {
    this.fireIssueActionClick_(this.issue.defaultActionType);
  },

  /**
   * Called when an optional issue action is clicked.
   *
   * @param {!Event} event The event object.
   * @private
   */
  onClickOptAction_: function(event) {
    this.fireIssueActionClick_(this.issue.optActionType);
  },
});
