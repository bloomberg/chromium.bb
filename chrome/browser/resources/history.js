// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
// Globals:
var RESULTS_PER_PAGE = 150;
var MAX_SEARCH_DEPTH_MONTHS = 18;

// Amount of time between pageviews that we consider a 'break' in browsing,
// measured in milliseconds.
var BROWSING_GAP_TIME = 15 * 60 * 1000;

function $(o) {return document.getElementById(o);}

function createElementWithClassName(type, className) {
  var elm = document.createElement(type);
  elm.className = className;
  return elm;
}

// Escapes a URI as appropriate for CSS.
function encodeURIForCSS(uri) {
  // CSS uris need to have '(' and ')' escaped.
  return uri.replace(/\(/g, "\\(").replace(/\)/g, "\\)");
}

// TODO(glen): Get rid of these global references, replace with a controller
//     or just make the classes own more of the page.
var historyModel;
var historyView;
var localStrings;
var pageState;
var deleteQueue = [];
var selectionAnchor = -1;
var activePage = null;

const MenuButton = cr.ui.MenuButton;
const Command = cr.ui.Command;
const Menu = cr.ui.Menu;

function createDropDownBgImage(canvasName, colorSpec) {
  var ctx = document.getCSSCanvasContext('2d', canvasName, 6, 4);
  ctx.fillStyle = ctx.strokeStyle = colorSpec;
  ctx.beginPath();
  ctx.moveTo(0, 0);
  ctx.lineTo(6, 0);
  ctx.lineTo(3, 3);
  ctx.closePath();
  ctx.fill();
  ctx.stroke();
  return ctx;
}

// Create the canvases to be used as the drop down button background images.
var arrow = createDropDownBgImage('drop-down-arrow', 'hsl(214, 91%, 85%)');
var hoverArrow = createDropDownBgImage('drop-down-arrow-hover', '#6A86DE');
var activeArrow = createDropDownBgImage('drop-down-arrow-active', 'white');

///////////////////////////////////////////////////////////////////////////////
// Page:
/**
 * Class to hold all the information about an entry in our model.
 * @param {Object} result An object containing the page's data.
 * @param {boolean} continued Whether this page is on the same day as the
 *     page before it
 */
function Page(result, continued, model, id) {
  this.model_ = model;
  this.title_ = result.title;
  this.url_ = result.url;
  this.domain_ = this.getDomainFromURL_(this.url_);
  this.starred_ = result.starred;
  this.snippet_ = result.snippet || "";
  this.id_ = id;

  this.changed = false;

  this.isRendered = false;

  // All the date information is public so that owners can compare properties of
  // two items easily.

  // We get the time in seconds, but we want it in milliseconds.
  this.time = new Date(result.time * 1000);

  // See comment in BrowsingHistoryHandler::QueryComplete - we won't always
  // get all of these.
  this.dateRelativeDay = result.dateRelativeDay || "";
  this.dateTimeOfDay = result.dateTimeOfDay || "";
  this.dateShort = result.dateShort || "";

  // Whether this is the continuation of a previous day.
  this.continued = continued;
}

// Page, Public: --------------------------------------------------------------
/**
 * Returns a dom structure for a browse page result or a search page result.
 * @param {boolean} Flag to indicate if result is a search result.
 * @return {Element} The dom structure.
 */
