// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Wraps the Ink component with an API that can be called
 * across an IFrame boundary.
 */
class InkAPI {
  constructor() {
    /** @type {ArrayBuffer} */
    this.buffer_ = null;
  }

  /**
   * @param {ArrayBuffer} buffer
   */
  setPDF(buffer) {
    this.buffer_ = buffer;
  }

  /**
   * @return {ArrayBuffer}
   */
  getPDF() {
    return this.buffer_;
  }
}

/**
 * @return {InkAPI}
 */
window.initInk = function() {
  // TODO(dstockwell): Create Ink embed and pass to InkAPI.
  return new InkAPI();
};