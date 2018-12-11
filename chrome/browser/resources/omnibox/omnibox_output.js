// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('omnibox_output', function() {
  /** @param {!Element} element*/
  function clearChildren(element) {
    while (element.firstChild)
      element.firstChild.remove();
  }

  class OmniboxOutput extends OmniboxElement {
    constructor() {
      super('omnibox-output-template');

      /** @type {!CopyDelegate} */
      this.copyDelegate = new CopyDelegate(this);

      /** @type {!Array<!mojom.OmniboxResult>} */
      this.responses = [];
      /** @private {!Array<!OutputResultsGroup>} */
      this.resultsGroups_ = [];
      /** @private {!QueryInputs} */
      this.queryInputs_ = /** @type {!QueryInputs} */ ({});
      /** @private {!DisplayInputs} */
      this.displayInputs_ = /** @type {!DisplayInputs} */ ({});
    }

    /** @param {!QueryInputs} queryInputs */
    updateQueryInputs(queryInputs) {
      this.queryInputs_ = queryInputs;
    }

    /** @param {!DisplayInputs} displayInputs */
    updateDisplayInputs(displayInputs) {
      this.displayInputs_ = displayInputs;
      this.updateVisibility_();
    }

    clearAutocompleteResponses() {
      this.responses = [];
      this.resultsGroups_ = [];
      clearChildren(this.$$('contents'));
    }

    /** @param {!mojom.OmniboxResult} response */
    addAutocompleteResponse(response) {
      this.responses.push(response);

      const resultsGroup =
          OutputResultsGroup.create(response, this.queryInputs_.cursorPosition);
      this.resultsGroups_.push(resultsGroup);
      this.$$('contents').appendChild(resultsGroup);

      this.updateVisibility_();
    }

    /**
     * Show or hide various output elements depending on display inputs.
     * 1) Show non-last result groups only if showIncompleteResults is true.
     * 2) Show the details section above each table if showDetails or
     * showIncompleteResults are true.
     * 3) Show individual results when showAllProviders is true.
     * 4) Show certain columns and headers only if they showDetails is true.
     * @private
     */
    updateVisibility_() {
      // Show non-last result groups only if showIncompleteResults is true.
      this.resultsGroups_.forEach(
          (resultsGroup, index) => resultsGroup.hidden =
              !this.displayInputs_.showIncompleteResults &&
              index !== this.resultsGroups_.length - 1);

      this.resultsGroups_.forEach(resultsGroup => {
        resultsGroup.updateVisibility(
            this.displayInputs_.showIncompleteResults,
            this.displayInputs_.showDetails,
            this.displayInputs_.showAllProviders);
      });
    }

    /**
     * @param {string} filterText
     * @param {boolean} filterHide
     */
    filter(filterText, filterHide) {
      this.matches.forEach(match => match.filter(filterText, filterHide));
    }

    /** @return {!Array<!OutputMatch>} */
    get matches() {
      return this.resultsGroups_.flatMap(resultsGroup => resultsGroup.matches);
    }

    /** @return {string} */
    get visibleTableText() {
      return this.resultsGroups_
          .flatMap(resultsGroup => resultsGroup.visibleText)
          .reduce((prev, cur) => `${prev}${cur}\n`, '');
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
  class OutputResultsGroup extends OmniboxElement {
    /**
     * @param {!mojom.OmniboxResult} resultsGroup
     * @param {number} cursorPosition
     * @return {!OutputResultsGroup}
     */
    static create(resultsGroup, cursorPosition) {
      const outputResultsGroup = new OutputResultsGroup();
      outputResultsGroup.setResultsGroup(resultsGroup, cursorPosition);
      return outputResultsGroup;
    }

    constructor() {
      super('details-and-table-template');
    }

    /**
     *  @param {!mojom.OmniboxResult} resultsGroup
     *  @param {number} cursorPosition
     */
    setResultsGroup(resultsGroup, cursorPosition) {
      /**
       * @type {{cursorPosition: number, time: number, done: boolean, host:
       *     string, isTypedHost: boolean}}
       */
      this.details = {
        cursorPosition: cursorPosition,
        time: resultsGroup.timeSinceOmniboxStartedMs,
        done: resultsGroup.done,
        host: resultsGroup.host,
        isTypedHost: resultsGroup.isTypedHost
      };
      /** @type {!Array<!OutputHeader>} */
      this.headers = COLUMNS.map(OutputHeader.create);
      /** @type {!OutputResultsTable} */
      this.combinedResults =
          OutputResultsTable.create(resultsGroup.combinedResults);
      /** @type {!Array<!OutputResultsTable>} */
      this.individualResultsList =
          resultsGroup.resultsByProvider
              .map(resultsWrapper => resultsWrapper.results)
              .filter(results => results.length > 0)
              .map(OutputResultsTable.create);
      if (this.hasAdditionalProperties)
        this.headers.push(OutputHeader.create(ADDITIONAL_PROPERTIES_COLUMN));
      this.render_();
    }

    /**
     * Creates a HTML Node representing this data.
     * @private
     */
    render_() {
      clearChildren(this);

      /** @private {!Array<!Element>} */
      this.innerHeaders_ = [];

      this.$$('details').appendChild(this.renderDetails_());
      this.$$('table').appendChild(this.renderHeader_());
      this.$$('table').appendChild(this.combinedResults);
      this.individualResultsList.forEach(results => {
        const innerHeader = this.renderInnerHeader_(results);
        this.innerHeaders_.push(innerHeader);
        this.$$('table').appendChild(innerHeader);
        this.$$('table').appendChild(results);
      });
    }

    /** @private @return {!Element} */
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

    /** @private @return {!Element} */
    renderHeader_() {
      const head = document.createElement('thead');
      const row = document.createElement('tr');
      this.headers.forEach(cell => row.appendChild(cell));
      head.appendChild(row);
      return head;
    }

    /**
     * @private
     * @param {!OutputResultsTable} results
     * @return {!Element}
     */
    renderInnerHeader_(results) {
      const head = document.createElement('thead');
      const row = document.createElement('tr');
      const cell = document.createElement('th');
      // Reserve 1 more column for showing the additional properties column.
      cell.colSpan = COLUMNS.length + 1;
      cell.textContent = results.innerHeaderText;
      row.appendChild(cell);
      head.appendChild(row);
      return head;
    }

    /**
     * @param {boolean} showIncompleteResults
     * @param {boolean} showDetails
     * @param {boolean} showAllProviders
     */
    updateVisibility(showIncompleteResults, showDetails, showAllProviders) {
      // Show the details section above each table if showDetails or
      // showIncompleteResults are true.
      this.$$('details').hidden = !showDetails && !showIncompleteResults;

      // Show individual results when showAllProviders is true.
      this.individualResultsList.forEach(
          individualResults => individualResults.hidden = !showAllProviders);
      this.innerHeaders_.forEach(
          innerHeader => innerHeader.hidden = !showAllProviders);

      // Show certain column headers only if they showDetails is true.
      COLUMNS.forEach(
          (column, index) => this.headers[index].hidden =
              !showDetails && !column.displayAlways);

      // Show certain columns only if they showDetails is true.
      this.matches.forEach(match => match.updateVisibility(showDetails));
    }

    /**
     * @private
     * @return {boolean}
     */
    get hasAdditionalProperties() {
      return this.combinedResults.hasAdditionalProperties ||
          this.individualResultsList.some(
              results => results.hasAdditionalProperties);
    }

    /** @return {!Array<!OutputMatch>} */
    get matches() {
      return [this.combinedResults]
          .concat(this.individualResultsList)
          .flatMap(results => results.matches);
    }

    /** @return {!Array<string>} */
    get visibleText() {
      return Array.from(this.shadowRoot.querySelectorAll(':host > :not(style)'))
          .map(child => child.innerText);
    }
  }

  /**
   * Helps track and render a list of results. Each result is tracked and
   * rendered by OutputMatch below.
   */
  class OutputResultsTable extends HTMLTableSectionElement {
    /**
     * @param {!Array<!mojom.AutocompleteMatch>} results
     * @return {!OutputResultsTable}
     */
    static create(results) {
      const resultsTable = new OutputResultsTable();
      resultsTable.results = results;
      return resultsTable;
    }

    constructor() {
      super();
      /** @type {!Array<!OutputMatch>} */
      this.matches = [];
    }

    /** @param {!Array<!mojom.AutocompleteMatch>} results */
    set results(results) {
      this.matches.forEach(match => match.remove());
      this.matches = results.map(OutputMatch.create);
      this.matches.forEach(this.appendChild.bind(this));
    }

    /** @return {string} */
    get innerHeaderText() {
      return this.matches[0].providerName;
    }

    /** @return {boolean} */
    get hasAdditionalProperties() {
      return this.matches.some(match => match.hasAdditionalProperties);
    }
  }

  /** Helps track and render a single match. */
  class OutputMatch extends HTMLTableRowElement {
    /**
     * @param {!mojom.AutocompleteMatch} match
     * @return {!OutputMatch}
     */
    static create(match) {
      /** @suppress {checkTypes} */
      const outputMatch = new OutputMatch();
      outputMatch.match = match;
      return outputMatch;
    }

    /** @param {!mojom.AutocompleteMatch} match */
    set match(match) {
      /** @type {!Object} */
      this.properties = {};
      /** @type {string} */
      this.providerName = match.providerName;

      COLUMNS.forEach(column => {
        const values = column.sourceProperties.map(
            propertyName =>
                /** @type {Object} */ (match)[propertyName]);
        this.properties[column.propertyName] =
            OutputProperty.create(column, values);
      });

      const unconsumedProperties = {};
      Object.entries(match)
          .filter(propertyNameValueTuple => {
            const propertyName = propertyNameValueTuple[0];
            return !CONSUMED_SOURCE_PROPERTIES.includes(propertyName);
          })
          .forEach(propertyNameValueTuple => {
            const propertyName = propertyNameValueTuple[0];
            const propertyValue = propertyNameValueTuple[1];
            unconsumedProperties[propertyName] = propertyValue;
          });

      /** @type {!OutputProperty} */
      this.additionalProperties = OutputProperty.create(
          ADDITIONAL_PROPERTIES_COLUMN, [unconsumedProperties]);

      this.render_();
    }

    /** @private */
    render_() {
      clearChildren(this);
      COLUMNS.map(property => this.properties[property.propertyName])
          .forEach(cell => this.appendChild(cell));
      if (this.hasAdditionalProperties)
        this.appendChild(this.additionalProperties);
    }

    /** @param {boolean} showDetails */
    updateVisibility(showDetails) {
      // Show certain columns only if they showDetails is true.
      COLUMNS.forEach(column => {
        this.properties[column.propertyName].hidden =
            !showDetails && !column.displayAlways;
      });
    }

    /**
     * @param {string} filterText
     * @param {boolean} filterHide
     */
    filter(filterText, filterHide) {
      this.hidden = false;
      this.classList.remove('filtered-highlighted');
      this.allProperties_.forEach(
          property => property.classList.remove('filtered-highlighted-nested'));

      if (!filterText)
        return;

      const matchedProperties = this.allProperties_.filter(
          property => FilterUtil.filterText(property.text, filterText));
      const isMatch = matchedProperties.length > 0;
      this.hidden = filterHide && !isMatch;
      this.classList.toggle('filtered-highlighted', !filterHide && isMatch);
      matchedProperties.forEach(
          property => property.classList.add('filtered-highlighted-nested'));
    }

    /**
     * @return {boolean} Used to determine if the additional properties column
     * needs to be displayed for this match.
     */
    get hasAdditionalProperties() {
      return Object.keys(this.additionalProperties.value).length > 0;
    }

    /** @private @return !Array<!OutputProperty> */
    get allProperties_() {
      return Object.values(this.properties).concat(this.additionalProperties);
    }
  }

  class OutputHeader extends HTMLTableCellElement {
    /**
     * @param {Column} column
     * @return {!OutputHeader}
     */
    static create(column) {
      const header = new OutputHeader();
      header.classList.add(column.headerClassName);
      header.setContents(column.header, column.url);
      header.title = column.tooltip;
      return header;
    }

    constructor() {
      super();
      /** @private {!Element} */
      this.div_ = document.createElement('div');
      this.appendChild(this.div_);
      /** @private {!Element} */
      this.link_ = document.createElement('a');
    }

    /**
     * @param {string} text
     * @param {string=} url
     */
    setContents(text, url) {
      if (url) {
        this.textContent = '';
        this.link_.textContent = text;
        this.link_.href = url;
        this.appendChild(this.link_);
      } else {
        this.textContent = text;
        this.link_.remove();
      }
    }
  }

  class OutputProperty extends HTMLTableCellElement {
    /**
     * @param {Column} column
     * @param {!Array<!Object>} values
     * @return {!OutputProperty}
     */
    static create(column, values) {
      const outputProperty = new column.outputClass();
      outputProperty.className = column.cellClassName;
      outputProperty.name = column.header;
      outputProperty.values = values;
      return outputProperty;
    }

    /** @return {!Object} */
    get value() {
      return this.value_;
    }

    /** @param {!Array<!Object>} values */
    set values(values) {
      /** @private {!Object} */
      this.value_ = values[0];
      /** @private {!Array<!Object>} */
      this.values_ = values;
      /** @override */
      this.render_();
    }

    /** @private */
    render_() {}

    /** @return {string} */
    get text() {
      return this.value_ + '';
    }
  }

  class OutputPairProperty extends OutputProperty {
    constructor() {
      super();

      this.container_ = document.createElement('div');
      this.container_.classList.add('pair-container');
      this.appendChild(this.container_);

      /** @type {!Element} */
      this.first_ = document.createElement('div');
      this.first_.classList.add('pair-item');
      this.container_.appendChild(this.first_);

      /** @type {!Element} */
      this.second_ = document.createElement('div');
      this.second_.classList.add('pair-item');
      this.container_.appendChild(this.second_);
    }

    /** @private @override */
    render_() {
      this.first_.textContent = this.values_[0];
      this.second_.textContent = this.values_[1];
    }

    /** @override @return {string} */
    get text() {
      return `${this.values_[0]}.${this.values_[1]}`;
    }
  }

  class OutputOverlappingPairProperty extends OutputPairProperty {
    constructor() {
      super();

      this.notOverlapWarning_ = document.createElement('div');
      this.notOverlapWarning_.classList.add('overlap-warning');
      this.container_.appendChild(this.notOverlapWarning_);
    }

    /** @private @override */
    render_() {
      const overlap = this.values_[0].endsWith(this.values_[1]);
      const firstText = this.values_[1] && overlap ?
          this.values_[0].slice(0, -this.values_[1].length) :
          this.values_[0];

      this.first_.textContent = firstText;
      this.second_.textContent = this.values_[1];
      this.notOverlapWarning_.textContent = overlap ?
          '' :
          `btw, these texts do not overlap; '${
              this.values_[1]}' was expected to be a suffix of '${
              this.values_[0]}'`;
    }
  }

  class OutputBooleanProperty extends OutputProperty {
    constructor() {
      super();
      /** @private {!Element} */
      this.icon_ = document.createElement('div');
      this.appendChild(this.icon_);
    }

    /** @private @override */
    render_() {
      this.icon_.classList.toggle('check-mark', !!this.value);
      this.icon_.classList.toggle('x-mark', !this.value);
    }

    get text() {
      return (this.value_ ? 'is: ' : 'not: ') + this.name;
    }
  }

  class OutputJsonProperty extends OutputProperty {
    constructor() {
      super();
      /** @private {!Element} */
      this.pre_ = document.createElement('pre');
      this.pre_.classList.add('json');
      this.appendChild(this.pre_);
    }

    /** @private @override */
    render_() {
      clearChildren(this.pre_);
      this.text.split(/("(?:[^"\\]|\\.)*":?|\w+)/)
          .map(
              word => OutputJsonProperty.renderJsonWord(
                  word, OutputJsonProperty.classifyJsonWord(word)))
          .forEach(jsonSpan => this.pre_.appendChild(jsonSpan));
    }

    /** @override @return {string} */
    get text() {
      return JSON.stringify(this.value_, null, 2);
    }

    /**
     * @param {string} word
     * @param {string|undefined} cls
     * @return {!Element}
     */
    static renderJsonWord(word, cls) {
      const span = document.createElement('span');
      if (cls)
        span.classList.add(cls);
      span.textContent = word;
      return span;
    }

    /**
     * @param {string} word
     * @return {string|undefined}
     */
    static classifyJsonWord(word) {
      if (/^\d+$/.test(word))
        return 'number';
      if (/^"[^]*":$/.test(word))
        return 'key';
      if (/^"[^]*"$/.test(word))
        return 'string';
      if (/true|false/.test(word))
        return 'boolean';
      if (/null/.test(word))
        return 'null';
    }
  }

  class OutputKeyValueTuplesProperty extends OutputJsonProperty {
    /** @private @override */
    render_() {
      clearChildren(this.pre_);
      this.value.forEach(({key, value}) => {
        this.pre_.appendChild(
            OutputJsonProperty.renderJsonWord(key + ': ', 'key'));
        this.pre_.appendChild(
            OutputJsonProperty.renderJsonWord(value + '\n', 'number'));
      });
    }

    /** @override @return {string} */
    get text() {
      return this.value_.reduce(
          (prev, {key, value}) => `${prev}${key}: ${value}\n`, '');
    }
  }

  class OutputLinkProperty extends OutputProperty {
    constructor() {
      super();
      /** @private {!Element} */
      this.link_ = document.createElement('a');
      this.appendChild(this.link_);
    }

    /** @private @override */
    render_() {
      this.link_.textContent = this.value_;
      this.link_.href = this.value_;
    }
  }

  class OutputTextProperty extends OutputProperty {
    constructor() {
      super();
      /** @private {!Element} */
      this.div_ = document.createElement('div');
      this.appendChild(this.div_);
    }

    /** @private @override */
    render_() {
      this.div_.textContent = this.value_;
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
      this.copy_(this.omniboxOutput_.visibleTableText);
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
  class FilterUtil {
    /**
     * Checks if a string fuzzy-matches a filter string. Each character
     * of filterText must be present in the search text, either adjacent to the
     * previous matched character, or at the start of a new word (see
     * textToWords_).
     * E.g. `abc` matches `abc`, `a big cat`, `a-bigCat`, `a very big cat`, and
     * `an amBer cat`; but does not match `abigcat` or `an amber cat`.
     * `green rainbow` is matched by `gre rain`, but not by `gre bow`.
     * One exception is the first character, which may be matched mid-word.
     * E.g. `een rain` can also match `green rainbow`.
     * @param {string} searchText
     * @param {string} filterText
     * @return {boolean}
     */
    static filterText(searchText, filterText) {
      const regexFilter =
          Array.from(filterText)
              .map(word => word.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'))
              .join('(.*\\.)?');
      const words = FilterUtil.textToWords_(searchText).join('.');
      return words.match(new RegExp(regexFilter, 'i')) !== null;
    }

    /**
     * Splits a string into words, delimited by either capital letters, groups
     * of digits, or non alpha characters.
     * E.g., `https://google.com/the-dog-ate-134pies` will be split to:
     * https, :, /, /, google, ., com, /, the, -,  dog, -, ate, -, 134, pies
     * We don't use `Array.split`, because we want to group digits, e.g. 134.
     * @private
     * @param {string} text
     * @return {!Array<string>}
     */
    static textToWords_(text) {
      return text.match(/[a-z]+|[A-Z][a-z]*|\d+|./g) || [];
    }
  }

  class Column {
    /**
     * @param {string} header
     * @param {string} url
     * @param {string} propertyName
     * @param {boolean} displayAlways
     * @param {string} tooltip
     * @param {function(new:OutputProperty)} outputClass
     * @param {!Array<string>} sourceProperties
     */
    constructor(
        header, url, propertyName, displayAlways, tooltip, sourceProperties,
        outputClass) {
      /** @type {string} */
      this.header = header;
      /** @type {string} */
      this.url = url;
      /** @type {string} */
      this.propertyName = propertyName;
      /** @type {boolean} */
      this.displayAlways = displayAlways;
      /** @type {string} */
      this.tooltip = tooltip;
      /** @type {!Array<string>} */
      this.sourceProperties = sourceProperties;
      /** @type {function(new:OutputProperty)} */
      this.outputClass = outputClass;

      const hyphenatedName =
          propertyName.replace(/[A-Z]/g, c => '-' + c.toLowerCase());
      /** @type {string} */
      this.cellClassName = 'cell-' + hyphenatedName;
      /** @type {string} */
      this.headerClassName = 'header-' + hyphenatedName;
    }
  }

  /**
   * A constant that's used to decide what autocomplete result
   * properties to output in what order.
   * @type {!Array<!Column>}
   */
  const COLUMNS = [
    new Column(
        'Provider & Type', '', 'providerAndType', true,
        'The AutocompleteProvider suggesting this result. / The type of the ' +
            'result.',
        ['providerName', 'type'], OutputPairProperty),
    new Column(
        'Relevance', '', 'relevance', true,
        'The result score. Higher is more relevant.', ['relevance'],
        OutputTextProperty),
    new Column(
        'Contents & Description', '', 'contentsAndDescription', true,
        'The text that is presented identifying the result. / The page title ' +
            'of the result.',
        ['contents', 'description'], OutputPairProperty),
    new Column(
        'Can Be Default', '', 'allowedToBeDefaultMatch', false,
        'A green checkmark indicates that the result can be the default ' +
            'match(i.e., can be the match that pressing enter in the ' +
            'omniboxnavigates to).',
        ['allowedToBeDefaultMatch'], OutputBooleanProperty),
    new Column(
        'Starred', '', 'starred', false,
        'A green checkmark indicates that the result has been bookmarked.',
        ['starred'], OutputBooleanProperty),
    new Column(
        'Has Tab Match', '', 'hasTabMatch', false,
        'A green checkmark indicates that the result URL matches an opentab.',
        ['hasTabMatch'], OutputBooleanProperty),
    new Column(
        'URL', '', 'destinationUrl', true, 'The URL for the result.',
        ['destinationUrl'], OutputLinkProperty),
    new Column(
        'Fill & Inline', '', 'fillAndInline', false,
        'The text shown in the omnibox when the result is selected. / The ' +
            'text shown in the omnibox as a blue highlight selection ' +
            'following the cursor, if this match is shown inline.',
        ['fillIntoEdit', 'inlineAutocompletion'],
        OutputOverlappingPairProperty),
    new Column(
        'Del', '', 'deletable', false,
        'A green checkmark indicates that the result can be deleted from the ' +
            'visit history.',
        ['deletable'], OutputBooleanProperty),
    new Column(
        'Prev', '', 'fromPrevious', false, '', ['fromPrevious'],
        OutputBooleanProperty),
    new Column(
        'Tran',
        'https://cs.chromium.org/chromium/src/ui/base/page_transition_types.h' +
            '?q=page_transition_types.h&sq=package:chromium&dr=CSs&l=14',
        'transition', false, 'How the user got to the result.', ['transition'],
        OutputTextProperty),
    new Column(
        'Done', '', 'providerDone', false,
        'A green checkmark indicates that the provider is done looking for ' +
            'more results.',
        ['providerDone'], OutputBooleanProperty),
    new Column(
        'Associated Keyword', '', 'associatedKeyword', false,
        'If non-empty, a "press tab to search" hint will be shown and will ' +
            'engage this keyword.',
        ['associatedKeyword'], OutputTextProperty),
    new Column(
        'Keyword', '', 'keyword', false,
        'The keyword of the search engine to be used.', ['keyword'],
        OutputTextProperty),
    new Column(
        'Duplicates', '', 'duplicates', false,
        'The number of matches that have been marked as duplicates of this ' +
            'match.',
        ['duplicates'], OutputTextProperty),
    new Column(
        'Additional Info', '', 'additionalInfo', false,
        'Provider-specific information about the result.', ['additionalInfo'],
        OutputKeyValueTuplesProperty)
  ];

  /** @type {!Column} */
  const ADDITIONAL_PROPERTIES_COLUMN = new Column(
      'Additional Properties', '', 'additionalProperties', false,
      'Properties not accounted for.', [], OutputJsonProperty);

  const CONSUMED_SOURCE_PROPERTIES =
      COLUMNS.flatMap(column => column.sourceProperties);

  customElements.define('omnibox-output', OmniboxOutput);
  customElements.define('output-results-group', OutputResultsGroup);
  customElements.define(
      'output-results-table', OutputResultsTable, {extends: 'tbody'});
  customElements.define('output-match', OutputMatch, {extends: 'tr'});
  customElements.define('output-haeder', OutputHeader, {extends: 'th'});
  customElements.define(
      'output-pair-property', OutputPairProperty, {extends: 'td'});
  customElements.define(
      'output-overlapping-pair-property', OutputOverlappingPairProperty,
      {extends: 'td'});
  customElements.define(
      'output-boolean-property', OutputBooleanProperty, {extends: 'td'});
  customElements.define(
      'output-json-property', OutputJsonProperty, {extends: 'td'});
  customElements.define(
      'output-key-value-tuple-property', OutputKeyValueTuplesProperty,
      {extends: 'td'});
  customElements.define(
      'output-link-property', OutputLinkProperty, {extends: 'td'});
  customElements.define(
      'output-text-property', OutputTextProperty, {extends: 'td'});

  return {OmniboxOutput: OmniboxOutput};
});
