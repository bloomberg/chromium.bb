// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   inputText: string,
 *   cursorPosition: number,
 *   preventInlineAutocomplete: boolean,
 *   preferKeyword: boolean,
 *   pageClassification: number,
 * }}
 */
let QueryInputs;

/**
 * @typedef {{
 *   showIncompleteResults: boolean,
 *   showDetails: boolean,
 *   showAllProviders: boolean,
 * }}
 */
let DisplayInputs;

class OmniboxInputs extends OmniboxElement {
  /** @return {string} */
  static get is() {
    return 'omnibox-inputs';
  }

  constructor() {
    super('omnibox-inputs-template');
  }

  /** @override */
  connectedCallback() {
    this.setupElementListeners_();
  }

  /** @private */
  setupElementListeners_() {
    const onQueryInputsChanged = this.onQueryInputsChanged_.bind(this);
    const onDisplayInputsChanged = this.onDisplayInputsChanged_.bind(this);

    this.$$('input-text').addEventListener('input', onQueryInputsChanged);
    [
      this.$$('prevent-inline-autocomplete'),
      this.$$('prefer-keyword'),
      this.$$('page-classification'),
    ].forEach(elem => elem.addEventListener('change', onQueryInputsChanged));
    [
      this.$$('show-incomplete-results'),
      this.$$('show-details'),
      this.$$('show-all-providers'),
    ].forEach(elem => elem.addEventListener('change', onDisplayInputsChanged));
  }

  /** @private */
  onQueryInputsChanged_() {
    /** @type {QueryInputs} */
    const queryInputs = {
      inputText: this.$$('input-text').value,
      cursorPosition: this.$$('input-text').selectionEnd,
      preventInlineAutocomplete: this.$$('prevent-inline-autocomplete').checked,
      preferKeyword: this.$$('prefer-keyword').checked,
      pageClassification: this.$$('page-classification').value,
    };
    this.dispatchEvent(
        new CustomEvent('query-inputs-changed', {detail: queryInputs}));
  }

  /** @private */
  onDisplayInputsChanged_() {
    /** @type {DisplayInputs} */
    const displayInputs = {
      showIncompleteResults: this.$$('show-incomplete-results').checked,
      showDetails: this.$$('show-details').checked,
      showAllProviders: this.$$('show-all-providers').checked,
    };
    this.dispatchEvent(
        new CustomEvent('display-inputs-changed', {detail: displayInputs}));
  }
}

window.customElements.define(OmniboxInputs.is, OmniboxInputs);
