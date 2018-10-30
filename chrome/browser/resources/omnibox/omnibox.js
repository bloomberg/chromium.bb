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

(function () {
  /**
   * @type {number} the value for cursor position we sent with the most
   *     recent request.  We need to remember this in order to display it
   *     in the output; otherwise it's hard or impossible to determine
   *     from screen captures or print-to-PDFs.
   */
  let cursorPosition = -1;

  /**
   * Tracks and aggregates responses from the C++ autocomplete controller.
   * Typically, the C++ controller returns 3 sets of results per query, unless
   * a new query is submitted before all 3 responses. OutputController also
   * triggers appending to and clearing of OmniboxOutput when appropriate (e.g.,
   * upon receiving a new response or a change in display inputs).
   */
  class OutputController {
    constructor() {
      /** @private {!Array<!mojom.OmniboxResult>} */
      this.responses_ = [];
      /** @private {QueryInputs} */
      this.queryInputs_ = /** @type {QueryInputs} */ ({});
      /** @private {DisplayInputs} */
      this.displayInputs_ = /** @type {DisplayInputs} */ ({});
    }

    /** @param {QueryInputs} queryInputs */
    updateQueryInputs(queryInputs) {
      this.queryInputs_ = queryInputs;
      this.refresh_();
    }

    /** @param {DisplayInputs} displayInputs */
    updateDisplayInputs(displayInputs) {
      this.displayInputs_ = displayInputs;
      this.refresh_();
    }

    clearAutocompleteResponses() {
      this.responses_ = [];
      this.refresh_();
    }

    /** @param {!mojom.OmniboxResult} response */
    addAutocompleteResponse(response) {
      this.responses_.push(response);
      this.refresh_();
    }

    /** @private */
    refresh_() {
      omniboxOutput.refresh(
          this.queryInputs_, this.responses_, this.displayInputs_);
    }
  }

  class BrowserProxy {
    constructor() {
      /** @private {!mojom.OmniboxPageHandlerPtr} */
      this.pagehandlePtr_ = new mojom.OmniboxPageHandlerPtr;
      Mojo.bindInterface(
          mojom.OmniboxPageHandler.name,
          mojo.makeRequest(this.pagehandlePtr_).handle);
      const client = new mojom.OmniboxPagePtr;
      // NOTE: Need to keep a global reference to the |binding_| such that it is
      // not garbage collected, which causes the pipe to close and future calls
      // from C++ to JS to get dropped.
      /** @private {!mojo.Binding} */
      this.binding_ =
          new mojo.Binding(mojom.OmniboxPage, this, mojo.makeRequest(client));
      this.pagehandlePtr_.setClientPage(client);
    }

    /**
     * Extracts the input text from the text field and sends it to the
     * C++ portion of chrome to handle.  The C++ code will iteratively
     * call handleNewAutocompleteResult as results come in.
     */
    makeRequest(inputString,
                cursorPosition,
                preventInlineAutocomplete,
                preferKeyword,
                pageClassification) {
      // Then, call chrome with a five-element list:
      // - first element: the value in the text box
      // - second element: the location of the cursor in the text box
      // - third element: the value of prevent-inline-autocomplete
      // - forth element: the value of prefer-keyword
      // - fifth element: the value of page-classification
      this.pagehandlePtr_.startOmniboxQuery(
          inputString,
          cursorPosition,
          preventInlineAutocomplete,
          preferKeyword,
          pageClassification);
    }

    // TODO (manukh) rename method to handleNewAutocompleteResponse in order to
    // keep terminology consistent. Result refers to a single autocomplete
    // match. Response refers to the data returned from the C++
    // AutocompleteController.
    handleNewAutocompleteResult(response) {
      outputController.addAutocompleteResponse(response);
    }
  }

  /** @type {BrowserProxy} */
  const browserProxy = new BrowserProxy();
  /** @type {OmniboxInputs} */
  let omniboxInputs;
  /** @type {omnibox_output.OmniboxOutput} */
  let omniboxOutput;
  /** @type {OutputController} */
  const outputController = new OutputController();

  document.addEventListener('DOMContentLoaded', () => {
    omniboxInputs = /** @type {!OmniboxInputs} */ ($('omnibox-inputs'));
    omniboxOutput =
        /** @type {!omnibox_output.OmniboxOutput} */ ($('omnibox-output'));
    omniboxInputs.addEventListener('query-inputs-changed', event => {
      outputController.clearAutocompleteResponses();
      outputController.updateQueryInputs(event.detail);
      browserProxy.makeRequest(
          event.detail.inputText,
          event.detail.cursorPosition,
          event.detail.preventInlineAutocomplete,
          event.detail.preferKeyword,
          event.detail.pageClassification);
    });
    omniboxInputs.addEventListener(
        'display-inputs-changed',
        event => outputController.updateDisplayInputs(event.detail));
  });
})();