Page.prototype.getResultDOM = function(searchResultFlag) {
  var node = createElementWithClassName('li', 'entry');
  var time = createElementWithClassName('div', 'time');
  var entryBox = createElementWithClassName('div', 'entry-box');
  var domain = createElementWithClassName('div', 'domain');

  var dropDown = createElementWithClassName('button', 'drop-down');
  dropDown.value = 'Open action menu';
  dropDown.title = localStrings.getString('actionMenuDescription');
  dropDown.setAttribute('menu', '#action-menu');
  cr.ui.decorate(dropDown, MenuButton);

  // Checkbox is always created, but only visible on hover & when checked.
  var checkbox = document.createElement('input');
  checkbox.type = 'checkbox';
  checkbox.id = 'checkbox-' + this.id_;
  checkbox.time = this.time.getTime();
  checkbox.addEventListener('click', checkboxClicked);
  time.appendChild(checkbox);

  // Keep track of the drop down that triggered the menu, so we know
  // which element to apply the command to.
  // TODO(dubroy): Ideally we'd use 'activate', but MenuButton swallows it.
  var self = this;
  var setActivePage = function(e) {
    activePage = self;
  };
  dropDown.addEventListener('mousedown', setActivePage);
  dropDown.addEventListener('focus', setActivePage);

  domain.style.backgroundImage =
    'url(chrome://favicon/' + encodeURIForCSS(this.url_) + ')';
  domain.textContent = this.domain_;

  // Clicking anywhere in the entryBox will check/uncheck the checkbox.
  entryBox.addEventListener('mousedown', entryBoxMousedown, false);

  // Prevent clicks on the drop down from affecting the checkbox.
  dropDown.addEventListener('click', function(e) { e.preventDefault(); });

  // A label around the parts that should be clicked to activate the check box.
  var label = document.createElement('label');
  label.appendChild(time);
  label.appendChild(domain);

  // We use a wrapper div so that the entry contents will be shinkwrapped.
  entryBox.appendChild(label);
  entryBox.appendChild(this.getTitleDOM_());
  entryBox.appendChild(dropDown);

  // Let the entryBox be styled appropriately when it contains keyboard focus.
  entryBox.addEventListener('focus', function() {
    this.classList.add('contains-focus');
  }, true);
  entryBox.addEventListener('blur', function() {
    this.classList.remove('contains-focus');
  }, true);

  node.appendChild(entryBox);

  if (searchResultFlag) {
    time.textContent = this.dateShort;
    var snippet = createElementWithClassName('div', 'snippet');
    this.addHighlightedText_(snippet,
                             this.snippet_,
                             this.model_.getSearchText());
    node.appendChild(snippet);
  } else {
    time.appendChild(document.createTextNode(this.dateTimeOfDay));
  }

  if (typeof this.domNode_ != 'undefined') {
      console.error('Already generated node for page.');
  }
  this.domNode_ = node;

  return node;
};

// Page, private: -------------------------------------------------------------
/**
 * Extracts and returns the domain (and subdomains) from a URL.
 * @param {string} The url
 * @return (string) The domain. An empty string is returned if no domain can
 *     be found.
 */
