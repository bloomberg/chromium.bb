// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   inputText: string,
 *   resetAutocompleteController: boolean,
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

class OmniboxInput extends OmniboxElement {
  constructor() {
    super('omnibox-input-template');

    const displayInputs = OmniboxInput.defaultDisplayInputs;
    this.$$('#show-incomplete-results').checked =
        displayInputs.showIncompleteResults;
    this.$$('#show-details').checked = displayInputs.showDetails;
    this.$$('#show-all-providers').checked = displayInputs.showAllProviders;
  }

  /** @override */
  connectedCallback() {
    this.setupElementListeners_();
  }

  /** @private */
  setupElementListeners_() {
    ['#input-text',
     '#reset-autocomplete-controller',
     '#lock-cursor-position',
     '#zero-suggest',
     '#prevent-inline-autocomplete',
     '#prefer-keyword',
     '#current-url',
     '#page-classification',
    ]
        .forEach(
            query => this.$$(query).addEventListener(
                'input', this.onQueryInputsChanged_.bind(this)));

    ['#show-incomplete-results',
     '#show-details',
     '#show-all-providers',
    ]
        .forEach(
            query => this.$$(query).addEventListener(
                'input', this.onDisplayInputsChanged_.bind(this)));

    this.$$('#copy-text')
        .addEventListener('click', () => this.onCopyOutput_('text'));
    this.$$('#copy-json')
        .addEventListener('click', () => this.onCopyOutput_('json'));

    this.$$('#filter-text')
        .addEventListener('input', this.onFilterInputsChanged_.bind(this));

    // Set text of .arrow-padding to substring of #input-text text, from
    // beginning until cursor position, in order to correctly align .arrow-up.
    this.$$('#input-text')
        .addEventListener(
            'input',
            () => this.$$('.arrow-padding').textContent =
                this.$$('#input-text')
                    .value.substring(0, this.cursorPosition_));
  }

  /** @private */
  onQueryInputsChanged_() {
    const zeroSuggest = this.$$('#zero-suggest').checked;
    this.$$('#current-url').disabled = zeroSuggest;
    if (zeroSuggest) {
      this.$$('#current-url').value = this.$$('#input-text').value;
    }

    /** @type {!QueryInputs} */
    const queryInputs = {
      inputText: this.$$('#input-text').value,
      resetAutocompleteController:
          this.$$('#reset-autocomplete-controller').checked,
      cursorPosition: this.cursorPosition_,
      zeroSuggest: zeroSuggest,
      preventInlineAutocomplete:
          this.$$('#prevent-inline-autocomplete').checked,
      preferKeyword: this.$$('#prefer-keyword').checked,
      currentUrl: this.$$('#current-url').value,
      pageClassification: this.$$('#page-classification').value,
    };
    this.dispatchEvent(
        new CustomEvent('query-inputs-changed', {detail: queryInputs}));
  }

  /** @private */
  onDisplayInputsChanged_() {
    /** @type {!DisplayInputs} */
    const displayInputs = {
      showIncompleteResults: this.$$('#show-incomplete-results').checked,
      showDetails: this.$$('#show-details').checked,
      showAllProviders: this.$$('#show-all-providers').checked,
    };
    this.dispatchEvent(
        new CustomEvent('display-inputs-changed', {detail: displayInputs}));
  }

  /** @private */
  onFilterInputsChanged_() {
    this.dispatchEvent(new CustomEvent(
        'filter-input-changed', {detail: this.$$('#filter-text').value}));
  }

  /** @private @param {string} format Either 'text' or 'json'. */
  onCopyOutput_(format) {
    this.dispatchEvent(new CustomEvent('copy-request', {detail: format}));
  }

  /** @private @return {number} */
  get cursorPosition_() {
    return this.$$('#lock-cursor-position').checked ?
        this.$$('#input-text').value.length :
        this.$$('#input-text').selectionEnd;
  }

  /** @return {DisplayInputs} */
  static get defaultDisplayInputs() {
    return {
      showIncompleteResults: false,
      showDetails: false,
      showAllProviders: true,
    };
  }
}

customElements.define('omnibox-input', OmniboxInput);
