// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../uber/uber_utils.js">

///////////////////////////////////////////////////////////////////////////////
// Globals:
/** @const */ var RESULTS_PER_PAGE = 150;

// Amount of time between pageviews that we consider a 'break' in browsing,
// measured in milliseconds.
/** @const */ var BROWSING_GAP_TIME = 15 * 60 * 1000;

// TODO(glen): Get rid of these global references, replace with a controller
//     or just make the classes own more of the page.
var historyModel;
var historyView;
var pageState;
var deleteQueue = [];
var selectionAnchor = -1;
var activeVisit = null;

/** @const */ var Command = cr.ui.Command;
/** @const */ var Menu = cr.ui.Menu;
/** @const */ var MenuButton = cr.ui.MenuButton;

MenuButton.createDropDownArrows();

///////////////////////////////////////////////////////////////////////////////
// Visit:

/**
 * Class to hold all the information about an entry in our model.
 * @param {Object} result An object containing the visit's data.
 * @param {boolean} continued Whether this visit is on the same day as the
 *     visit before it.
 * @param {HistoryModel} model The model object this entry belongs to.
 * @param {number} id The identifier for the entry.
 * @constructor
 */
function Visit(result, continued, model, id) {
  this.model_ = model;
  this.title_ = result.title;
  this.url_ = result.url;
  this.starred_ = result.starred;
  this.snippet_ = result.snippet || '';
  this.id_ = id;

  this.changed = false;

  this.isRendered = false;

  // All the date information is public so that owners can compare properties of
  // two items easily.

  this.date = new Date(result.time);

  // See comment in BrowsingHistoryHandler::QueryComplete - we won't always
  // get all of these.
  this.dateRelativeDay = result.dateRelativeDay || '';
  this.dateTimeOfDay = result.dateTimeOfDay || '';
  this.dateShort = result.dateShort || '';

  // Whether this is the continuation of a previous day.
  this.continued = continued;
}

// Visit, public: -------------------------------------------------------------

/**
 * Returns a dom structure for a browse page result or a search page result.
 * @param {boolean} searchResultFlag Indicates whether the result is a search
 *     result or not.
 * @return {Node} A DOM node to represent the history entry or search result.
 */
Visit.prototype.getResultDOM = function(searchResultFlag) {
  var node = createElementWithClassName('li', 'entry');
  var time = createElementWithClassName('div', 'time');
  var entryBox = createElementWithClassName('label', 'entry-box');
  var domain = createElementWithClassName('div', 'domain');

  var dropDown = createElementWithClassName('button', 'drop-down');
  dropDown.value = 'Open action menu';
  dropDown.title = loadTimeData.getString('actionMenuDescription');
  dropDown.setAttribute('menu', '#action-menu');
  cr.ui.decorate(dropDown, MenuButton);

  // Checkbox is always created, but only visible on hover & when checked.
  var checkbox = document.createElement('input');
  checkbox.type = 'checkbox';
  checkbox.id = 'checkbox-' + this.id_;
  checkbox.time = this.date.getTime();
  checkbox.addEventListener('click', checkboxClicked);
  time.appendChild(checkbox);

  // Keep track of the drop down that triggered the menu, so we know
  // which element to apply the command to.
  // TODO(dubroy): Ideally we'd use 'activate', but MenuButton swallows it.
  var self = this;
  var setActiveVisit = function(e) {
    activeVisit = self;
  };
  dropDown.addEventListener('mousedown', setActiveVisit);
  dropDown.addEventListener('focus', setActiveVisit);

  domain.textContent = this.getDomainFromURL_(this.url_);

  // Clicking anywhere in the entryBox will check/uncheck the checkbox.
  entryBox.setAttribute('for', checkbox.id);
  entryBox.addEventListener('mousedown', entryBoxMousedown);

  // Prevent clicks on the drop down from affecting the checkbox.
  dropDown.addEventListener('click', function(e) { e.preventDefault(); });

  // We use a wrapper div so that the entry contents will be shinkwrapped.
  entryBox.appendChild(time);
  entryBox.appendChild(this.getTitleDOM_());
  entryBox.appendChild(domain);
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
    time.appendChild(document.createTextNode(this.dateShort));
    var snippet = createElementWithClassName('div', 'snippet');
    this.addHighlightedText_(snippet,
                             this.snippet_,
                             this.model_.getSearchText());
    node.appendChild(snippet);
  } else {
    time.appendChild(document.createTextNode(this.dateTimeOfDay));
  }

  this.domNode_ = node;

  return node;
};

