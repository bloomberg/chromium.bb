// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for omnibox.html, served from chrome://omnibox/
 * This is used to debug omnibox ranking.  The user enters some text
 * into a box, submits it, and then sees lots of debug information
 * from the autocompleter that shows what omnibox would do with that
 * input.
 *
 * The simple object defined in this javascript file listens for
 * certain events on omnibox.html, sends (when appropriate) the
 * input text to C++ code to start the omnibox autcomplete controller
 * working, and listens from callbacks from the C++ code saying that
 * results are available.  When results (possibly intermediate ones)
 * are available, the Javascript formats them and displays them.
 */

(function() {
/**
 * @typedef {{
 *   queryInputs: QueryInputs,
 *   displayInputs: DisplayInputs,
 *   responses: !Array<!mojom.OmniboxResult>,
 * }}
 */
let OmniboxExport;

class BrowserProxy {
  /** @param {!omnibox_output.OmniboxOutput} omniboxOutput */
  constructor(omniboxOutput) {
    /** @private {!mojom.OmniboxPageCallbackRouter} */
    this.callbackRouter_ = new mojom.OmniboxPageCallbackRouter;

    // TODO (manukh) rename method to handleNewAutocompleteResponse in order
    // to keep terminology consistent. Result refers to a single autocomplete
    // match. Response refers to the data returned from the C++
    // AutocompleteController.
    this.callbackRouter_.handleNewAutocompleteResult.addListener(
        omniboxOutput.addAutocompleteResponse.bind(omniboxOutput));
    this.callbackRouter_.handleAnswerImageData.addListener(
        omniboxOutput.updateAnswerImage.bind(omniboxOutput));

    /** @private {!mojom.OmniboxPageHandlerProxy} */
    this.handler_ = mojom.OmniboxPageHandler.getProxy();
    this.handler_.setClientPage(this.callbackRouter_.createProxy());

    /**
     * @type {function(string, boolean, number, boolean, boolean, boolean,
     *     string, number)}
     */
    this.makeRequest = this.handler_.startOmniboxQuery.bind(this.handler_);
  }
}

/** @type {!BrowserProxy} */
let browserProxy;
/** @type {!OmniboxInput} */
let omniboxInput;
/** @type {!omnibox_output.OmniboxOutput} */
let omniboxOutput;
/** @type {!ExportDelegate} */
let exportDelegate;

document.addEventListener('DOMContentLoaded', () => {
  omniboxInput = /** @type {!OmniboxInput} */ ($('omnibox-input'));
  omniboxOutput =
      /** @type {!omnibox_output.OmniboxOutput} */ ($('omnibox-output'));
  browserProxy = new BrowserProxy(omniboxOutput);
  exportDelegate = new ExportDelegate(omniboxOutput, omniboxInput);

  omniboxInput.addEventListener('query-inputs-changed', event => {
    omniboxOutput.clearAutocompleteResponses();
    omniboxOutput.updateQueryInputs(event.detail);
    browserProxy.makeRequest(
        event.detail.inputText, event.detail.resetAutocompleteController,
        event.detail.cursorPosition, event.detail.zeroSuggest,
        event.detail.preventInlineAutocomplete, event.detail.preferKeyword,
        event.detail.currentUrl, event.detail.pageClassification);
  });
  omniboxInput.addEventListener(
      'display-inputs-changed',
      event => omniboxOutput.updateDisplayInputs(event.detail));
  omniboxInput.addEventListener(
      'filter-input-changed',
      event => omniboxOutput.updateFilterText(event.detail));

  omniboxInput.addEventListener(
      'import-json', event => exportDelegate.importJson(event.detail));
  omniboxInput.addEventListener('copy-text', () => exportDelegate.copyText());
  omniboxInput.addEventListener(
      'download-json', () => exportDelegate.downloadJson());
});

class ExportDelegate {
  /**
   * @param {!omnibox_output.OmniboxOutput} omniboxOutput
   * @param {!OmniboxInput} omniboxInput
   */
  constructor(omniboxOutput, omniboxInput) {
    /** @private {!OmniboxInput} */
    this.omniboxInput_ = omniboxInput;
    /** @private {!omnibox_output.OmniboxOutput} */
    this.omniboxOutput_ = omniboxOutput;
  }

  /** @param {OmniboxExport} importData */
  importJson(importData) {
    // This is the minimum validation required to ensure no console errors.
    // Invalid importData that passes validation will be processed with a
    // best-attempt; e.g. if responses are missing 'relevance' values, then
    // those cells will be left blank.
    const valid = importData && importData.queryInputs &&
        importData.displayInputs && Array.isArray(importData.responses) &&
        importData.responses.every(
            response => Array.isArray(response.combinedResults) &&
                Array.isArray(response.resultsByProvider));
    if (!valid) {
      return console.error(
          'invalid import format:',
          'expected {queryInputs: {}, displayInputs: {}, responses: []}');
    }
    this.omniboxInput_.queryInputs = importData.queryInputs;
    this.omniboxInput_.displayInputs = importData.displayInputs;
    this.omniboxOutput_.updateQueryInputs(importData.queryInputs);
    this.omniboxOutput_.updateDisplayInputs(importData.displayInputs);
    this.omniboxOutput_.setAutocompleteResponses(importData.responses);
  }

  copyText() {
    ExportDelegate.copy_(this.omniboxOutput_.visibleTableText);
  }

  downloadJson() {
    /** @type {OmniboxExport} */
    const exportObj = {
      queryInputs: this.omniboxInput_.queryInputs,
      displayInputs: this.omniboxInput_.displayInputs,
      responses: this.omniboxOutput_.responses,
    };
    const fileName = `omnibox_debug_export_${exportObj.queryInputs.inputText}_${
        new Date().toISOString()}.json`;
    ExportDelegate.download_(exportObj, fileName);
  }

  /** @private @param {string} value */
  static copy_(value) {
    navigator.clipboard.writeText(value).catch(
        error => console.error('unable to copy to clipboard:', error));
  }

  /**
   * @private
   * @param {Object} object
   * @param {string} fileName
   */
  static download_(object, fileName) {
    const content = JSON.stringify(object, null, 2);
    const blob = new Blob([content], {type: 'application/json'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = fileName;
    a.click();
  }
}
})();
