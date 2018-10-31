// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Graph {
  /**
   * TODO(siggi): This should be SVGElement, but closure doesn't have externs
   *    for this yet.
   * @param {Element} svg
   */
  constructor(svg) {
    /**
     * TODO(siggi): SVGElement.
     * @private {Element}
     */
    this.svg_ = svg;

    /** @private {number} */
    this.width_ = 100;
    /** @private {number} */
    this.height_ = 100;
  }

  initialize() {
    // Set up a message listener to receive the graph data from the WebUI.
    // This is hosted in a webview that is never navigated anywhere else,
    // so these event handlers are never removed.
    window.addEventListener('message', (graph) => {
      this.onMessage_(graph);
    }, false);

    // Set up a window resize listener to track the graph on resize.
    window.addEventListener('resize', () => {
      this.onResize_();
    });

    // TODO(siggi): Create the graph.
  }

  /**
   * @param {!Event} event A graph update event posted from the WebUI.
   * @private
   */
  onMessage_(event) {
    this.updateGraph_(event.data);
  }

  /**
   * @param {resourceCoordinator.mojom.WebUIGraph} graph An updated graph from
   *     the WebUI.
   * @private
   */
  updateGraph_(graph) {
    // TODO(siggi): update the graph
  }

  /** @private */
  onResize_() {
    // TODO(siggi): update the graph.
  }
}

let graph = null;
function onLoad() {
  graph = new Graph(document.querySelector('svg'));

  graph.initialize();
}

window.addEventListener('load', onLoad);
