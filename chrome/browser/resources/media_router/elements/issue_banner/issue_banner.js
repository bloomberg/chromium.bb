// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('issue-banner', {
  publish: {
    /**
     * The issue to show.
     *
     * @attribute issue
     * @type {media_router.Issue}
     * @default: null
     */
    issue: null
  },

  /**
   * Fires an issue-action-click event. This is called when an issue action
   * is clicked.
   *
   * @param {!Event} event The event object.
   * @param {Object} detail The details of the event.
   * @param {!Element} sender Reference to clicked node.
   */
  onClickAction: function(event, detail, sender) {
    this.fire('issue-action-click', {
      id: this.issue.id,
      actionType: parseInt(sender.title),
      helpURL: this.issue.helpURL
    });
  }
});
