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
    this.$$('lock-cursor-position')
        .addEventListener('change', onQueryInputsChanged);
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
    this.$$('copy-text')
        .addEventListener('click', () => this.onCopyOutput_('text'));
    this.$$('copy-json')
        .addEventListener('click', () => this.onCopyOutput_('json'));
  }

  /** @private */
  onQueryInputsChanged_() {
    /** @type {QueryInputs} */
    const queryInputs = {
      inputText: this.$$('input-text').value,
      cursorPosition: this.cursorPosition_,
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

  /**
   * @private
   * @return {number}
   */
  get cursorPosition_() {
    return this.$$('lock-cursor-position').checked ?
        this.$$('input-text').value.length :
        this.$$('input-text').selectionEnd;
  }

  /**
   * @param {string} format Either 'text' or 'json'.
   * @private
   */
  onCopyOutput_(format) {
    this.dispatchEvent(new CustomEvent('copy-request', {detail: format}));
  }
}

window.customElements.define(OmniboxInputs.is, OmniboxInputs);
