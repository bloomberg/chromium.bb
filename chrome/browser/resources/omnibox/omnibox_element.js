// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Helper class to be used as the super class of all custom elements in
 * chrome://omnibox.
 * @abstract
 */
class OmniboxElement extends HTMLElement {
  /** @param {string} templateId */
  constructor(templateId) {
    super();
    this.attachShadow({mode: 'open'});
    const template = OmniboxElement.getTemplate(templateId);
    this.shadowRoot.appendChild(template);
    /** @type {!ShadowRoot} */
    this.shadowRoot;
  }

  /**
   * Get an element that's known to exist within this OmniboxElement.
   * Searches local shadow root for element by id.
   * @param {string} id
   * @return {!Element}
   */
  $$(id) {
    return OmniboxElement.getById_(id, (this.shadowRoot));
  }

  /**
   * Get a template that's known to exist within the DOM document.
   * @param {string} templateId
   * @return {!Element}
   */
  static getTemplate(templateId) {
    return OmniboxElement.getById_(templateId).content.cloneNode(true);
  }

  /**
   * Get an element that's known to exist by its ID. We use this instead of just
   * calling getElementById because this lets us satisfy the JSCompiler type
   * system.
   * @private
   * @param {string} id
   * @param {!Node=} context
   * @return {!Element}
   */
  static getById_(id, context) {
    return assertInstanceof(
        (context || document).getElementById(id), Element,
        `Missing required element: ${id}`);
  }
}
