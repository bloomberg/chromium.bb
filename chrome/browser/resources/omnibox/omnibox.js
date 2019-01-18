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
 *   responsesHistory: !Array<!Array<!mojom.OmniboxResponse>>,
 * }}
 */
let OmniboxExport;

/** @type {!BrowserProxy} */
let browserProxy;
/** @type {!OmniboxInput} */
let omniboxInput;
/** @type {!omnibox_output.OmniboxOutput} */
let omniboxOutput;
/** @type {!ExportDelegate} */
let exportDelegate;

class BrowserProxy {
  /** @param {!omnibox_output.OmniboxOutput} omniboxOutput */
  constructor(omniboxOutput) {
    /** @private {!mojom.OmniboxPageCallbackRouter} */
    this.callbackRouter_ = new mojom.OmniboxPageCallbackRouter;

    this.callbackRouter_.handleNewAutocompleteResponse.addListener(
        (response, isPageController) => {
          // When unfocusing the browser omnibox, the autocomplete controller
          // sends a response with no combined results. This response is ignored
          // in order to prevent the previous non-empty response from being
          // hidden and because these results wouldn't normally be displayed by
          // the browser window omnibox.
          if (isPageController ||
              (omniboxInput.connectWindowOmnibox &&
               response.combinedResults.length)) {
            omniboxOutput.addAutocompleteResponse(response);
          }
        });
    this.callbackRouter_.handleNewAutocompleteQuery.addListener(
        isPageController => {
          if (isPageController || omniboxInput.connectWindowOmnibox) {
            omniboxOutput.prepareNewQuery();
          }
        });
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

document.addEventListener('DOMContentLoaded', () => {
  omniboxInput = /** @type {!OmniboxInput} */ ($('omnibox-input'));
  omniboxOutput =
      /** @type {!omnibox_output.OmniboxOutput} */ ($('omnibox-output'));
  browserProxy = new BrowserProxy(omniboxOutput);
  exportDelegate = new ExportDelegate(omniboxOutput, omniboxInput);

  omniboxInput.addEventListener('query-inputs-changed', e => {
    omniboxOutput.updateQueryInputs(e.detail);
    browserProxy.makeRequest(
        e.detail.inputText, e.detail.resetAutocompleteController,
        e.detail.cursorPosition, e.detail.zeroSuggest,
        e.detail.preventInlineAutocomplete, e.detail.preferKeyword,
        e.detail.currentUrl, e.detail.pageClassification);
  });
  omniboxInput.addEventListener(
      'display-inputs-changed',
      e => omniboxOutput.updateDisplayInputs(e.detail));
  omniboxInput.addEventListener(
      'filter-input-changed', e => omniboxOutput.updateFilterText(e.detail));
  omniboxInput.addEventListener(
      'import-json', e => exportDelegate.importJson(e.detail));
  omniboxInput.addEventListener('copy-text', () => exportDelegate.copyText());
  omniboxInput.addEventListener(
      'download-json', () => exportDelegate.downloadJson());
  omniboxInput.addEventListener(
      'response-select',
      e => omniboxOutput.updateSelectedResponseIndex(e.detail));

  omniboxOutput.addEventListener(
      'responses-count-changed', e => omniboxInput.responsesCount = e.detail);
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
    if (!validateImportData_(importData)) {
      return;
    }
    this.omniboxInput_.queryInputs = importData.queryInputs;
    this.omniboxInput_.displayInputs = importData.displayInputs;
    this.omniboxOutput_.updateQueryInputs(importData.queryInputs);
    this.omniboxOutput_.updateDisplayInputs(importData.displayInputs);
    this.omniboxOutput_.setResponsesHistory(importData.responsesHistory);
  }

  copyText() {
    ExportDelegate.copy_(this.omniboxOutput_.visibleTableText);
  }

  downloadJson() {
    /** @type {OmniboxExport} */
    const exportObj = {
      queryInputs: this.omniboxInput_.queryInputs,
      displayInputs: this.omniboxInput_.displayInputs,
      responsesHistory: this.omniboxOutput_.responsesHistory,
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

/**
 * This is the minimum validation required to ensure no console errors.
 * Invalid importData that passes validation will be processed with a
 * best-attempt; e.g. if responses are missing 'relevance' values, then those
 * cells will be left blank.
 * @private
 * @param {OmniboxExport} importData
 * @return {boolean}
 */
function validateImportData_(importData) {
  const EXPECTED_FORMAT = {
    queryInputs: {},
    displayInputs: {},
    responsesHistory: [[{combinedResults: [], resultsByProvider: []}]]
  };
  const INVALID_MESSAGE = `Invalid import format; expected \n${
      JSON.stringify(EXPECTED_FORMAT, null, 2)};\n`;

  if (!importData) {
    console.error(INVALID_MESSAGE + 'received non object.');
    return false;
  }

  if (!importData.queryInputs || !importData.displayInputs) {
    console.error(
        INVALID_MESSAGE +
        'import missing objects queryInputs and displayInputs.');
    return false;
  }

  if (!Array.isArray(importData.responsesHistory)) {
    console.error(INVALID_MESSAGE + 'import missing array responsesHistory.');
    return false;
  }

  if (!importData.responsesHistory.every(Array.isArray)) {
    console.error(INVALID_MESSAGE + 'responsesHistory contains non arrays.');
    return false;
  }

  if (!importData.responsesHistory.every(
          responses => responses.every(
              ({combinedResults, resultsByProvider}) =>
                  Array.isArray(combinedResults) &&
                  Array.isArray(resultsByProvider)))) {
    console.error(
        INVALID_MESSAGE +
        'responsesHistory items\' items missing combinedResults and ' +
        'resultsByProvider arrays.');
    return false;
  }

  return true;
}
})();
