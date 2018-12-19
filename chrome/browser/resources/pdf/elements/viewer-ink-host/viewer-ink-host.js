// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Hosts the Ink component which is responsible for both PDF rendering and
 * annotation when in annotation mode.
 */
Polymer({
  is: 'viewer-ink-host',

  properties: {
    /** @private */
    dummyContent_: String,
  },

  /** @private {?string} */
  dummyFileName_: null,

  /** @private {ArrayBuffer} */
  dummyData_: null,

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
    this.dummyContent_ = `Annotating ${data.byteLength} bytes`;
    this.dummyFileName_ = fileName;
    this.dummyData_ = data;
    this.style.visibility = 'visible';
  },

  /**
   * @return {!Promise<{fileName: string, dataToSave: ArrayBuffer}>}
   *     The serialized PDF document including any annotations that were made.
   */
  saveDocument: async function() {
    return {
      fileName: this.dummyFileName_,
      dataToSave: this.dummyData_,
    };
  },
});
