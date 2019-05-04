// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'graph-tab',

  /**
   * The Mojo graph data source.
   *
   * @private {performanceManager.mojom.WebUIGraphDumpProxy}
   */
  graphDump_: null,

  /** @private {number} The current update timer if any. */
  updateTimer_: 0,

  /** @override */
  ready: function() {
    this.graphDump_ = performanceManager.mojom.WebUIGraphDump.getProxy();
  },

  /** @override */
  detached: function() {
    // Clear the update timer to avoid memory leaks.
    if (this.updateTimer_) {
      clearInterval(this.updateTimer_);
      this.updateTimer_ = 0;
    }
  },

  /** @private */
  onWebViewReady_: function() {
    // Set up regular updates.
    this.updateTimer_ = setInterval(() => {
      this.graphDump_.getCurrentGraph().then(response => {
        this.onGraphDump_(response.graph);
      });
    }, 1000);
  },

  /** @private */
  onGraphDump_: function(graph) {
    this.$.webView.contentWindow.postMessage(graph, '*');
  },
});
