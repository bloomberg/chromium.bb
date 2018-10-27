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
      header: 'Description',
      url: '',
      propertyName: 'description',
      displayAlways: false,
      tooltip: 'The page title of the result.'
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

  class OmniboxOutput extends OmniboxElement {
    /** @return {string} */
    static get is() {
      return 'omnibox-output';
    }

    constructor() {
      super('omnibox-output-template');
    }

    /** @param {Element} element the element to the output */
    addOutput(element) {
      this.$$('contents').appendChild(element);
    }

    clearOutput() {
      while (this.$$('contents').firstChild)
        this.$$('contents').removeChild(this.$$('contents').firstChild);
    }
  }

  /** Helps track and render a list of results. */
  class OutputResultsTable {
    /** @param {Array<!mojom.AutocompleteMatch>} results */
    constructor(results) {
      /** @type {Array<OutputMatch>} */
      this.matches = results.map(match => new OutputMatch(match));
    }

    /**
     * Creates a HTML Node representing this data.
     * @param {boolean} showDetails
     * @return {Node}
     */
    render(showDetails) {
      const resultsTable = OmniboxElement.getTemplate('results-table-template');
      // The additional properties column only needs be displayed if at least
      // one of the results have additional properties.
      const showAdditionalPropertiesHeader = this.matches.some(
          match => match.showAdditionalProperties(showDetails));
      resultsTable.querySelector('.results-table-body')
          .appendChild(OutputMatch.renderHeader_(
              showDetails, showAdditionalPropertiesHeader));
      this.matches.forEach(
          match => resultsTable.querySelector('.results-table-body')
                       .appendChild(match.render(showDetails)));
      return resultsTable;
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
    }

    /**
     * Creates a HTML Node representing this data.
     * @param {boolean} showDetails
     * @return {Node}
     */
    render(showDetails) {
      const match = document.createElement('tr');
      OutputMatch.displayedProperties(showDetails)
          .map(property => {
            const value = this.properties[property.propertyName];
            if (typeof value === 'object')
              return OutputMatch.renderJsonProperty_(value);
            if (typeof value === 'boolean')
              return OutputMatch.renderBooleanProperty_(value);
            return OutputMatch.renderTextProperty_(value);
          })
          .forEach(cell => match.appendChild(cell));

      if (this.showAdditionalProperties(showDetails)) {
        match.appendChild(
            OutputMatch.renderJsonProperty_(this.additionalProperties));
      }
      return match;
    }

    /**
     * TODO(manukh) replace these static render_ functions with subclasses when
     * rendering becomes more substantial
     * @private
     * @param {string} propertyValue
     * @return {Node}
     */
    static renderTextProperty_(propertyValue) {
      const cell = document.createElement('td');
      cell.textContent = propertyValue;
      return cell;
    }

    /**
     * @private
     * @param {Object} propertyValue
     * @return {Node}
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
     * @return {Node}
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
     * @param {boolean} showDetails
     * @param {boolean} showAdditionalHeader
     * @return {Node}
     */
    static renderHeader_(showDetails, showAdditionalHeader) {
      const row = document.createElement('tr');
      const headerCells =
          OutputMatch.displayedProperties(showDetails)
              .map(
                  displayProperty => OutputMatch.renderHeaderCell_(
                      displayProperty.header, displayProperty.url,
                      displayProperty.tooltip));
      if (showAdditionalHeader) {
        headerCells.push(
            OutputMatch.renderHeaderCell_('Additional Properties'));
      }
      headerCells.forEach(headerCell => row.appendChild(headerCell));
      return row;
    }

    /**
     * @private
     * @param {string} name
     * @param {string=} url
     * @param {string=} tooltip
     * @return {Node}
     */
    static renderHeaderCell_(name, url, tooltip) {
      const cell = document.createElement('th');
      if (url) {
        const link = document.createElement('a');
        link.textContent = name;
        link.href = url;
        cell.appendChild(link);
      } else {
        cell.textContent = name;
      }
      cell.title = tooltip || '';
      return cell;
    }

    /**
     * @return {Array<PresentationInfoRecord>} Array representing which columns
     * need to be displayed.
     */
    static displayedProperties(showDetails) {
      return showDetails ?
          PROPERTY_OUTPUT_ORDER :
          PROPERTY_OUTPUT_ORDER.filter(property => property.displayAlways);
    }

    /**
     * @return {boolean} True if the additional properties column is required
     * to be displayed for this result. False if the column can be hidden
     * because this result does not have additional properties.
     */
    showAdditionalProperties(showDetails) {
      return showDetails && Object.keys(this.additionalProperties).length;
    }
  }

  window.customElements.define(OmniboxOutput.is, OmniboxOutput);

  // TODO(manukh) remove PROPERTY_OUTPUT_ORDER from exports after the remaining
  // OmniboxOutput classes are extracted
  // TODO(manukh) use shorthand object creation if/when approved
  // https://chromium.googlesource.com/chromium/src/+/master/styleguide/web/es6.md#object-literal-extensions
  return {
    OmniboxOutput: OmniboxOutput,
    OutputResultsTable: OutputResultsTable,
  };
});
