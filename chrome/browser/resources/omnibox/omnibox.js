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
  // Variables to DOM elements to avoid multiple document.getElement calls.
  let omniboxDebugText;
  let showIncompleteResults;
  let showDetails;
  let showAllProviders;

  /**
   * @type {number} the value for cursor position we sent with the most
   *     recent request.  We need to remember this in order to display it
   *     in the output; otherwise it's hard or impossible to determine
   *     from screen captures or print-to-PDFs.
   */
  let cursorPositionUsed = -1;

  /**
   * Register our event handlers.
   */
  function initializeEventHandlers() {
    omniboxDebugText = $('omnibox-debug-text');
    showIncompleteResults = $('show-incomplete-results');
    showDetails = $('show-details');
    showAllProviders = $('show-all-providers');

    let startOmniboxQuery = () => browserProxy.makeRequest();

    $('input-text').addEventListener('input', startOmniboxQuery);
    $('prevent-inline-autocomplete')
        .addEventListener('change', startOmniboxQuery);
    $('prefer-keyword').addEventListener('change', startOmniboxQuery);
    $('page-classification').addEventListener('change', startOmniboxQuery);
    showIncompleteResults.addEventListener('change', refreshAllResults);
    showDetails.addEventListener('change', refreshAllResults);
    showAllProviders.addEventListener('change', refreshAllResults);
  }

  /**
   * Returns a simple object with information about how to display an
   * autocomplete result data field.
   */
  class PresentationInfoRecord {
    /**
     * @param {string} header the label for the top of the column/table.
     * @param {string} url the URL that the header should point
     *     to (if non-empty).
     * @param {string} propertyName the name of the property in the autocomplete
     *     result record that we lookup.
     * @param {boolean} displayAlways whether the property should be displayed
     *     regardless of whether we're in detailed mode.
     * @param {string} tooltip a description of the property that will be
     *     presented as a tooltip when the mouse is hovered over the column title.
     */
    constructor(header, url, propertyName, displayAlways, tooltip) {
      this.header = header;
      this.urlLabelForHeader = url;
      this.propertyName = propertyName;
      this.displayAlways = displayAlways;
      this.tooltip = tooltip;
    }
  }

  /**
   * A constant that's used to decide what autocomplete result
   * properties to output in what order.  This is an array of
   * PresentationInfoRecord() objects; for details see that
   * function.
   * @type {!Array<!PresentationInfoRecord>}
   */
  const PROPERTY_OUTPUT_ORDER = [
    new PresentationInfoRecord(
        'Provider', '', 'providerName', true,
        'The AutocompleteProvider suggesting this result.'),
    new PresentationInfoRecord(
        'Type', '', 'type', true, 'The type of the result.'),
    new PresentationInfoRecord(
        'Relevance', '', 'relevance', true,
        'The result score. Higher is more relevant.'),
    new PresentationInfoRecord(
        'Contents', '', 'contents', true,
        'The text that is presented identifying the result.'),
    new PresentationInfoRecord(
        'Can Be Default', '', 'allowedToBeDefaultMatch', false,
        'A green checkmark indicates that the result can be the default ' +
            'match (i.e., can be the match that pressing enter in the ' +
            'omnibox navigates to).'),
    new PresentationInfoRecord(
        'Starred', '', 'starred', false,
        'A green checkmark indicates that the result has been bookmarked.'),
    new PresentationInfoRecord(
        'Has tab match', '', 'hasTabMatch', false,
        'A green checkmark indicates that the result URL matches an open tab.'),
    new PresentationInfoRecord(
        'Description', '', 'description', false,
        'The page title of the result.'),
    new PresentationInfoRecord(
        'URL', '', 'destinationUrl', true, 'The URL for the result.'),
    new PresentationInfoRecord(
        'Fill Into Edit', '', 'fillIntoEdit', false,
        'The text shown in the omnibox when the result is selected.'),
    new PresentationInfoRecord(
        'Inline Autocompletion', '', 'inlineAutocompletion', false,
        'The text shown in the omnibox as a blue highlight selection ' +
            'following the cursor, if this match is shown inline.'),
    new PresentationInfoRecord(
        'Del', '', 'deletable', false,
        'A green checkmark indicates that the result can be deleted from ' +
            'the visit history.'),
    new PresentationInfoRecord('Prev', '', 'fromPrevious', false, ''),
    new PresentationInfoRecord(
        'Tran',
        'https://cs.chromium.org/chromium/src/ui/base/page_transition_types.h' +
            '?q=page_transition_types.h&sq=package:chromium&dr=CSs&l=14',
        'transition', false, 'How the user got to the result.'),
    new PresentationInfoRecord(
        'Done', '', 'providerDone', false,
        'A green checkmark indicates that the provider is done looking for ' +
            'more results.'),
    new PresentationInfoRecord(
        'Associated Keyword', '', 'associatedKeyword', false,
        'If non-empty, a "press tab to search" hint will be shown and will ' +
            'engage this keyword.'),
    new PresentationInfoRecord(
        'Keyword', '', 'keyword', false,
        'The keyword of the search engine to be used.'),
    new PresentationInfoRecord(
        'Duplicates', '', 'duplicates', false,
        'The number of matches that have been marked as duplicates of this ' +
            'match.'),
    new PresentationInfoRecord(
        'Additional Info', '', 'additionalInfo', false,
        'Provider-specific information about the result.'),
  ];

  /**
   * Appends some human-readable information about the provided
   * autocomplete result to the HTML node with id omnibox-debug-text.
   * The current human-readable form is a few lines about general
   * autocomplete result statistics followed by a table with one line
   * for each autocomplete match.  The input parameter is an OmniboxResultMojo.
   */
  function addResultToOutput(result) {
    // Output the result-level features in detailed mode and in
    // show incomplete results mode.  We do the latter because without
    // these result-level features, one can't make sense of each
    // batch of results.
    if (showDetails.checked || showIncompleteResults.checked) {
      addParagraph(omniboxDebugText, `cursor position = ${cursorPositionUsed}`);
      addParagraph(omniboxDebugText, `inferred input type = ${result.type}`);
      addParagraph(
          omniboxDebugText, `elapsed time = ${result.timeSinceOmniboxStartedMs}ms`);
      addParagraph(omniboxDebugText, `all providers done = ${result.done}`);
      let p = document.createElement('p');
      p.textContent = `host = ${result.host}`;
      // The field isn't actually optional in the mojo object; instead it assumes
      // failed lookups are not typed hosts.  Fix this to make it optional.
      // http://crbug.com/863201
      if ('isTypedHost' in result) {
        // Only output the isTypedHost information if available.  (It may
        // be missing if the history database lookup failed.)
        p.textContent =
            p.textContent + ` has isTypedHost = ${result.isTypedHost}`;
      }
      omniboxDebugText.appendChild(p);
    }

    // Combined results go after the lines below.
    let group = document.createElement('a');
    group.className = 'group-separator';
    group.textContent = 'Combined results.';
    omniboxDebugText.appendChild(group);

    // Add combined/merged result table.
    let p = document.createElement('p');
    p.appendChild(addResultTableToOutput(result.combinedResults));
    omniboxDebugText.appendChild(p);

    // Move forward only if you want to display per provider results.
    if (!showAllProviders.checked) {
      return;
    }

    // Individual results go after the lines below.
    group = document.createElement('a');
    group.className = 'group-separator';
    group.textContent = 'Results for individual providers.';
    omniboxDebugText.appendChild(group);

    // Add the per-provider result tables with labels. We do not append the
    // combined/merged result table since we already have the per provider
    // results.
    result.resultsByProvider.forEach(providerResults => {
      // If we have no results we do not display anything.
      if (providerResults.results.length === 0)
        return;
      let p = document.createElement('p');
      p.appendChild(addResultTableToOutput(providerResults.results));
      omniboxDebugText.appendChild(p);
    });
  }

  /**
   * @param {Object} result an array of AutocompleteMatchMojos.
   * @return {Element} that is a user-readable HTML
   *     representation of this object.
   */
  function addResultTableToOutput(result) {
    let inDetailedMode = $('show-details').checked;
    // Create a table to hold all the autocomplete items.
    let table = document.createElement('table');
    table.className = 'autocomplete-results-table';
    table.appendChild(createAutocompleteResultTableHeader());
    // Loop over every autocomplete item and add it as a row in the table.
    result.forEach(autocompleteSuggestion => {
      let row = document.createElement('tr');
      // Loop over all the columns/properties and output either them
      // all (if we're in detailed mode) or only the ones marked displayAlways.
      // Keep track of which properties we displayed.
      let displayedProperties = {};
      PROPERTY_OUTPUT_ORDER.forEach(property => {
        if (inDetailedMode || property.displayAlways) {
          row.appendChild(createCellForPropertyAndRemoveProperty(
              autocompleteSuggestion, property.propertyName));
          displayedProperties[property.propertyName] = true;
        }
      });

      // Now, if we're in detailed mode, add all the properties that
      // haven't already been output.  (We know which properties have
      // already been output because we delete the property when we output
      // it.  The only way we have properties left at this point if
      // we're in detailed mode and we're getting back properties
      // not listed in PROPERTY_OUTPUT_ORDER.  Perhaps someone added
      // something to the C++ code but didn't bother to update this
      // Javascript?  In any case, we want to display them.)
      if (inDetailedMode) {
        for (let key in autocompleteSuggestion) {
          if (!displayedProperties[key] &&
              typeof autocompleteSuggestion[key] !== 'function') {
            let cell = document.createElement('td');
            cell.textContent = key + '=' + autocompleteSuggestion[key];
            row.appendChild(cell);
          }
        }
      }

      table.appendChild(row);
    });
    return table;
  }

  /**
   * Returns an HTML Element of type table row that contains the
   * headers we'll use for labeling the columns.  If we're in
   * detailedMode, we use all the headers.  If not, we only use ones
   * marked displayAlways.
   */
  function createAutocompleteResultTableHeader() {
    let row = document.createElement('tr');
    let inDetailedMode = $('show-details').checked;
    PROPERTY_OUTPUT_ORDER.forEach(property => {
      if (inDetailedMode || property.displayAlways) {
        let headerCell = document.createElement('th');
        if (property.urlLabelForHeader !== '') {
          // Wrap header text in URL.
          let linkNode = document.createElement('a');
          linkNode.href = property.urlLabelForHeader;
          linkNode.textContent = property.header;
          headerCell.appendChild(linkNode);
        } else {
          // Output header text without a URL.
          headerCell.textContent = property.header;
          headerCell.className = 'table-header';
          headerCell.title = property.tooltip;
        }
        row.appendChild(headerCell);
      }
    });
    return row;
  }

  /**
   * @param {!Object} autocompleteSuggestion the particular
   *     autocomplete suggestion we're in the process of displaying.
   * @param {string} propertyName the particular property of the autocomplete
   *     suggestion that should go in this cell.
   * @return {Element} that contains the value within this
   *     autocompleteSuggestion associated with propertyName.
   */
  function createCellForPropertyAndRemoveProperty(autocompleteSuggestion, propertyName) {
    let cell = document.createElement('td');
    if (propertyName in autocompleteSuggestion) {
      if (propertyName === 'additionalInfo') {
        // |additionalInfo| embeds a two-column table of provider-specific data
        // within this cell. |additionalInfo| is an array of
        // AutocompleteAdditionalInfo.
        let additionalInfoTable = document.createElement('table');
        autocompleteSuggestion[propertyName].forEach(additionalInfo => {
          let additionalInfoRow = document.createElement('tr');

          // Set the title (name of property) cell text.
          let propertyCell = document.createElement('td');
          propertyCell.textContent = additionalInfo.key + ':';
          propertyCell.className = 'additional-info-property';
          additionalInfoRow.appendChild(propertyCell);

          // Set the value of the property cell text.
          let valueCell = document.createElement('td');
          valueCell.textContent = additionalInfo.value;
          valueCell.className = 'additional-info-value';
          additionalInfoRow.appendChild(valueCell);

          additionalInfoTable.appendChild(additionalInfoRow);
        });
        cell.appendChild(additionalInfoTable);
      } else if (typeof autocompleteSuggestion[propertyName] === 'boolean') {
        // If this is a boolean, display a checkmark or an X instead of
        // the strings true or false.
        if (autocompleteSuggestion[propertyName]) {
          cell.className = 'check-mark';
          cell.textContent = 'true';
        } else {
          cell.className = 'x-mark';
          cell.textContent = 'false';
        }
      } else {
        let text = String(autocompleteSuggestion[propertyName]);
        // If it's a URL wrap it in an href.
        let re = /^(http|https|ftp|chrome|file):\/\//;
        if (re.test(text)) {
          let aCell = document.createElement('a');
          aCell.textContent = text;
          aCell.href = text;
          cell.appendChild(aCell);
        } else {
          // All other data types (integer, strings, etc.) display their
          // normal toString() output.
          cell.textContent = autocompleteSuggestion[propertyName];
        }
      }
    }  // else: if propertyName is undefined, we leave the cell blank
    return cell;
  }

  /**
   * Appends a paragraph node containing text to the parent node.
   */
  function addParagraph(parent, text) {
    let p = document.createElement('p');
    p.textContent = text;
    parent.appendChild(p);
  }

  /* Repaints the page based on the contents of the array
   * progressiveAutocompleteResults, which represents consecutive
   * autocomplete results.  We only display the last (most recent)
   * entry unless we're asked to display incomplete results.  For an
   * example of the output, play with chrome://omnibox/
   */
  function refreshAllResults() {
    clearOutput();
    if (showIncompleteResults.checked)
      browserProxy.progressiveAutocompleteResults.forEach(addResultToOutput);
    else if (browserProxy.progressiveAutocompleteResults.length)
      addResultToOutput(
          browserProxy.progressiveAutocompleteResults
              [browserProxy.progressiveAutocompleteResults.length - 1]);
  }

  /*
   * Adds the last result, based on the contents of the array
   * progressiveAutocompleteResults, to the page. If we're not displaying
   * incomplete results, we clear the page and add the last result, similar to
   * refreshAllResults. If we are displaying incomplete results, then this is
   * more efficient than refreshAllResults, as there's no need to clear and
   * re-add previous results.
   */
  function refreshNewResult() {
    if (!showIncompleteResults.checked)
      clearOutput();
    addResultToOutput(
        browserProxy.progressiveAutocompleteResults
            [browserProxy.progressiveAutocompleteResults.length - 1]);
  }

  /*
    Removes all results from the page.
   */
  function clearOutput() {
    while (omniboxDebugText.firstChild)
      omniboxDebugText.removeChild(omniboxDebugText.firstChild);
  }

  class BrowserProxy {
    constructor() {
      /** @private {!mojom.OmniboxPageHandlerPtr} */
      this.pagehandlePtr_ = new mojom.OmniboxPageHandlerPtr;
      Mojo.bindInterface(
          mojom.OmniboxPageHandler.name,
          mojo.makeRequest(this.pagehandlePtr_).handle);
      let client = new mojom.OmniboxPagePtr;
      // NOTE: Need to keep a global reference to the |binding_| such that it is
      // not garbage collected, which causes the pipe to close and future calls
      // from C++ to JS to get dropped.
      /** @private {!mojo.Binding} */
      this.binding_ =
          new mojo.Binding(mojom.OmniboxPage, this, mojo.makeRequest(client));
      this.pagehandlePtr_.setClientPage(client);

      /**
       * @type {!Array<mojom.OmniboxResult>} an array of all autocomplete
       *     results we've seen for this query.  We append to this list once for
       *     every call to handleNewAutocompleteResult.  See omnibox.mojom for
       *     details..
       */
      this.progressiveAutocompleteResults = [];
    }

    /**
     * Extracts the input text from the text field and sends it to the
     * C++ portion of chrome to handle.  The C++ code will iteratively
     * call handleNewAutocompleteResult as results come in.
     */
    makeRequest() {
      clearOutput();
      this.progressiveAutocompleteResults = [];
      // Then, call chrome with a five-element list:
      // - first element: the value in the text box
      // - second element: the location of the cursor in the text box
      // - third element: the value of prevent-inline-autocomplete
      // - forth element: the value of prefer-keyword
      // - fifth element: the value of page-classification
      let inputTextElement = $('input-text');
      cursorPositionUsed = inputTextElement.selectionEnd;
      this.pagehandlePtr_.startOmniboxQuery(
          inputTextElement.value, cursorPositionUsed,
          $('prevent-inline-autocomplete').checked, $('prefer-keyword').checked,
          parseInt($('page-classification').value, 10));
    }

    handleNewAutocompleteResult(result) {
      this.progressiveAutocompleteResults.push(result);
      refreshNewResult();
    }
  }

  let browserProxy;

  document.addEventListener('DOMContentLoaded', () => {
    browserProxy = new BrowserProxy();
    initializeEventHandlers();
  });
})();
