// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper class to be used as the super class of all Custom Elements in
// chrome://omnibox. Constructed with a string id of a <template> element. When
// this element is added to the DOM, it instantiates its internal DOM from the
// corresponding <template> element.
class OmniboxElement extends HTMLElement {
  /** @param {string} templateId Template's HTML id attribute */
  constructor(templateId) {
    super();
    /** @type {string} */
    this.templateId = templateId;
  }

  /** @override */
  connectedCallback() {
    this.attachShadow({mode: 'open'});
    let template = $(this.templateId).content.cloneNode(true);
    this.shadowRoot.appendChild(template);
  }

  /**
   * Searches local shadow root for element by id
   * @param {string} id
   * @return {Element}
   */
  $$(id) {
    return this.shadowRoot.getElementById(id);
  }
}
