// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   tool: string,
 *   size: number,
 *   color: string,
 * }}
 */
let AnnotationTool;

/**
 * Wraps the Ink component with an API that can be called
 * across an IFrame boundary.
 */
class InkAPI {
  /** @param {!ink.embed.EmbedComponent} embed */
  constructor(embed) {
    this.embed_ = embed;
    this.brush_ = ink.BrushModel.getInstance(embed);
  }

  /**
   * @param {!ArrayBuffer} buffer
   */
  setPDF(buffer) {
    // We change the type from ArrayBuffer to Uint8Array due to the consequences
    // of the buffer being passed across the iframe boundary. This realm has a
    // different ArrayBuffer constructor than `buffer`.
    // TODO(dstockwell): Update Ink to allow Uint8Array here.
    this.embed_.setPDF(
        /** @type {!ArrayBuffer} */ (
            /** @type {!*} */ (new Uint8Array(buffer))));
  }

  /**
   * @return {!Promise<Uint8Array>}
   */
  getPDF() {
    return this.embed_.getPDF();
  }

  /**
   * @return {!Uint8Array}
   */
  getPDFDestructive() {
    return this.embed_.getPDFDestructive();
  }

  setCamera(camera) {
    this.embed_.setCamera(camera);
  }

  /** @param {AnnotationTool} tool */
  setAnnotationTool(tool) {
    const shape = {
      eraser: 'MAGIC_ERASE',
      pen: 'INKPEN',
      highlighter: 'HIGHLIGHTER',
    }[tool.tool];
    this.brush_.setShape(shape);
    if (tool.tool != 'eraser') {
      this.brush_.setColor(tool.color);
    }
    this.brush_.setStrokeWidth(tool.size);
  }

  flush() {
    return new Promise(resolve => this.embed_.flush(resolve));
  }

  /** @param {string} hexColor */
  setOutOfBoundsColor(hexColor) {
    this.embed_.setOutOfBoundsColor(ink.Color.fromString(hexColor));
  }

  /** @param {string} url */
  setBorderImage(url) {
    this.embed_.setBorderImage(url);
  }

  /** @param {number} spacing in points */
  setPageSpacing(spacing) {
    this.embed_.setVerticalPageLayout(spacing);
  }

  dispatchPointerEvent(type, init) {
    const event = new PointerEvent(type, init);
    document.querySelector('#ink-engine').dispatchEvent(event);
  }
}

/** @return {Promise<InkAPI>} */
window.initInk = async function() {
  const config = new ink.embed.Config();
  const embed = await ink.embed.EmbedComponent.execute(config);
  embed.assignFlag(ink.proto.Flag.ENABLE_HOST_CAMERA_CONTROL, true);
  return new InkAPI(embed);
};
