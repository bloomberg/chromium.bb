// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class OmniboxOutput extends OmniboxElement {
  /** @return {string} */
  static get is() {
    return 'omnibox-output';
  }

  constructor() {
    super('omnibox-output-template');
  }

  /** @param {Element} element the element to the output */
  addOutput(element) {
    this.$$('contents').appendChild(element);
  }

  clearOutput() {
    while (this.$$('contents').firstChild)
      this.$$('contents').removeChild(this.$$('contents').firstChild);
  }
}

window.customElements.define(OmniboxOutput.is, OmniboxOutput);