Page.prototype.getDomainFromURL_ = function(url) {
  var domain = url.replace(/^.+:\/\//, '').match(/[^/]+/);
  return domain ? domain[0] : '';
};

/**
 * Add child text nodes to a node such that occurrences of the specified text is
 * highlighted.
 * @param {Node} node The node under which new text nodes will be made as
 *     children.
 * @param {string} content Text to be added beneath |node| as one or more
 *     text nodes.
 * @param {string} highlightText Occurences of this text inside |content| will
 *     be highlighted.
 */
Page.prototype.addHighlightedText_ = function(node, content, highlightText) {
  var i = 0;
  if (highlightText) {
    var re = new RegExp(Page.pregQuote_(highlightText), 'gim');
    var match;
    while (match = re.exec(content)) {
      if (match.index > i)
        node.appendChild(document.createTextNode(content.slice(i,
                                                               match.index)));
      i = re.lastIndex;
      // Mark the highlighted text in bold.
      var b = document.createElement('b');
      b.textContent = content.substring(match.index, i);
      node.appendChild(b);
    }
  }
  if (i < content.length)
    node.appendChild(document.createTextNode(content.slice(i)));
};

/**
 * @return {DOMObject} DOM representation for the title block.
 */
Page.prototype.getTitleDOM_ = function() {
  var node = createElementWithClassName('div', 'title');
  var link = document.createElement('a');
  link.href = this.url_;
  link.id = "id-" + this.id_;

  // Add a tooltip, since it might be ellipsized.
  // TODO(dubroy): Find a way to show the tooltip only when necessary.
  link.title = this.title_;

  this.addHighlightedText_(link, this.title_, this.model_.getSearchText());
  node.appendChild(link);

  if (this.starred_) {
    node.className += ' starred';
    node.appendChild(createElementWithClassName('div', 'starred'));
  }

  return node;
};

/**
 * Launch a search for more history entries from the same domain.
 */
Page.prototype.showMoreFromSite_ = function() {
  setSearch(this.domain_);
};

/**
 * Remove a single entry from the history.
 */
Page.prototype.removeFromHistory_ = function() {
  var self = this;
  var onSuccessCallback = function() {
    removeEntryFromView(self.domNode_);
  };
  queueURLsForDeletion(this.time, [this.url_], onSuccessCallback);
  deleteNextInQueue();
};


// Page, private, static: -----------------------------------------------------

/**
 * Quote a string so it can be used in a regular expression.
 * @param {string} str The source string
 * @return {string} The escaped string
 */
Page.pregQuote_ = function(str) {
  return str.replace(/([\\\.\+\*\?\[\^\]\$\(\)\{\}\=\!\<\>\|\:])/g, "\\$1");
};

///////////////////////////////////////////////////////////////////////////////
// HistoryModel:
/**
 * Global container for history data. Future optimizations might include
 * allowing the creation of a HistoryModel for each search string, allowing
 * quick flips back and forth between results.
 *
 * The history model is based around pages, and only fetching the data to
 * fill the currently requested page. This is somewhat dependent on the view,
 * and so future work may wish to change history model to operate on
 * timeframe (day or week) based containers.
 */
function HistoryModel() {
  this.clearModel_();
}

// HistoryModel, Public: ------------------------------------------------------
/**
 * Sets our current view that is called when the history model changes.
 * @param {HistoryView} view The view to set our current view to.
 */
HistoryModel.prototype.setView = function(view) {
  this.view_ = view;
};

/**
 * Start a new search - this will clear out our model.
 * @param {String} searchText The text to search for
 * @param {Number} opt_page The page to view - this is mostly used when setting
 *     up an initial view, use #requestPage otherwise.
 */
HistoryModel.prototype.setSearchText = function(searchText, opt_page) {
  this.clearModel_();
  this.searchText_ = searchText;
  this.requestedPage_ = opt_page ? opt_page : 0;
  this.getSearchResults_();
};

/**
 * Reload our model with the current parameters.
 */
HistoryModel.prototype.reload = function() {
  var search = this.searchText_;
  var page = this.requestedPage_;
  this.clearModel_();
  this.searchText_ = search;
  this.requestedPage_ = page;
  this.getSearchResults_();
};

/**
 * @return {String} The current search text.
 */
HistoryModel.prototype.getSearchText = function() {
  return this.searchText_;
};

/**
 * Tell the model that the view will want to see the current page. When
 * the data becomes available, the model will call the view back.
 * @page {Number} page The page we want to view.
 */
HistoryModel.prototype.requestPage = function(page) {
  this.requestedPage_ = page;
  this.changed = true;
  this.updateSearch_(false);
};

/**
 * Receiver for history query.
 * @param {String} term The search term that the results are for.
 * @param {Array} results A list of results
 */
HistoryModel.prototype.addResults = function(info, results) {
  this.inFlight_ = false;
  if (info.term != this.searchText_) {
    // If our results aren't for our current search term, they're rubbish.
    return;
  }

  // Currently we assume we're getting things in date order. This needs to
  // be updated if that ever changes.
  if (results) {
    var lastURL, lastDay;
    var oldLength = this.pages_.length;
    if (oldLength) {
      var oldPage = this.pages_[oldLength - 1];
      lastURL = oldPage.url;
      lastDay = oldPage.dateRelativeDay;
    }

    for (var i = 0, thisResult; thisResult = results[i]; i++) {
      var thisURL = thisResult.url;
      var thisDay = thisResult.dateRelativeDay;

      // Remove adjacent duplicates.
      if (!lastURL || lastURL != thisURL) {
        // Figure out if this page is in the same day as the previous page,
        // this is used to determine how day headers should be drawn.
        this.pages_.push(new Page(thisResult, thisDay == lastDay, this,
            this.last_id_++));
        lastDay = thisDay;
        lastURL = thisURL;
      }
    }
    if (results.length)
      this.changed = true;
  }

  this.updateSearch_(info.finished);
};

/**
 * @return {Number} The number of pages in the model.
 */
HistoryModel.prototype.getSize = function() {
  return this.pages_.length;
};

/**
 * @return {boolean} Whether our history query has covered all of
 *     the user's history
 */
HistoryModel.prototype.isComplete = function() {
  return this.complete_;
};

/**
 * Get a list of pages between specified index positions.
 * @param {Number} start The start index
 * @param {Number} end The end index
 * @return {Array} A list of pages
 */
HistoryModel.prototype.getNumberedRange = function(start, end) {
  if (start >= this.getSize())
    return [];

  var end = end > this.getSize() ? this.getSize() : end;
  return this.pages_.slice(start, end);
};

// HistoryModel, Private: -----------------------------------------------------
HistoryModel.prototype.clearModel_ = function() {
  this.inFlight_ = false; // Whether a query is inflight.
  this.searchText_ = '';
  this.searchDepth_ = 0;
  this.pages_ = []; // Date-sorted list of pages.
  this.last_id_ = 0;
  selectionAnchor = -1;

  // The page that the view wants to see - we only fetch slightly past this
  // point. If the view requests a page that we don't have data for, we try
  // to fetch it and call back when we're done.
  this.requestedPage_ = 0;

  this.complete_ = false;

  if (this.view_) {
    this.view_.clear_();
  }
};

/**
 * Figure out if we need to do more searches to fill the currently requested
 * page. If we think we can fill the page, call the view and let it know
 * we're ready to show something.
 */
HistoryModel.prototype.updateSearch_ = function(finished) {
  if ((this.searchText_ && this.searchDepth_ >= MAX_SEARCH_DEPTH_MONTHS) ||
      finished) {
    // We have maxed out. There will be no more data.
    this.complete_ = true;
    this.view_.onModelReady();
    this.changed = false;
  } else {
    // If we can't fill the requested page, ask for more data unless a request
    // is still in-flight.
    if (!this.canFillPage_(this.requestedPage_) && !this.inFlight_) {
      this.getSearchResults_(this.searchDepth_ + 1);
    }

    // If we have any data for the requested page, show it.
    if (this.changed && this.haveDataForPage_(this.requestedPage_)) {
      this.view_.onModelReady();
      this.changed = false;
    }
  }
};

/**
 * Get search results for a selected depth. Our history system is optimized
 * for queries that don't cross month boundaries, but an entire month's
 * worth of data is huge. When we're in browse mode (searchText is empty)
 * we request the data a day at a time. When we're searching, a month is
 * used.
 *
 * TODO: Fix this for when the user's clock goes across month boundaries.
 * @param {number} opt_day How many days back to do the search.
 */
HistoryModel.prototype.getSearchResults_ = function(depth) {
  this.searchDepth_ = depth || 0;

  if (this.searchText_ == "") {
    chrome.send('getHistory',
        [String(this.searchDepth_)]);
  } else {
    chrome.send('searchHistory',
        [this.searchText_, String(this.searchDepth_)]);
  }

  this.inFlight_ = true;
};

/**
 * Check to see if we have data for a given page.
 * @param {number} page The page number
 * @return {boolean} Whether we have any data for the given page.
 */
HistoryModel.prototype.haveDataForPage_ = function(page) {
  return (page * RESULTS_PER_PAGE < this.getSize());
};

/**
 * Check to see if we have data to fill a page.
 * @param {number} page The page number.
 * @return {boolean} Whether we have data to fill the page.
 */
HistoryModel.prototype.canFillPage_ = function(page) {
  return ((page + 1) * RESULTS_PER_PAGE <= this.getSize());
};

///////////////////////////////////////////////////////////////////////////////
// HistoryView:
/**
 * Functions and state for populating the page with HTML. This should one-day
 * contain the view and use event handlers, rather than pushing HTML out and
 * getting called externally.
 * @param {HistoryModel} model The model backing this view.
 */
function HistoryView(model) {
  this.summaryTd_ = $('results-summary');
  this.summaryTd_.textContent = localStrings.getString('loading');
  this.editButtonTd_ = $('edit-button');
  this.editingControlsDiv_ = $('editing-controls');
  this.resultDiv_ = $('results-display');
  this.pageDiv_ = $('results-pagination');
  this.model_ = model
  this.pageIndex_ = 0;
  this.lastDisplayed_ = [];

  this.model_.setView(this);

  this.currentPages_ = [];

  var self = this;
  window.onresize = function() {
    self.updateEntryAnchorWidth_();
  };

  $('clear-browsing-data').addEventListener('click', openClearBrowsingData);
  $('remove-selected').addEventListener('click', removeItems);
}

// HistoryView, public: -------------------------------------------------------
/**
 * Do a search and optionally view a certain page.
 * @param {string} term The string to search for.
 * @param {number} opt_page The page we wish to view, only use this for
 *     setting up initial views, as this triggers a search.
 */
HistoryView.prototype.setSearch = function(term, opt_page) {
  this.pageIndex_ = parseInt(opt_page || 0, 10);
  window.scrollTo(0, 0);
  this.model_.setSearchText(term, this.pageIndex_);
  pageState.setUIState(term, this.pageIndex_);
};

/**
 * Reload the current view.
 */
HistoryView.prototype.reload = function() {
  this.model_.reload();
  this.updateRemoveButton();
};

/**
 * Switch to a specified page.
 * @param {number} page The page we wish to view.
 */
HistoryView.prototype.setPage = function(page) {
  this.clear_();
  this.pageIndex_ = parseInt(page, 10);
  window.scrollTo(0, 0);
  this.model_.requestPage(page);
  pageState.setUIState(this.model_.getSearchText(), this.pageIndex_);
};

/**
 * @return {number} The page number being viewed.
 */
HistoryView.prototype.getPage = function() {
  return this.pageIndex_;
};

/**
 * Callback for the history model to let it know that it has data ready for us
 * to view.
 */
HistoryView.prototype.onModelReady = function() {
  this.displayResults_();
};

/**
 * Enables or disables the 'Remove selected items' button as appropriate.
 */
HistoryView.prototype.updateRemoveButton = function() {
  var anyChecked = document.querySelector('.entry input:checked') != null;
  $('remove-selected').disabled = !anyChecked;
}

// HistoryView, private: ------------------------------------------------------
/**
 * Clear the results in the view.  Since we add results piecemeal, we need
 * to clear them out when we switch to a new page or reload.
 */
HistoryView.prototype.clear_ = function() {
  this.resultDiv_.textContent = '';

  var pages = this.currentPages_;
  for (var i = 0; i < pages.length; i++) {
    pages[i].isRendered = false;
  }
  this.currentPages_ = [];
};

HistoryView.prototype.setPageRendered_ = function(page) {
  page.isRendered = true;
  this.currentPages_.push(page);
};

/**
 * Update the page with results.
 */
HistoryView.prototype.displayResults_ = function() {
  var results = this.model_.getNumberedRange(
      this.pageIndex_ * RESULTS_PER_PAGE,
      this.pageIndex_ * RESULTS_PER_PAGE + RESULTS_PER_PAGE);

  if (this.model_.getSearchText()) {
    var searchResults = createElementWithClassName('ol', 'search-results');
    for (var i = 0, page; page = results[i]; i++) {
      if (!page.isRendered) {
        searchResults.appendChild(page.getResultDOM(true));
        this.setPageRendered_(page);
      }
    }
    this.resultDiv_.appendChild(searchResults);
  } else {
    var resultsFragment = document.createDocumentFragment();
    var lastTime = Math.infinity;
    var dayResults;
    for (var i = 0, page; page = results[i]; i++) {
      if (page.isRendered) {
        continue;
      }
      // Break across day boundaries and insert gaps for browsing pauses.
      // Create a dayResults element to contain results for each day
      var thisTime = page.time.getTime();

      if ((i == 0 && page.continued) || !page.continued) {
        var day = createElementWithClassName('h2', 'day');
        day.appendChild(document.createTextNode(page.dateRelativeDay));
        if (i == 0 && page.continued) {
          day.appendChild(document.createTextNode(' ' +
              localStrings.getString('cont')));
        }

        // If there is an existing dayResults element, append it.
        if (dayResults) {
          resultsFragment.appendChild(dayResults);
        }
        resultsFragment.appendChild(day);
        dayResults = createElementWithClassName('ol', 'day-results');
      } else if (lastTime - thisTime > BROWSING_GAP_TIME) {
        if (dayResults) {
          dayResults.appendChild(createElementWithClassName('li', 'gap'));
        }
      }
      lastTime = thisTime;
      // Add entry.
      if (dayResults) {
        dayResults.appendChild(page.getResultDOM(false));
        this.setPageRendered_(page);
      }
    }
    // Add final dayResults element.
    if (dayResults) {
      resultsFragment.appendChild(dayResults);
    }
    this.resultDiv_.appendChild(resultsFragment);
  }

  this.displaySummaryBar_();
  this.displayNavBar_();
  this.updateEntryAnchorWidth_();
};

/**
 * Update the summary bar with descriptive text.
 */
HistoryView.prototype.displaySummaryBar_ = function() {
  var searchText = this.model_.getSearchText();
  if (searchText != '') {
    this.summaryTd_.textContent = localStrings.getStringF('searchresultsfor',
        searchText);
  } else {
    this.summaryTd_.textContent = localStrings.getString('history');
  }
};

/**
 * Update the pagination tools.
 */
HistoryView.prototype.displayNavBar_ = function() {
  this.pageDiv_.textContent = '';

  if (this.pageIndex_ > 0) {
    this.pageDiv_.appendChild(
        this.createPageNav_(0, localStrings.getString('newest')));
    this.pageDiv_.appendChild(
        this.createPageNav_(this.pageIndex_ - 1,
                            localStrings.getString('newer')));
  }

  // TODO(feldstein): this causes the navbar to not show up when your first
  // page has the exact amount of results as RESULTS_PER_PAGE.
  if (this.model_.getSize() > (this.pageIndex_ + 1) * RESULTS_PER_PAGE) {
    this.pageDiv_.appendChild(
        this.createPageNav_(this.pageIndex_ + 1,
                            localStrings.getString('older')));
  }
};

/**
 * Make a DOM object representation of a page navigation link.
 * @param {number} page The page index the navigation element should link to
 * @param {string} name The text content of the link
 * @return {HTMLAnchorElement} the pagination link
 */
HistoryView.prototype.createPageNav_ = function(page, name) {
  anchor = document.createElement('a');
  anchor.className = 'page-navigation';
  anchor.textContent = name;
  var hashString = PageState.getHashString(this.model_.getSearchText(), page);
  var link = 'chrome://history/' + (hashString ? '#' + hashString : '');
  anchor.href = link;
  anchor.onclick = function() {
    setPage(page);
    return false;
  };
  return anchor;
};

/**
 * Updates the CSS rule for the entry anchor.
 * @private
 */
HistoryView.prototype.updateEntryAnchorWidth_ = function() {
  // We need to have at least on .title div to be able to calculate the
  // desired width of the anchor.
  var titleElement = document.querySelector('.entry .title');
  if (!titleElement)
    return;

  // Create new CSS rules and add them last to the last stylesheet.
  // TODO(jochen): The following code does not work due to WebKit bug #32309
  // if (!this.entryAnchorRule_) {
  //   var styleSheets = document.styleSheets;
  //   var styleSheet = styleSheets[styleSheets.length - 1];
  //   var rules = styleSheet.cssRules;
  //   var createRule = function(selector) {
  //     styleSheet.insertRule(selector + '{}', rules.length);
  //     return rules[rules.length - 1];
  //   };
  //   this.entryAnchorRule_ = createRule('.entry .title > a');
  //   // The following rule needs to be more specific to have higher priority.
  //   this.entryAnchorStarredRule_ = createRule('.entry .title.starred > a');
  // }
  //
  // var anchorMaxWith = titleElement.offsetWidth;
  // this.entryAnchorRule_.style.maxWidth = anchorMaxWith + 'px';
  // // Adjust by the width of star plus its margin.
  // this.entryAnchorStarredRule_.style.maxWidth = anchorMaxWith - 23 + 'px';
};

///////////////////////////////////////////////////////////////////////////////
// State object:
/**
 * An 'AJAX-history' implementation.
 * @param {HistoryModel} model The model we're representing
 * @param {HistoryView} view The view we're representing
 */
function PageState(model, view) {
  // Enforce a singleton.
  if (PageState.instance) {
    return PageState.instance;
  }

  this.model = model;
  this.view = view;

  if (typeof this.checker_ != 'undefined' && this.checker_) {
    clearInterval(this.checker_);
  }

  // TODO(glen): Replace this with a bound method so we don't need
  //     public model and view.
  this.checker_ = setInterval((function(state_obj) {
    var hashData = state_obj.getHashData();

    if (hashData.q != state_obj.model.getSearchText(term)) {
      state_obj.view.setSearch(hashData.q, parseInt(hashData.p, 10));
    } else if (parseInt(hashData.p, 10) != state_obj.view.getPage()) {
      state_obj.view.setPage(hashData.p);
    }
  }), 50, this);
}

PageState.instance = null;

/**
 * @return {Object} An object containing parameters from our window hash.
 */
PageState.prototype.getHashData = function() {
  var result = {
    e : 0,
    q : '',
    p : 0
  };

  if (!window.location.hash) {
    return result;
  }

  var hashSplit = window.location.hash.substr(1).split('&');
  for (var i = 0; i < hashSplit.length; i++) {
    var pair = hashSplit[i].split('=');
    if (pair.length > 1) {
      result[pair[0]] = decodeURIComponent(pair[1].replace(/\+/g, ' '));
    }
  }

  return result;
};

/**
 * Set the hash to a specified state, this will create an entry in the
 * session history so the back button cycles through hash states, which
 * are then picked up by our listener.
 * @param {string} term The current search string.
 * @param {string} page The page currently being viewed.
 */
PageState.prototype.setUIState = function(term, page) {
  // Make sure the form looks pretty.
  document.forms[0].term.value = term;
  var currentHash = this.getHashData();
  if (currentHash.q != term || currentHash.p != page) {
    window.location.hash = PageState.getHashString(term, page);
  }
};

/**
 * Static method to get the hash string for a specified state
 * @param {string} term The current search string.
 * @param {string} page The page currently being viewed.
 * @return {string} The string to be used in a hash.
 */
PageState.getHashString = function(term, page) {
  var newHash = [];
  if (term) {
    newHash.push('q=' + encodeURIComponent(term));
  }
  if (page != undefined) {
    newHash.push('p=' + page);
  }

  return newHash.join('&');
};

///////////////////////////////////////////////////////////////////////////////
// Document Functions:
/**
 * Window onload handler, sets up the page.
 */
function load() {
  $('term').focus();

  localStrings = new LocalStrings();
  historyModel = new HistoryModel();
  historyView = new HistoryView(historyModel);
  pageState = new PageState(historyModel, historyView);

  // Create default view.
  var hashData = pageState.getHashData();
  historyView.setSearch(hashData.q, hashData.p);

  // Setup click handlers.
  $('history-section').onclick = function () {
    setSearch('');
    return false;
  };
  $('search-form').onsubmit = function () {
    setSearch(this.term.value);
    return false;
  };

  $('remove-page').addEventListener('activate', function(e) {
    activePage.removeFromHistory_();
    activePage = null;
  });
  $('more-from-site').addEventListener('activate', function(e) {
    activePage.showMoreFromSite_();
    activePage = null;
  });
}

/**
 * TODO(glen): Get rid of this function.
 * Set the history view to a specified page.
 * @param {String} term The string to search for
 */
function setSearch(term) {
  if (historyView) {
    historyView.setSearch(term);
  }
}

/**
 * TODO(glen): Get rid of this function.
 * Set the history view to a specified page.
 * @param {number} page The page to set the view to.
 */
function setPage(page) {
  if (historyView) {
    historyView.setPage(page);
  }
}

/**
 * Delete the next item in our deletion queue.
 */
function deleteNextInQueue() {
  if (deleteQueue.length > 0) {
    // Call the native function to remove history entries.
    // First arg is a time in seconds (passed as String) identifying the day.
    // Remaining args are URLs of history entries from that day to delete.
    var timeInSeconds = Math.floor(deleteQueue[0].date.getTime() / 1000);
    chrome.send('removeURLsOnOneDay',
                [String(timeInSeconds)].concat(deleteQueue[0].urls));
  }
}

/**
 * Open the clear browsing data dialog.
 */
function openClearBrowsingData() {
  chrome.send('clearBrowsingData', []);
  return false;
}

/**
 * Queue a set of URLs from the same day for deletion.
 * @param {Date} date A date indicating the day the URLs were visited.
 * @param {Array} urls Array of URLs from the same day to be deleted.
 * @param {Function} opt_callback An optional callback to be executed when
 *        the deletion is complete.
 */
function queueURLsForDeletion(date, urls, opt_callback) {
  deleteQueue.push({ 'date': date, 'urls': urls, 'callback': opt_callback });
}

function reloadHistory() {
  historyView.reload();
}

/**
 * Collect IDs from checked checkboxes and send to Chrome for deletion.
 */
function removeItems() {
  var checked = document.querySelectorAll(
      'input[type=checkbox]:checked:not([disabled])');
  var urls = [];
  var disabledItems = [];
  var queue = [];
  var date = new Date();

  for (var i = 0; i < checked.length; i++) {
    var checkbox = checked[i];
    var cbDate = new Date(checkbox.time);
    if (date.getFullYear() != cbDate.getFullYear() ||
        date.getMonth() != cbDate.getMonth() ||
        date.getDate() != cbDate.getDate()) {
      if (urls.length > 0) {
        queue.push([date, urls]);
      }
      urls = [];
      date = cbDate;
    }
    var link = checkbox.parentNode.parentNode.parentNode.querySelector('a');
    checkbox.disabled = true;
    link.classList.add('to-be-removed');
    disabledItems.push(checkbox);
    urls.push(link.href);
  }
  if (urls.length > 0) {
    queue.push([date, urls]);
  }
  if (checked.length > 0 && confirm(localStrings.getString('deletewarning'))) {
    for (var i = 0; i < queue.length; i++) {
      // Reload the page when the final entry has been deleted.
      var callback = i == 0 ? reloadHistory : null;

      queueURLsForDeletion(queue[i][0], queue[i][1], callback);
    }
    deleteNextInQueue();
  } else {
    // If the remove is cancelled, return the checkboxes to their
    // enabled, non-line-through state.
    for (var i = 0; i < disabledItems.length; i++) {
      var checkbox = disabledItems[i];
      var link = checkbox.parentNode.parentNode.parentNode.querySelector('a');
      checkbox.disabled = false;
      link.classList.remove('to-be-removed');
    }
  }
  return false;
}

/**
 * Toggle state of checkbox and handle Shift modifier.
 */
function checkboxClicked(event) {
  var id = Number(this.id.slice("checkbox-".length));
  if (event.shiftKey && (selectionAnchor != -1)) {
    var checked = this.checked;
    // Set all checkboxes from the anchor up to the clicked checkbox to the
    // state of the clicked one.
    var begin = Math.min(id, selectionAnchor);
    var end = Math.max(id, selectionAnchor);
    for (var i = begin; i <= end; i++) {
      var checkbox = document.querySelector('#checkbox-' + i);
      if (checkbox)
        checkbox.checked = checked;
    }
  }
  selectionAnchor = id;

  historyView.updateRemoveButton();
}

function entryBoxMousedown(event) {
  // Prevent text selection when shift-clicking to select multiple entries.
  if (event.shiftKey) {
    event.preventDefault();
  }
}

function removeNode(node) {
  node.classList.add('fade-out'); // Trigger CSS fade out animation.

  // Delete the node when the animation is complete.
  node.addEventListener('webkitTransitionEnd', function() {
    node.parentNode.removeChild(node);
  });
}

/**
 * Removes a single entry from the view. Also removes gaps before and after
 * entry if necessary.
 */
function removeEntryFromView(entry) {
  var nextEntry = entry.nextSibling;
  var previousEntry = entry.previousSibling;

  removeNode(entry);

  // if there is no previous entry, and the next entry is a gap, remove it
  if (!previousEntry && nextEntry && nextEntry.className == 'gap') {
    removeNode(nextEntry);
  }

  // if there is no next entry, and the previous entry is a gap, remove it
  if (!nextEntry && previousEntry && previousEntry.className == 'gap') {
    removeNode(previousEntry);
  }

  // if both the next and previous entries are gaps, remove one
  if (nextEntry && nextEntry.className == 'gap' &&
      previousEntry && previousEntry.className == 'gap') {
    removeNode(nextEntry);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Chrome callbacks:
/**
 * Our history system calls this function with results from searches.
 */
function historyResult(info, results) {
  historyModel.addResults(info, results);
}

/**
 * Our history system calls this function when a deletion has finished.
 */
function deleteComplete() {
  if (deleteQueue.length > 0) {
    // Remove the successfully deleted entry from the queue.
    if (deleteQueue[0].callback)
      deleteQueue[0].callback.apply();
    deleteQueue.splice(0, 1);
    deleteNextInQueue();
  } else {
    console.error('Received deleteComplete but queue is empty.');
  }
}

/**
 * Our history system calls this function if a delete is not ready (e.g.
 * another delete is in-progress).
 */
function deleteFailed() {
  window.console.log('Delete failed');

  // The deletion failed - try again later.
  // TODO(dubroy): We should probably give up at some point.
  setTimeout(deleteNextInQueue, 500);
}

/**
 * Called when the history is deleted by someone else.
 */
function historyDeleted() {
  window.console.log('History deleted');
  var anyChecked = document.querySelector('.entry input:checked') != null;
  // Reload the page, unless the user has any items checked.
  // TODO(dubroy): We should just reload the page & restore the checked items.
  if (!anyChecked)
    historyView.reload();
}

// Add handlers to HTML elements.
document.addEventListener('DOMContentLoaded', load);

// This event lets us enable and disable menu items before the menu is shown.
document.addEventListener('canExecute', function(e) {
  e.canExecute = true;
});