// Visit, private: ------------------------------------------------------------

/**
 * Extracts and returns the domain (and subdomains) from a URL.
 * @param {string} url The url.
 * @return {string} The domain. An empty string is returned if no domain can
 *     be found.
 * @private
 */
Visit.prototype.getDomainFromURL_ = function(url) {
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
 * @private
 */
Visit.prototype.addHighlightedText_ = function(node, content, highlightText) {
  var i = 0;
  if (highlightText) {
    var re = new RegExp(Visit.pregQuote_(highlightText), 'gim');
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
 * @private
 */
Visit.prototype.getTitleDOM_ = function() {
  var node = createElementWithClassName('div', 'title');
  node.style.backgroundImage = getFaviconImageSet(this.url_);
  node.style.backgroundSize = '16px';

  var link = document.createElement('a');
  link.href = this.url_;
  link.id = 'id-' + this.id_;
  link.target = '_top';

  // Add a tooltip, since it might be ellipsized.
  // TODO(dubroy): Find a way to show the tooltip only when necessary.
  link.title = this.title_;

  this.addHighlightedText_(link, this.title_, this.model_.getSearchText());
  node.appendChild(link);

  if (this.starred_) {
    var star = createElementWithClassName('div', 'starred');
    node.appendChild(star);
    star.addEventListener('click', this.starClicked_.bind(this));
  }

  return node;
};

/**
 * Launch a search for more history entries from the same domain.
 * @private
 */
Visit.prototype.showMoreFromSite_ = function() {
  setSearch(this.getDomainFromURL_(this.url_));
};

/**
 * Remove a single entry from the history.
 * @private
 */
Visit.prototype.removeFromHistory_ = function() {
  var self = this;
  var onSuccessCallback = function() {
    removeEntryFromView(self.domNode_);
  };
  queueURLsForDeletion(this.date, [this.url_], onSuccessCallback);
  deleteNextInQueue();
};

/**
 * Click event handler for the star icon that appears beside bookmarked URLs.
 * When clicked, the bookmark is removed for that URL.
 * @param {Event} event The click event.
 * @private
 */
Visit.prototype.starClicked_ = function(event) {
  chrome.send('removeBookmark', [this.url_]);
  event.currentTarget.hidden = true;
  event.preventDefault();
};

// Visit, private, static: ----------------------------------------------------

/**
 * Quote a string so it can be used in a regular expression.
 * @param {string} str The source string
 * @return {string} The escaped string
 * @private
 */
Visit.pregQuote_ = function(str) {
  return str.replace(/([\\\.\+\*\?\[\^\]\$\(\)\{\}\=\!\<\>\|\:])/g, '\\$1');
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
 *
 * @constructor
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
 * @param {string} searchText The text to search for
 * @param {number} opt_page The page to view - this is mostly used when setting
 *     up an initial view, use #requestPage otherwise.
 */
HistoryModel.prototype.setSearchText = function(searchText, opt_page) {
  this.clearModel_();
  this.searchText_ = searchText;
  this.requestedPage_ = opt_page ? opt_page : 0;
  this.queryHistory_();
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
  this.queryHistory_();
};

/**
 * @return {string} The current search text.
 */
HistoryModel.prototype.getSearchText = function() {
  return this.searchText_;
};

/**
 * Tell the model that the view will want to see the current page. When
 * the data becomes available, the model will call the view back.
 * @param {number} page The page we want to view.
 */
HistoryModel.prototype.requestPage = function(page) {
  this.requestedPage_ = page;
  this.changed = true;
  this.updateSearch_();
};

/**
 * Receiver for history query.
 * @param {Object} info An object containing information about the query.
 * @param {Array} results A list of results.
 */
HistoryModel.prototype.addResults = function(info, results) {
  $('loading-spinner').hidden = true;
  this.inFlight_ = false;
  this.isQueryFinished_ = info.finished;
  this.queryCursor_ = info.cursor;

  // If there are no results, or they're not for the current search term,
  // there's nothing more to do.
  if (!results || !results.length || info.term != this.searchText_)
    return;

  // If necessary, sort the results from newest to oldest.
  if (!results.sorted)
    results.sort(function(a, b) { return b.time - a.time; });

  var lastVisit = this.visits_.slice(-1)[0];
  var lastDay = lastVisit ? lastVisit.dateRelativeDay : null;

  for (var i = 0, thisResult; thisResult = results[i]; i++) {
    var thisDay = thisResult.dateRelativeDay;
    var isSameDay = lastDay == thisDay;

    // Keep track of all URLs seen on a particular day, and only use the
    // latest visit from that day.
    if (!isSameDay)
      this.urlsFromLastSeenDay_ = {};
    else if (thisResult.url in this.urlsFromLastSeenDay_)
      continue;
    this.urlsFromLastSeenDay_[thisResult.url] = thisResult.time;

    this.visits_.push(new Visit(thisResult, isSameDay, this, this.last_id_++));
    this.changed = true;
    lastDay = thisDay;
  }

  this.updateSearch_();
};

/**
 * @return {number} The number of visits in the model.
 */
HistoryModel.prototype.getSize = function() {
  return this.visits_.length;
};

/**
 * Get a list of visits between specified index positions.
 * @param {number} start The start index
 * @param {number} end The end index
 * @return {Array.<Visit>} A list of visits
 */
HistoryModel.prototype.getNumberedRange = function(start, end) {
  return this.visits_.slice(start, end);
};

/**
 * Return true if there are more results beyond the current page.
 * @return {boolean} true if the there are more results, otherwise false.
 */
HistoryModel.prototype.hasMoreResults = function() {
  return this.haveDataForPage_(this.requestedPage_ + 1) ||
      !this.isQueryFinished_;
};

// HistoryModel, Private: -----------------------------------------------------

/**
 * Clear the history model.
 * @private
 */
HistoryModel.prototype.clearModel_ = function() {
  this.inFlight_ = false;  // Whether a query is inflight.
  this.searchText_ = '';

  this.visits_ = [];  // Date-sorted list of visits (most recent first).
  this.last_id_ = 0;
  selectionAnchor = -1;

  // The page that the view wants to see - we only fetch slightly past this
  // point. If the view requests a page that we don't have data for, we try
  // to fetch it and call back when we're done.
  this.requestedPage_ = 0;

  // Keeps track of whether or not there are more results available than are
  // currently held in |this.visits_|.
  this.isQueryFinished_ = false;

  // An opaque value that is returned with the query results. This is used to
  // fetch the next page of results for a query.
  this.queryCursor_ = null;

  // A map of URLs of visits on the same day as the last known visit.
  // This is used for de-duping URLs, so that we only show the most recent
  // visit to a URL on any day.
  this.urlsFromLastSeenDay_ = {};

  if (this.view_)
    this.view_.clear_();
};

/**
 * Figure out if we need to do more queries to fill the currently requested
 * page. If we think we can fill the page, call the view and let it know
 * we're ready to show something.
 * @private
 */
HistoryModel.prototype.updateSearch_ = function() {
  var doneLoading =
      this.canFillPage_(this.requestedPage_) || this.isQueryFinished_;

  // Try to fetch more results if the current page isn't full.
  if (!doneLoading && !this.inFlight_)
    this.queryHistory_();

  // If we have any data for the requested page, show it.
  if (this.changed && this.haveDataForPage_(this.requestedPage_)) {
    this.view_.onModelReady();
    this.changed = false;
  }
};

/**
 * Query for history, either for a search or time-based browsing.
 * @private
 */
HistoryModel.prototype.queryHistory_ = function() {
  var endTime = 0;

  // If there are already some visits, pick up the previous query where it
  // left off.
  if (this.visits_.length > 0) {
    var lastVisit = this.visits_.slice(-1)[0];
    endTime = lastVisit.date.getTime();
    cursor = this.queryCursor_;
  }

  $('loading-spinner').hidden = false;
  this.inFlight_ = true;
  chrome.send('queryHistory',
      [this.searchText_, endTime, this.queryCursor_, RESULTS_PER_PAGE]);
};

/**
 * Check to see if we have data for the given page.
 * @param {number} page The page number.
 * @return {boolean} Whether we have any data for the given page.
 * @private
 */
HistoryModel.prototype.haveDataForPage_ = function(page) {
  return (page * RESULTS_PER_PAGE < this.getSize());
};

/**
 * Check to see if we have data to fill the given page.
 * @param {number} page The page number.
 * @return {boolean} Whether we have data to fill the page.
 * @private
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
 * @constructor
 */
function HistoryView(model) {
  this.editButtonTd_ = $('edit-button');
  this.editingControlsDiv_ = $('editing-controls');
  this.resultDiv_ = $('results-display');
  this.pageDiv_ = $('results-pagination');
  this.model_ = model;
  this.pageIndex_ = 0;
  this.lastDisplayed_ = [];

  this.model_.setView(this);

  this.currentVisits_ = [];

  var self = this;

  $('clear-browsing-data').addEventListener('click', openClearBrowsingData);
  $('remove-selected').addEventListener('click', removeItems);

  // Add handlers for the page navigation buttons at the bottom.
  $('newest-button').addEventListener('click', function() {
    self.setPage(0);
  });
  $('newer-button').addEventListener('click', function() {
    self.setPage(self.pageIndex_ - 1);
  });
  $('older-button').addEventListener('click', function() {
    self.setPage(self.pageIndex_ + 1);
  });
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
  this.updateNavBar_();
};

/**
 * Enables or disables the 'Remove selected items' button as appropriate.
 */
HistoryView.prototype.updateRemoveButton = function() {
  var anyChecked = document.querySelector('.entry input:checked') != null;
  $('remove-selected').disabled = !anyChecked;
};

// HistoryView, private: ------------------------------------------------------

/**
 * Clear the results in the view.  Since we add results piecemeal, we need
 * to clear them out when we switch to a new page or reload.
 * @private
 */
HistoryView.prototype.clear_ = function() {
  this.resultDiv_.textContent = '';

  this.currentVisits_.forEach(function(visit) {
    visit.isRendered = false;
  });
  this.currentVisits_ = [];
};

/**
 * Record that the given visit has been rendered.
 * @param {Visit} visit The visit that was rendered.
 * @private
 */
HistoryView.prototype.setVisitRendered_ = function(visit) {
  visit.isRendered = true;
  this.currentVisits_.push(visit);
};

/**
 * Update the page with results.
 * @private
 */
HistoryView.prototype.displayResults_ = function() {
  var rangeStart = this.pageIndex_ * RESULTS_PER_PAGE;
  var rangeEnd = rangeStart + RESULTS_PER_PAGE;
  var results = this.model_.getNumberedRange(rangeStart, rangeEnd);

  var searchText = this.model_.getSearchText();
  if (searchText) {
    // Add a header for the search results, if there isn't already one.
    if (!this.resultDiv_.querySelector('h3')) {
      var header = document.createElement('h3');
      header.textContent = loadTimeData.getStringF('searchresultsfor',
                                                   searchText);
      this.resultDiv_.appendChild(header);
    }

    var searchResults = createElementWithClassName('ol', 'search-results');
    if (results.length == 0) {
      var noResults = document.createElement('div');
      noResults.textContent = loadTimeData.getString('noresults');
      searchResults.appendChild(noResults);
    } else {
      for (var i = 0, visit; visit = results[i]; i++) {
        if (!visit.isRendered) {
          searchResults.appendChild(visit.getResultDOM(true));
          this.setVisitRendered_(visit);
        }
      }
    }
    this.resultDiv_.appendChild(searchResults);
  } else {
    var resultsFragment = document.createDocumentFragment();
    var lastTime = Math.infinity;
    var dayResults;

    for (var i = 0, visit; visit = results[i]; i++) {
      if (visit.isRendered)
        continue;

      var thisTime = visit.date.getTime();

      // Break across day boundaries and insert gaps for browsing pauses.
      // Create a dayResults element to contain results for each day.
      if ((i == 0 && visit.continued) || !visit.continued) {
        // It's the first visit of the day, or the day is continued from
        // the previous page. Create a header for the day on the current page.
        var day = createElementWithClassName('h3', 'day');
        day.appendChild(document.createTextNode(visit.dateRelativeDay));
        if (visit.continued) {
          day.appendChild(document.createTextNode(' ' +
              loadTimeData.getString('cont')));
        }

        resultsFragment.appendChild(day);
        dayResults = createElementWithClassName('ol', 'day-results');
        resultsFragment.appendChild(dayResults);
      } else if (dayResults && lastTime - thisTime > BROWSING_GAP_TIME) {
        dayResults.appendChild(createElementWithClassName('li', 'gap'));
      }
      lastTime = thisTime;

      // Add the entry to the appropriate day.
      dayResults.appendChild(visit.getResultDOM(false));
      this.setVisitRendered_(visit);
    }
    this.resultDiv_.appendChild(resultsFragment);
  }
};

/**
 * Update the visibility of the page navigation buttons.
 * @private
 */
HistoryView.prototype.updateNavBar_ = function() {
  $('newest-button').hidden = this.pageIndex_ == 0;
  $('newer-button').hidden = this.pageIndex_ == 0;
  $('older-button').hidden = !this.model_.hasMoreResults();
};

///////////////////////////////////////////////////////////////////////////////
// State object:
/**
 * An 'AJAX-history' implementation.
 * @param {HistoryModel} model The model we're representing.
 * @param {HistoryView} view The view we're representing.
 * @constructor
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
    if (hashData.q != state_obj.model.getSearchText()) {
      state_obj.view.setSearch(hashData.q, parseInt(hashData.p, 10));
    } else if (parseInt(hashData.p, 10) != state_obj.view.getPage()) {
      state_obj.view.setPage(hashData.p);
    }
  }), 50, this);
}

/**
 * Holds the singleton instance.
 */
PageState.instance = null;

/**
 * @return {Object} An object containing parameters from our window hash.
 */
PageState.prototype.getHashData = function() {
  var result = {
    e: 0,
    q: '',
    p: 0
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
  $('search-field').value = term;
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
  uber.onContentFrameLoaded();

  var searchField = $('search-field');
  searchField.focus();

  historyModel = new HistoryModel();
  historyView = new HistoryView(historyModel);
  pageState = new PageState(historyModel, historyView);

  // Create default view.
  var hashData = pageState.getHashData();
  historyView.setSearch(hashData.q, hashData.p);

  $('search-form').onsubmit = function() {
    setSearch(searchField.value);
    return false;
  };

  $('remove-visit').addEventListener('activate', function(e) {
    activeVisit.removeFromHistory_();
    activeVisit = null;
  });
  $('more-from-site').addEventListener('activate', function(e) {
    activeVisit.showMoreFromSite_();
    activeVisit = null;
  });

  var title = loadTimeData.getString('title');
  uber.invokeMethodOnParent('setTitle', {title: title});

  window.addEventListener('message', function(e) {
    if (e.data.method == 'frameSelected')
      searchField.focus();
  });
}

/**
 * TODO(glen): Get rid of this function.
 * Set the history view to a specified page.
 * @param {string} term The string to search for
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
    // First arg is a time (in ms since the epoch) identifying the day.
    // Remaining args are URLs of history entries from that day to delete.
    chrome.send('removeURLsOnOneDay',
                [deleteQueue[0].date.getTime()].concat(deleteQueue[0].urls));
  }
}

/**
 * Click handler for the 'Clear browsing data' dialog.
 * @param {Event} e The click event.
 */
function openClearBrowsingData(e) {
  chrome.send('clearBrowsingData');
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
 * Click handler for the 'Remove selected items' button.
 * Collect IDs from the checked checkboxes and send to Chrome for deletion.
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
    var link = findAncestorByClass(checkbox, 'entry-box').querySelector('a');
    checkbox.disabled = true;
    link.classList.add('to-be-removed');
    disabledItems.push(checkbox);
    urls.push(link.href);
  }
  if (urls.length > 0) {
    queue.push([date, urls]);
  }
  if (checked.length > 0 && confirm(loadTimeData.getString('deletewarning'))) {
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
      var link = findAncestorByClass(checkbox, 'entry-box').querySelector('a');
      checkbox.disabled = false;
      link.classList.remove('to-be-removed');
    }
  }
}

/**
 * Handler for the 'click' event on a checkbox.
 * @param {Event} e The click event.
 */
function checkboxClicked(e) {
  var checkbox = e.currentTarget;
  var id = Number(checkbox.id.slice('checkbox-'.length));
  // Handle multi-select if shift was pressed.
  if (event.shiftKey && (selectionAnchor != -1)) {
    var checked = checkbox.checked;
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
 * @param {Node} entry The DOM node representing the entry to be removed.
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
 * @param {Object} info An object containing information about the query.
 * @param {Array} results A list of results.
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
