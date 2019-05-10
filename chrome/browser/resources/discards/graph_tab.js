// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('graph_tab', function() {
  'use strict';
  /**
   * @implements {performanceManager.mojom.WebUIGraphChangeStreamInterface}
   */
  class WebUIGraphChangeStreamImpl {
    constructor(contentWindow) {
      this.contentWindow_ = contentWindow;
    }

    /**
     * @param {string} type
     * @param {Object|number} data
     */
    postMessage_(type, data) {
      this.contentWindow_.postMessage([type, data], '*');
    }

    /**
     * @param {!performanceManager.mojom.WebUIFrameInfo} frame
     */
    frameCreated(frame) {
      this.postMessage_('frameCreated', frame);
    }

    /**
     * @param {!performanceManager.mojom.WebUIPageInfo} page
     */
    pageCreated(page) {
      this.postMessage_('pageCreated', page);
    }

    /**
     * @param {!performanceManager.mojom.WebUIProcessInfo} process
     */
    processCreated(process) {
      this.postMessage_('processCreated', process);
    }

    /**
     * @param {!performanceManager.mojom.WebUIFrameInfo} frame
     */
    frameChanged(frame) {
      this.postMessage_('frameChanged', frame);
    }

    /**
     * @param {!performanceManager.mojom.WebUIPageInfo} page
     */
    pageChanged(page) {
      this.postMessage_('pageChanged', page);
    }

    /**
     * @param {!performanceManager.mojom.WebUIProcessInfo} process
     */
    processChanged(process) {
      this.postMessage_('processChanged', process);
    }

    /**
     * @param {!number} nodeId
     */
    nodeDeleted(nodeId) {
      this.postMessage_('nodeDeleted', nodeId);
    }
  }

  return {
    WebUIGraphChangeStreamImpl: WebUIGraphChangeStreamImpl,
  };
});

Polymer({
  is: 'graph-tab',

  /**
   * The Mojo graph data source.
   *
   * @private {performanceManager.mojom.WebUIGraphDumpProxy}
   */
  graphDump_: null,

  /**
   * The graph change listener.
   *
   * @private {performanceManager.mojom.WebUIGraphChangeStreamInterface}
   */
  changeListener_: null,

  /** @override */
  ready: function() {
    this.graphDump_ = performanceManager.mojom.WebUIGraphDump.getProxy();
  },

  /** @override */
  detached: function() {
    // TODO(siggi): Is there a way to tear down the binding explicitly?
    this.graphDump_ = null;
    this.changeListener_ = null;
  },

  /** @private */
  onWebViewReady_: function() {
    this.changeListener_ =
        new graph_tab.WebUIGraphChangeStreamImpl(this.$.webView.contentWindow);
    const client = new performanceManager.mojom
                       .WebUIGraphChangeStream(this.changeListener_)
                       .createProxy();
    // Subscribe for graph updates.
    this.graphDump_.subscribeToChanges(client);
  },
});
