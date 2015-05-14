// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('log-panel', {
  publish: {
    /**
     * List of displayed logs.
     * @type {Array.<{{
     *    text: string,
     *    date: string,
     *    source: string
     * }}>}
     */
    logs: null,
  },

  /**
   * @type {boolean}
   * @private
   */
  isScrollAtBottom_: true,

  /**
   * Called after the Polymer element is initialized.
   */
  ready: function() {
    this.$.list.onscroll = this.onScroll_.bind(this);
    this.async(this.scrollToBottom_);
  },

  /**
   * Called when the list of logs change.
   */
  logsChanged: function(oldValue, newValue) {
    if (this.isScrollAtBottom_)
      this.async(this.scrollToBottom_);
  },

  /**
   * Clears the logs.
   * @private
   */
  clearLogs_: function() {
    this.$.logBuffer.clearLogs();
  },

  /**
   * Event handler when the list is scrolled.
   * @private
   */
  onScroll_: function() {
    var list = this.$.list;
    this.isScrollAtBottom_ =
        list.scrollTop + list.offsetHeight == list.scrollHeight;
  },

  /**
   * Scrolls the logs container to the bottom.
   * @private
   */
  scrollToBottom_: function() {
    this.$.list.scrollTop = this.$.list.scrollHeight;
  },

  /**
   * @param {string} filename
   * @return {string} The filename stripped of its preceeding path.
   * @private
   */
  stripPath_: function(filename) {
    var directories = filename.split('/');
    return directories[directories.length - 1];
  },
});
