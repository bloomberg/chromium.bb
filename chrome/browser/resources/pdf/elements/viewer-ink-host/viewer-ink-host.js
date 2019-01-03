// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  /**
   * Begins annotation mode with the document represented by `data`.
   * When the return value resolves the Ink component will be ready
   * to render immediately.
   *
   * @param {string} fileName The name of the PDF file.
   * @param {ArrayBuffer} data The contents of the PDF document.
   * @return {!Promise} void value.
   */
  load: async function(fileName, data) {
    this.fileName_ = fileName;
    this.state_ = State.LOADING;
    this.$.frame.src = 'ink/index.html';
    await new Promise(resolve => this.$.frame.onload = resolve);
    this.ink_ = await this.$.frame.contentWindow.initInk();
    this.ink_.setPDF(data);
    this.state_ = State.ACTIVE;
    this.style.visibility = 'visible';
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
