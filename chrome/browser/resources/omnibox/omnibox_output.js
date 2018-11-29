// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('omnibox_output', function() {
  /**
   * Details how to display an autocomplete result data field.
   * @typedef {{
   *   header: string,
   *   url: string,
   *   propertyName: string,
   *   displayAlways: boolean,
   *   tooltip: string,
   * }}
   */
  let PresentationInfoRecord;

  /**
   * A constant that's used to decide what autocomplete result
   * properties to output in what order.
   * @type {!Array<!PresentationInfoRecord>}
   */
  const PROPERTY_OUTPUT_ORDER = [
    {
      header: 'Provider',
      url: '',
      propertyName: 'providerName',
      displayAlways: true,
      tooltip: 'The AutocompleteProvider suggesting this result.'
    },
    {
      header: 'Type',
      url: '',
      propertyName: 'type',
      displayAlways: true,
      tooltip: 'The type of the result.'
    },
    {
      header: 'Relevance',
      url: '',
      propertyName: 'relevance',
      displayAlways: true,
      tooltip: 'The result score. Higher is more relevant.'
    },
    {
      header: 'Contents',
      url: '',
      propertyName: 'contents',
      displayAlways: true,
      tooltip: 'The text that is presented identifying the result.'
    },
    {
      header: 'Description',
      url: '',
      propertyName: 'description',
      displayAlways: false,
      tooltip: 'The page title of the result.'
    },
    {
      header: 'CanBeDefault',
      url: '',
      propertyName: 'allowedToBeDefaultMatch',
      displayAlways: false,
      tooltip:
          'A green checkmark indicates that the result can be the default ' +
          'match(i.e., can be the match that pressing enter in the omnibox' +
          'navigates to).'
    },
    {
      header: 'Starred',
      url: '',
      propertyName: 'starred',
      displayAlways: false,
      tooltip:
          'A green checkmark indicates that the result has been bookmarked.'
    },
    {
      header: 'Hastabmatch',
      url: '',
      propertyName: 'hasTabMatch',
      displayAlways: false,
      tooltip:
          'A green checkmark indicates that the result URL matches an open' +
          'tab.'
    },
    {
      header: 'URL',
      url: '',
      propertyName: 'destinationUrl',
      displayAlways: true,
      tooltip: 'The URL for the result.'
    },
    {
      header: 'FillIntoEdit',
      url: '',
      propertyName: 'fillIntoEdit',
      displayAlways: false,
      tooltip: 'The text shown in the omnibox when the result is selected.'
    },
    {
      header: 'InlineAutocompletion',
      url: '',
      propertyName: 'inlineAutocompletion',
      displayAlways: false,
      tooltip: 'The text shown in the omnibox as a blue highlight selection ' +
          'following the cursor, if this match is shown inline.'
    },
    {
      header: 'Del',
      url: '',
      propertyName: 'deletable',
      displayAlways: false,
      tooltip:
          'A green checkmark indicates that the result can be deleted from ' +
          'the visit history.'
    },
    {
      header: 'Prev',
      url: '',
      propertyName: 'fromPrevious',
      displayAlways: false,
      tooltip: ''
    },
    {
      header: 'Tran',
      url:
          'https://cs.chromium.org/chromium/src/ui/base/page_transition_types.h?q=page_transition_types.h&sq=package:chromium&dr=CSs&l=14',
      propertyName: 'transition',
      displayAlways: false,
      tooltip: 'How the user got to the result.'
    },
    {
      header: 'Done',
      url: '',
      propertyName: 'providerDone',
      displayAlways: false,
      tooltip:
          'A green checkmark indicates that the provider is done looking ' +
          'for more results.'
    },
    {
      header: 'AssociatedKeyword',
      url: '',
      propertyName: 'associatedKeyword',
      displayAlways: false,
      tooltip: 'If non-empty, a "press tab to search" hint will be shown and ' +
          'will engage this keyword.'
    },
    {
      header: 'Keyword',
      url: '',
      propertyName: 'keyword',
      displayAlways: false,
      tooltip: 'The keyword of the search engine to be used.'
    },
    {
      header: 'Duplicates',
      url: '',
      propertyName: 'duplicates',
      displayAlways: false,
      tooltip: 'The number of matches that have been marked as duplicates of ' +
          'this match..'
    },
    {
      header: 'AdditionalInfo',
      url: '',
      propertyName: 'additionalInfo',
      displayAlways: false,
      tooltip: 'Provider-specific information about the result.'
    }
  ];

  /**
   * In addition to representing the rendered HTML element, OmniboxOutput also
   * provides a single public interface to interact with the output:
   * 1. Render tables from responses  (RenderDelegate)
   * 2. Control visibility based on display options (TODO)
   * 3. Control visibility and coloring based on search text (FilterDelegate)
   * 4. Export and copy output (CopyDelegate)
   * 5. Preserve inputs and reset inputs to default (TODO)
   * 6. Export and import inputs (TODO)
   * With regards to interacting with RenderDelegate, OmniboxOutput tracks and
   * aggregates responses from the C++ autocomplete controller. Typically, the
   * C++ controller returns 3 sets of results per query, unless a new query is
   * submitted before all 3 responses. OmniboxController also triggers
   * appending to and clearing of OmniboxOutput when appropriate (e.g., upon
   * receiving a new response or a change in display inputs).
   */
  class OmniboxOutput extends OmniboxElement {
    /** @return {string} */
    static get is() {
      return 'omnibox-output';
    }

    constructor() {
      super('omnibox-output-template');

      /** @type {!RenderDelegate} */
      this.renderDelegate = new RenderDelegate(this.$$('contents'));
      /** @type {!CopyDelegate} */
      this.copyDelegate = new CopyDelegate(this);
      /** @type {!FilterDelegate} */
      this.filterDelegate = new FilterDelegate(this);

      /** @type {!Array<!mojom.OmniboxResult>} */
      this.responses = [];
      /** @private {!QueryInputs} */
      this.queryInputs_ = /** @type {!QueryInputs} */ ({});
      /** @private {!DisplayInputs} */
      this.displayInputs_ = /** @type {!DisplayInputs} */ ({});
    }

    /** @param {!QueryInputs} queryInputs */
    updateQueryInputs(queryInputs) {
      this.queryInputs_ = queryInputs;
      this.refresh_();
    }

    /** @param {!DisplayInputs} displayInputs */
    updateDisplayInputs(displayInputs) {
      this.displayInputs_ = displayInputs;
      this.refresh_();
    }

    clearAutocompleteResponses() {
      this.responses = [];
      this.refresh_();
    }

    /** @param {!mojom.OmniboxResult} response */
    addAutocompleteResponse(response) {
      this.responses.push(response);
      this.refresh_();
    }

    /** @private */
    refresh_() {
      this.renderDelegate.refresh(
          this.queryInputs_, this.responses, this.displayInputs_);
    }

    /** @return {!Array<!OutputMatch>} */
    get matches() {
      return this.renderDelegate.matches;
    }
  }

  // Responsible for rendering the output HTML.
  class RenderDelegate {
    /** @param {!Element} containerElement */
    constructor(containerElement) {
      /** @private {!Element} */
      this.containerElement_ = containerElement;
    }

    /**
     * @param {!QueryInputs} queryInputs
     * @param {!Array<!mojom.OmniboxResult>} responses
     * @param {!DisplayInputs} displayInputs
     */
    refresh(queryInputs, responses, displayInputs) {
      if (!responses.length)
        return;

      /** @private {!Array<OutputResultsGroup>} */
      this.resultsGroup_;

      if (displayInputs.showIncompleteResults) {
        this.resultsGroup_ = responses.map(
            response =>
                new OutputResultsGroup(response, queryInputs.cursorPosition));
      } else {
        const lastResponse = responses[responses.length - 1];
        this.resultsGroup_ =
            [new OutputResultsGroup(lastResponse, queryInputs.cursorPosition)];
      }

      this.clearOutput_();
      this.resultsGroup_.forEach(
          resultsGroup =>
              this.containerElement_.appendChild(resultsGroup.render(
                  displayInputs.showDetails,
                  displayInputs.showIncompleteResults,
                  displayInputs.showAllProviders)));
    }

    /** @private */
    clearOutput_() {
      const contents = this.containerElement_;
      // Clears all children.
      while (contents.firstChild)
        contents.removeChild(contents.firstChild);
    }

    /** @return {string} */
    get visibletableText() {
      return this.containerElement_.innerText;
    }

    /** @return {!Array<!OutputMatch>} */
    get matches() {
      return this.resultsGroup_.flatMap(resultsGroup => resultsGroup.matches);
    }
  }

  /**
   * Helps track and render a results group. C++ Autocomplete typically returns
   * 3 result groups per query. It may return less if the next query is
   * submitted before all 3 have been returned. Each result group contains
   * top level information (e.g., how long the result took to generate), as well
   * as a single list of combined results and multiple lists of individual
   * results. Each of these lists is tracked and rendered by OutputResultsTable
   * below.
   */
  class OutputResultsGroup {
    /**
     * @param {!mojom.OmniboxResult} resultsGroup
     * @param {number} cursorPosition
     */
    constructor(resultsGroup, cursorPosition) {
      /** @struct */
      this.details = {
        cursorPosition,
        time: resultsGroup.timeSinceOmniboxStartedMs,
        done: resultsGroup.done,
        host: resultsGroup.host,
        isTypedHost: resultsGroup.isTypedHost
      };
      /** @type {!OutputResultsTable} */
      this.combinedResults =
          new OutputResultsTable(resultsGroup.combinedResults);
      /** @type {!Array<!OutputResultsTable>} */
      this.individualResultsList =
          resultsGroup.resultsByProvider
              .map(resultsWrapper => resultsWrapper.results)
              .filter(results => results.length > 0)
              .map(results => new OutputResultsTable(results));
    }

    /**
     * Creates a HTML Node representing this data.
     * @param {boolean} showDetails
     * @param {boolean} showIncompleteResults
     * @param {boolean} showAllProviders
     * @return {!Element}
     */
    render(showDetails, showIncompleteResults, showAllProviders) {
      const detailsAndTable =
          OmniboxElement.getTemplate('details-and-table-template');
      if (showDetails || showIncompleteResults) {
        detailsAndTable.querySelector('.details')
            .appendChild(this.renderDetails_());
      }

      const showAdditionalPropertiesColumn =
          this.showAdditionalPropertiesColumn_(showDetails);

      detailsAndTable.querySelector('.table').appendChild(
          OutputResultsTable.renderHeader(
              showDetails, showAdditionalPropertiesColumn));
      detailsAndTable.querySelector('.table').appendChild(
          this.combinedResults.render(showDetails));
      if (showAllProviders) {
        this.individualResultsList.forEach(individualResults => {
          detailsAndTable.querySelector('.table').appendChild(
              individualResults.renderInnerHeader(
                  showDetails, showAdditionalPropertiesColumn));
          detailsAndTable.querySelector('.table').appendChild(
              individualResults.render(showDetails));
        });
      }
      return detailsAndTable;
    }

    /**
     * @private
     * @return {!Element}
     */
    renderDetails_() {
      const details = OmniboxElement.getTemplate('details-template');
      details.querySelector('.cursor-position').textContent =
          this.details.cursorPosition;
      details.querySelector('.time').textContent = this.details.time;
      details.querySelector('.done').textContent = this.details.done;
      details.querySelector('.host').textContent = this.details.host;
      details.querySelector('.is-typed-host').textContent =
          this.details.isTypedHost;
      return details;
    }

    /**
     * @private
     * @param {boolean} showDetails
     * @return {boolean}
     */
    showAdditionalPropertiesColumn_(showDetails) {
      return showDetails &&
          (this.combinedResults.hasAdditionalProperties ||
           this.individualResultsList.some(
               results => results.hasAdditionalProperties));
    }

    /** @return {!Array<!OutputMatch>} */
    get matches() {
      return [this.combinedResults]
          .concat(this.individualResultsList)
          .flatMap(results => results.matches);
    }
  }

  /**
   * Helps track and render a list of results. Each result is tracked and
   * rendered by OutputMatch below.
   */
  class OutputResultsTable {
    /** @param {!Array<!mojom.AutocompleteMatch>} results */
    constructor(results) {
      /** @type {!Array<!OutputMatch>} */
      this.matches = results.map(match => new OutputMatch(match));
    }

    /**
     * @param {boolean} showDetails
     * @param {boolean} showAdditionalPropertiesColumn
     * @return {Element}
     */
    static renderHeader(showDetails, showAdditionalPropertiesColumn) {
      const head = document.createElement('thead');
      const row = document.createElement('tr');
      const cells =
          OutputMatch.displayedProperties(showDetails)
              .map(
                  ({header, url, tooltip}) =>
                      OutputMatch.renderHeaderCell(header, url, tooltip));
      if (showAdditionalPropertiesColumn)
        cells.push(OutputMatch.renderHeaderCell('Additional Properties'));
      cells.forEach(cell => row.appendChild(cell));
      head.appendChild(row);
      return head;
    }

    /**
     * Creates a HTML Node representing this data.
     * @param {boolean} showDetails
     * @return {!Element}
     */
    render(showDetails) {
      const body = document.createElement('tbody');
      this.matches.forEach(
          match => body.appendChild(match.render(showDetails)));
      return body;
    }

    /**
     * @param {boolean} showDetails
     * @param {boolean} showAdditionalPropertiesColumn
     * @return {!Element}
     */
    renderInnerHeader(showDetails, showAdditionalPropertiesColumn) {
      const head = document.createElement('thead');
      const row = document.createElement('tr');
      const cell = document.createElement('th');
      // Reserve 1 more column if showing the additional properties column.
      cell.colSpan = OutputMatch.displayedProperties(showDetails).length +
          showAdditionalPropertiesColumn;
      cell.textContent = this.matches[0].properties.providerName;
      row.appendChild(cell);
      head.appendChild(row);
      return head;
    }

    /** @return {boolean} */
    get hasAdditionalProperties() {
      return this.matches.some(match => match.hasAdditionalProperties);
    }
  }

  /** Helps track and render a single match. */
  class OutputMatch {
    /** @param {!mojom.AutocompleteMatch} match */
    constructor(match) {
      /** @dict */
      this.properties = {};
      /** @dict */
      this.additionalProperties = {};
      Object.entries(match).forEach(propertyNameValueTuple => {
        // TODO(manukh) replace with destructuring when the styleguide is
        // updated
        // https://chromium-review.googlesource.com/c/chromium/src/+/1271915
        const propertyName = propertyNameValueTuple[0];
        const propertyValue = propertyNameValueTuple[1];

        if (PROPERTY_OUTPUT_ORDER.some(
                displayProperty =>
                    displayProperty.propertyName === propertyName)) {
          this.properties[propertyName] = propertyValue;
        } else {
          this.additionalProperties[propertyName] = propertyValue;
        }
      });

      /** @type {!Element} */
      this.associatedElement;
    }

    /**
     * Creates a HTML Node representing this data.
     * @param {boolean} showDetails
     * @return {!Element}
     */
    render(showDetails) {
      const row = document.createElement('tr');
      OutputMatch.displayedProperties(showDetails)
          .map(property => {
            const value = this.properties[property.propertyName];
            if (typeof value === 'boolean')
              return OutputMatch.renderBooleanProperty_(value);
            if (typeof value === 'object') {
              // We check if the first element has key and value properties.
              if (value && value[0] && value[0].key && value[0].value)
                return OutputMatch.renderKeyValueTuples_(value);
              else
                return OutputMatch.renderJsonProperty_(value);
            }
            const LINK_REGEX = /^(http|https|ftp|chrome|file):\/\//;
            if (LINK_REGEX.test(value))
              return OutputMatch.renderLinkProperty_(value);
            return OutputMatch.renderTextProperty_(value);
          })
          .forEach(cell => row.appendChild(cell));

      if (showDetails && this.hasAdditionalProperties) {
        row.appendChild(
            OutputMatch.renderJsonProperty_(this.additionalProperties));
      }
      this.associatedElement = row;
      return this.associatedElement;
    }

    /**
     * TODO(manukh) replace these static render_ functions with subclasses when
     * rendering becomes more substantial
     * @private
     * @param {string} propertyValue
     * @return {!Element}
     */
    static renderTextProperty_(propertyValue) {
      const cell = document.createElement('td');
      cell.textContent = propertyValue;
      return cell;
    }

    /**
     * @private
     * @param {Object} propertyValue
     * @return {!Element}
     */
    static renderJsonProperty_(propertyValue) {
      const cell = document.createElement('td');
      const pre = document.createElement('pre');
      pre.textContent = JSON.stringify(propertyValue, null, 2);
      cell.appendChild(pre);
      return cell;
    }

    /**
     * @private
     * @param {boolean} propertyValue
     * @return {!Element}
     */
    static renderBooleanProperty_(propertyValue) {
      const cell = document.createElement('td');
      const icon = document.createElement('div');
      icon.className = propertyValue ? 'check-mark' : 'x-mark';
      icon.textContent = propertyValue;
      cell.appendChild(icon);
      return cell;
    }

    /**
     * @private
     * @param {string} propertyValue
     * @return {!Element}
     */
    static renderLinkProperty_(propertyValue) {
      const cell = document.createElement('td');
      const link = document.createElement('a');
      link.textContent = propertyValue;
      link.href = propertyValue;
      cell.appendChild(link);
      return cell;
    }

    /**
     * @private
     * @param {Array<{key: string, value: string}>} propertyValue
     * @return {Element}
     */
    static renderKeyValueTuples_(propertyValue) {
      const cell = document.createElement('td');
      const pre = document.createElement('pre');
      const text = propertyValue.reduce(
          (prev, current) => `${prev}${current.key}: ${current.value}\n`, '');
      pre.textContent = text;
      cell.appendChild(pre);
      return cell;
    }

    /**
     * @param {string} name
     * @param {string=} url
     * @param {string=} tooltip
     * @return {!Element}
     */
    static renderHeaderCell(name, url, tooltip) {
      const cell = document.createElement('th');
      if (url) {
        const link = document.createElement('a');
        link.textContent = name;
        link.href = url;
        cell.appendChild(link);
      } else {
        cell.textContent = name;
      }
      cell.className =
          'column-' + name.replace(/[A-Z]/g, c => '-' + c.toLowerCase());
      cell.title = tooltip || '';
      return cell;
    }

    /**
     * @return {!Array<!PresentationInfoRecord>} Array representing which columns
     * need to be displayed.
     */
    static displayedProperties(showDetails) {
      return showDetails ?
          PROPERTY_OUTPUT_ORDER :
          PROPERTY_OUTPUT_ORDER.filter(property => property.displayAlways);
    }

    /**
     * @return {boolean} Used to determine if the additional properties column
     * needs to be displayed for this match.
     */
    get hasAdditionalProperties() {
      return Object.keys(this.additionalProperties).length > 0;
    }
  }

  /** Responsible for setting clipboard contents. */
  class CopyDelegate {
    /** @param {!omnibox_output.OmniboxOutput} omniboxOutput */
    constructor(omniboxOutput) {
      /** @private {!omnibox_output.OmniboxOutput} */
      this.omniboxOutput_ = omniboxOutput;
    }

    copyTextOutput() {
      this.copy_(this.omniboxOutput_.renderDelegate.visibletableText);
    }

    copyJsonOutput() {
      this.copy_(JSON.stringify(this.omniboxOutput_.responses, null, 2));
    }

    /**
     * @private
     * @param {string} value
     */
    copy_(value) {
      navigator.clipboard.writeText(value).catch(
          error => console.error('unable to copy to clipboard:', error));
    }
  }

  /** Responsible for highlighting and hiding rows using filter text. */
  class FilterDelegate {
    /** @param {!omnibox_output.OmniboxOutput} omniboxOutput */
    constructor(omniboxOutput) {
      /** @private {!omnibox_output.OmniboxOutput} */
      this.omniboxOutput_ = omniboxOutput;
    }

    /**
     * @param {string} filterText
     * @param {boolean} filterHide
     */
    filter(filterText, filterHide) {
      this.omniboxOutput_.matches.filter(match => match.associatedElement)
          .forEach(match => {
            const row = match.associatedElement;
            row.classList.remove('filtered-hidden');
            row.classList.remove('filtered-highlighted');

            if (!filterText)
              return;

            const isMatch = FilterDelegate.filterMatch_(match, filterText);
            row.classList.toggle('filtered-hidden', filterHide && !isMatch);
            row.classList.toggle(
                'filtered-highlighted', !filterHide && isMatch);
          });
    }

    /**
     * Checks if a omnibox match fuzzy-matches a filter string. Each character
     * of filterText must be present in the match text, either adjacent to the
     * previous matched character, or at the start of a new word (see
     * textToWords_).
     * E.g. `abc` matches `abc`, `a big cat`, `a-bigCat`, `a very big cat`, and
     * `an amBer cat`; but does not match `abigcat` or `an amber cat`.
     * `green rainbow` is matched by `gre rain`, but not by `gre bow`.
     * One exception is the first character, which may be matched mid-word.
     * E.g. `een rain` can also match `green rainbow`.
     * @private
     * @param {!OutputMatch} match
     * @param {string} filterText
     * @return {boolean}
     */
    static filterMatch_(match, filterText) {
      const row = match.associatedElement;
      const cells = Array.from(row.querySelectorAll('td'));
      const regexFilter = Array.from(filterText).join('(.*\\.)?');
      return cells
          .map(cell => FilterDelegate.textToWords_(cell.textContent).join('.'))
          .some(text => text.match(regexFilter));
    }

    /**
     * Splits a string into words, delimited by either capital letters, groups
     * of digits, or non alpha characters.
     * E.g., `https://google.com/the-dog-ate-134pies` will be split to:
     * https, :, /, /, google, ., com, /, the, -, dog, -, ate, -, 134, pies
     * We don't use `Array.split`, because we want to group digits, e.g. 134.
     * @private
     * @param {string} text
     * @return {!Array<string>}
     */
    static textToWords_(text) {
      return text.match(/[a-z]+|[A-Z][a-z]*|\d+|./g) || [];
    }
  }

  window.customElements.define(OmniboxOutput.is, OmniboxOutput);

  return {OmniboxOutput: OmniboxOutput};
});
