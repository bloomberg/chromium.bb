// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {string} */
const State = {
  LOADING: 'loading',
  ACTIVE: 'active',
  IDLE: 'idle'
};

/**
 * Hosts the Ink component which is responsible for both PDF rendering and
 * annotation when in annotation mode.
 */
Polymer({
  is: 'viewer-ink-host',

  /** @private {InkAPI} */
  ink_: null,

  /** @private {?string} */
  fileName_: null,

  /** @private {State} */
  state_: State.IDLE,

  /** @param {AnnotationTool} tool */
  setAnnotationTool(tool) {
    this.tool_ = tool;
    if (this.state_ == State.ACTIVE) {
      this.ink_.setAnnotationTool(tool);
    }
  },

  /**
   * Begins annotation mode with the document represented by `data`.
   * When the return value resolves the Ink component will be ready
   * to render immediately.
   *
   * @param {string} fileName The name of the PDF file.
   * @param {ArrayBuffer} data The contents of the PDF document.
   * @param {Viewport} viewport
   * @return {!Promise} void value.
   */
  load: async function(fileName, data, viewport) {
    this.fileName_ = fileName;
    this.state_ = State.LOADING;
    this.$.frame.src = 'ink/index.html';
    await new Promise(resolve => this.$.frame.onload = resolve);
    this.ink_ = await this.$.frame.contentWindow.initInk();
    this.ink_.setPDF(data);
    this.state_ = State.ACTIVE;
    this.viewportChanged(viewport);
    this.style.visibility = 'visible';
  },

  viewportChanged: function(viewport) {
    if (this.state_ != State.ACTIVE) {
      return;
    }
    const pos = viewport.position;
    const size = viewport.size;
    const zoom = viewport.zoom;
    const documentWidth = viewport.getDocumentDimensions(zoom).width * zoom;
    // Adjust for page shadows.
    const y = pos.y - Viewport.PAGE_SHADOW.top * zoom;
    let x = pos.x - Viewport.PAGE_SHADOW.left * zoom;
    // Center the document if the width is smaller than the viewport.
    if (documentWidth < size.width) {
      x += (documentWidth - size.width) / 2;
    }
    // Invert the Y-axis and convert Pixels to Points.
    const pixelsToPoints = 72 / 96;
    const scale = pixelsToPoints / zoom;
    const camera = {
      top: (-y) * scale,
      left: (x) * scale,
      right: (x + size.width) * scale,
      bottom: (-y - size.height) * scale,
    };
    this.ink_.setCamera(camera);
  },

  /**
   * @return {!Promise<{fileName: string, dataToSave: ArrayBuffer}>}
   *     The serialized PDF document including any annotations that were made.
   */
  saveDocument: async function() {
    return {
      fileName: this.fileName_,
      dataToSave: await this.ink_.getPDF(),
    };
  },
});
