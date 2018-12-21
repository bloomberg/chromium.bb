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
  class BrowserProxy {
    constructor() {
      /** @private {!mojom.OmniboxPageCallbackRouter} */
      this.callbackRouter_ = new mojom.OmniboxPageCallbackRouter;

      // TODO (manukh) rename method to handleNewAutocompleteResponse in order
      // to keep terminology consistent. Result refers to a single autocomplete
      // match. Response refers to the data returned from the C++
      // AutocompleteController.
      this.callbackRouter_.handleNewAutocompleteResult.addListener(
          result => omniboxOutput.addAutocompleteResponse(result));

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
  const browserProxy = new BrowserProxy();
  /** @type {!OmniboxInputs} */
  let omniboxInputs;
  /** @type {!omnibox_output.OmniboxOutput} */
  let omniboxOutput;

  document.addEventListener('DOMContentLoaded', () => {
    omniboxInputs = /** @type {!OmniboxInputs} */ ($('omnibox-inputs'));
    omniboxOutput =
        /** @type {!omnibox_output.OmniboxOutput} */ ($('omnibox-output'));

    omniboxInputs.addEventListener('query-inputs-changed', event => {
      omniboxOutput.clearAutocompleteResponses();
      omniboxOutput.updateQueryInputs(event.detail);
      browserProxy.makeRequest(
          event.detail.inputText,
          event.detail.resetAutocompleteController,
          event.detail.cursorPosition,
          event.detail.zeroSuggest,
          event.detail.preventInlineAutocomplete,
          event.detail.preferKeyword,
          event.detail.currentUrl,
          event.detail.pageClassification);
    });
    omniboxInputs.addEventListener(
        'display-inputs-changed',
        event => omniboxOutput.updateDisplayInputs(event.detail));
    omniboxInputs.addEventListener(
        'filter-input-changed',
        event => omniboxOutput.updateFilterText(event.detail));
    omniboxInputs.addEventListener('copy-request', event => {
      event.detail === 'text' ? omniboxOutput.copyDelegate.copyTextOutput() :
                                omniboxOutput.copyDelegate.copyJsonOutput();
    });
  });
})();
