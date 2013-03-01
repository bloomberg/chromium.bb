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

/**
 * Enum that shows whether a manual exception is set in managed mode for a
 * host or URL.
 * Must behave like the ManualBehavior enum from managed_user_service.h.
 * @enum {number}
 */
ManagedModeManualBehavior = {
  NONE: 0,
  ALLOW: 1,
  BLOCK: 2
};

MenuButton.createDropDownArrows();

///////////////////////////////////////////////////////////////////////////////
// Visit:

/**
 * Class to hold all the information about an entry in our model.
 * @param {Object} result An object containing the visit's data.
 * @param {boolean} continued Whether this visit is on the same day as the
 *     visit before it.
 * @param {HistoryModel} model The model object this entry belongs to.
 * @constructor
 */
function Visit(result, continued, model) {
  this.model_ = model;
  this.title_ = result.title;
  this.url_ = result.url;
  this.starred_ = result.starred;
  this.snippet_ = result.snippet || '';
  // The id will be set according to when the visit was displayed, not
  // received. Set to -1 to show that it has not been set yet.
  this.id_ = -1;

  this.isRendered = false;  // Has the visit already been rendered on the page?

  // Holds the timestamps of duplicates of this visit (visits to the same URL on
  // the same day).
  this.duplicateTimestamps_ = [];

  // All the date information is public so that owners can compare properties of
  // two items easily.

  this.date = new Date(result.time);

  // See comment in BrowsingHistoryHandler::QueryComplete - we won't always
  // get all of these.
  this.dateRelativeDay = result.dateRelativeDay || '';
  this.dateTimeOfDay = result.dateTimeOfDay || '';
  this.dateShort = result.dateShort || '';

  // These values represent indicators shown to users in managed mode.
  // |*manualBehavior| shows whether the user has manually added an exception
  // for that URL or host while |*inContentPack| shows whether that URL or host
  // is in a content pack or not.
  this.urlManualBehavior = result.urlManualBehavior ||
                           ManagedModeManualBehavior.NONE;
  this.hostManualBehavior = result.hostManualBehavior ||
                            ManagedModeManualBehavior.NONE;
  this.urlInContentPack = result.urlInContentPack || false;
  this.hostInContentPack = result.hostInContentPack || false;

  // Whether this is the continuation of a previous day.
  this.continued = continued;
}

// Visit, public: -------------------------------------------------------------

/**
 * Records the timestamp of another visit to the same URL as this visit.
 * @param {Number} timestamp The timestamp to add.
 */
Visit.prototype.addDuplicateTimestamp = function(timestamp) {
  this.duplicateTimestamps_.push(timestamp);
};

/**
 * Returns a dom structure for a browse page result or a search page result.
 * @param {Object} propertyBag A bag of configuration properties, false by
 * default:
 *  - isSearchResult: Whether or not the result is a search result.
 *  - addTitleFavicon: Whether or not the favicon should be added.
 *  - useMonthDate: Whether or not the full date should be inserted (used for
 * monthly view).
 * @return {Node} A DOM node to represent the history entry or search result.
 */
Visit.prototype.getResultDOM = function(propertyBag) {
  var isSearchResult = propertyBag.isSearchResult || false;
  var addTitleFavicon = propertyBag.addTitleFavicon || false;
  var useMonthDate = propertyBag.useMonthDate || false;
  var node = createElementWithClassName('li', 'entry');
  var time = createElementWithClassName('div', 'time');
  var entryBox = createElementWithClassName('label', 'entry-box');
  var domain = createElementWithClassName('div', 'domain');

  var dropDown = createElementWithClassName('button', 'drop-down');
  dropDown.value = 'Open action menu';
  dropDown.title = loadTimeData.getString('actionMenuDescription');
  dropDown.setAttribute('menu', '#action-menu');
  cr.ui.decorate(dropDown, MenuButton);

  this.id_ = this.model_.nextVisitId_++;

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

  // We use a wrapper div so that the entry contents will be shrinkwrapped.
  entryBox.appendChild(time);
  entryBox.appendChild(this.getTitleDOM_(addTitleFavicon));
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
  if (this.model_.isManagedProfile && this.model_.getGroupByDomain()) {
    entryBox.appendChild(
        getManagedStatusDOM(this.urlManualBehavior, this.urlInContentPack));
  }

  if (isSearchResult) {
    time.appendChild(document.createTextNode(this.dateShort));
    var snippet = createElementWithClassName('div', 'snippet');
    this.addHighlightedText_(snippet,
                             this.snippet_,
                             this.model_.getSearchText());
    node.appendChild(snippet);
  } else if (useMonthDate) {
    // Show the day instead of the time.
    time.appendChild(document.createTextNode(this.dateShort));
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
  // TODO(sergiu): Extract the domain from the C++ side and send it here.
  var domain = url.replace(/^.+?:\/\//, '').match(/[^/]+/);
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
 * Returns the DOM element containing a link on the title of the URL for the
 * current visit. Optionally sets the favicon as well.
 * @param {boolean} addFavicon Whether to add a favicon or not.
 * @return {Element} DOM representation for the title block.
 * @private
 */
Visit.prototype.getTitleDOM_ = function(addFavicon) {
  var node = createElementWithClassName('div', 'title');
  if (addFavicon) {
    node.style.backgroundImage = getFaviconImageSet(this.url_);
    node.style.backgroundSize = '16px';
  }

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
 * Set the favicon for an element.
 * @param {Element} el The DOM element to which to add the icon.
 * @private
 */
Visit.prototype.addFaviconToElement_ = function(el) {
  el.style.backgroundImage = getFaviconImageSet(this.url_);
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

/** @enum {number} */
HistoryModel.Range = {
  ALL_TIME: 0,
  WEEK: 1,
  MONTH: 2
};

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
 * Clear the search text.
 */
HistoryModel.prototype.clearSearchText = function() {
  this.searchText_ = '';
};

/**
 * Reload our model with the current parameters.
 */
HistoryModel.prototype.reload = function() {
  // Save user-visible state, clear the model, and restore the state.
  var search = this.searchText_;
  var page = this.requestedPage_;
  var range = this.rangeInDays_;
  var offset = this.offset_;
  var groupByDomain = this.groupByDomain_;

  this.clearModel_();
  this.searchText_ = search;
  this.requestedPage_ = page;
  this.rangeInDays_ = range;
  this.offset_ = offset;
  this.groupByDomain_ = groupByDomain;
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
  this.queryStartTime = info.queryStartTime;
  this.queryEndTime = info.queryEndTime;

  // If the results are not for the current search term then there is nothing
  // more to do.
  if (info.term != this.searchText_)
    return;

  var lastVisit = this.visits_.slice(-1)[0];
  var lastDay = lastVisit ? lastVisit.dateRelativeDay : null;

  for (var i = 0, thisResult; thisResult = results[i]; i++) {
    var thisDay = thisResult.dateRelativeDay;
    var isSameDay = lastDay == thisDay;
    this.visits_.push(new Visit(thisResult, isSameDay, this));
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

// Getter and setter for HistoryModel.rangeInDays_.
Object.defineProperty(HistoryModel.prototype, 'rangeInDays', {
  get: function() {
    return this.rangeInDays_;
  },
  set: function(range) {
    this.rangeInDays_ = range;
  }
});

/**
 * Getter and setter for HistoryModel.offset_. The offset moves the current
 * query 'window' |range| days behind. As such for range set to WEEK an offset
 * of 0 refers to the last 7 days, an offset of 1 refers to the 7 day period
 * that ended 7 days ago, etc. For MONTH an offset of 0 refers to the current
 * calendar month, 1 to the previous one, etc.
 */
Object.defineProperty(HistoryModel.prototype, 'offset', {
  get: function() {
    return this.offset_;
  },
  set: function(offset) {
    this.offset_ = offset;
  }
});

// Setter for HistoryModel.requestedPage_.
Object.defineProperty(HistoryModel.prototype, 'requestedPage', {
  set: function(page) {
    this.requestedPage_ = page;
  }
});

// HistoryModel, Private: -----------------------------------------------------

/**
 * Clear the history model.
 * @private
 */
HistoryModel.prototype.clearModel_ = function() {
  this.inFlight_ = false;  // Whether a query is inflight.
  this.searchText_ = '';
  // Flag to show that the results are grouped by domain or not.
  this.groupByDomain_ = false;

  this.visits_ = [];  // Date-sorted list of visits (most recent first).
  this.nextVisitId_ = 0;
  selectionAnchor = -1;

  // The page that the view wants to see - we only fetch slightly past this
  // point. If the view requests a page that we don't have data for, we try
  // to fetch it and call back when we're done.
  this.requestedPage_ = 0;

  // The range of history to view or search over.
  this.rangeInDays_ = HistoryModel.Range.ALL_TIME;

  // Skip |offset_| * weeks/months from the begining.
  this.offset_ = 0;

  // Keeps track of whether or not there are more results available than are
  // currently held in |this.visits_|.
  this.isQueryFinished_ = false;

  // Whether this user is a managed user.
  this.isManagedProfile = loadTimeData.getBoolean('isManagedProfile');

  if (this.view_)
    this.view_.clear_();
};

/**
 * Figure out if we need to do more queries to fill the currently requested
 * page. If we think we can fill the page, call the view and let it know
 * we're ready to show something. This only applies to the daily time-based
 * view.
 * @private
 */
HistoryModel.prototype.updateSearch_ = function() {
  var doneLoading = this.rangeInDays_ != HistoryModel.Range.ALL_TIME ||
                    this.isQueryFinished_ ||
                    this.canFillPage_(this.requestedPage_);

  // Try to fetch more results if more results can arrive and the page is not
  // full.
  if (!doneLoading && !this.inFlight_)
    this.queryHistory_();

  // Show the result or a message if no results were returned.
  this.view_.onModelReady(doneLoading);
};

/**
 * Query for history, either for a search or time-based browsing.
 * @private
 */
HistoryModel.prototype.queryHistory_ = function() {
  var maxResults =
      (this.rangeInDays_ == HistoryModel.Range.ALL_TIME) ? RESULTS_PER_PAGE : 0;

  // If there are already some visits, pick up the previous query where it
  // left off.
  var lastVisit = this.visits_.slice(-1)[0];
  var endTime = lastVisit ? lastVisit.date.getTime() : 0;

  $('loading-spinner').hidden = false;
  this.inFlight_ = true;
  chrome.send('queryHistory',
      [this.searchText_, this.offset_, this.rangeInDays_, endTime, maxResults]);
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

/**
 * Enables or disables grouping by domain.
 * @param {boolean} groupByDomain New groupByDomain_ value.
 */
HistoryModel.prototype.setGroupByDomain = function(groupByDomain) {
  this.groupByDomain_ = groupByDomain;
  this.offset_ = 0;
};

/**
 * Gets whether we are grouped by domain.
 * @return {boolean} Whether the results are grouped by domain.
 */
HistoryModel.prototype.getGroupByDomain = function() {
  return this.groupByDomain_;
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

  $('allow-selected').addEventListener('click', function(e) {
    processManagedList(true);
  });

  $('block-selected').addEventListener('click', function(e) {
    processManagedList(false);
  });

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

  // Add handlers for the range options.
  $('timeframe-filter').addEventListener('change', function(e) {
    self.setRangeInDays(parseInt(e.target.value, 10));
  });

  $('display-filter-sites').addEventListener('click', function(e) {
    self.setGroupByDomain($('display-filter-sites').checked);
  });

  $('range-previous').addEventListener('click', function(e) {
    if (self.getRangeInDays() == HistoryModel.Range.ALL_TIME)
      self.setPage(self.pageIndex_ + 1);
    else
      self.setOffset(self.getOffset() + 1);
  });
  $('range-next').addEventListener('click', function(e) {
    if (self.getRangeInDays() == HistoryModel.Range.ALL_TIME)
      self.setPage(self.pageIndex_ - 1);
    else
      self.setOffset(self.getOffset() - 1);
  });
  $('range-today').addEventListener('click', function(e) {
    if (self.getRangeInDays() == HistoryModel.Range.ALL_TIME)
      self.setPage(0);
    else
      self.setOffset(0);
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
  pageState.setUIState(term, this.pageIndex_, this.model_.getGroupByDomain(),
                       this.getRangeInDays(), this.getOffset());
};

/**
 * Enable or disable results as being grouped by domain.
 * @param {boolean} groupedByDomain Whether to group by domain or not.
 */
HistoryView.prototype.setGroupByDomain = function(groupedByDomain) {
  // Group by domain is not currently supported for search results, so reset
  // the search term if there was one.
  this.model_.clearSearchText();
  this.model_.setGroupByDomain(groupedByDomain);
  this.model_.reload();
  pageState.setUIState(this.model_.getSearchText(),
                       this.pageIndex_,
                       this.model_.getGroupByDomain(),
                       this.getRangeInDays(),
                       this.getOffset());
};

/**
 * Reload the current view.
 */
HistoryView.prototype.reload = function() {
  this.model_.reload();
  this.updateSelectionEditButtons();
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
  pageState.setUIState(this.model_.getSearchText(),
                       this.pageIndex_,
                       this.model_.getGroupByDomain(),
                       this.getRangeInDays(),
                       this.getOffset());
};

/**
 * @return {number} The page number being viewed.
 */
HistoryView.prototype.getPage = function() {
  return this.pageIndex_;
};

/**
 * Set the current range for grouped results.
 * @param {string} range The number of days to which the range should be set.
 */
HistoryView.prototype.setRangeInDays = function(range) {
  // Set the range, offset and reset the page
  this.model_.rangeInDays = range;
  this.model_.offset = 0;
  this.pageIndex_ = 0;
  if (range == HistoryModel.Range.ALL_TIME)
    this.model_.requestedPage = 0;
  this.model_.reload();
  pageState.setUIState(this.model_.getSearchText(), this.pageIndex_,
      this.model_.getGroupByDomain(), range, this.getOffset());
};

/**
 * Get the current range in days.
 * @return {number} Current range in days from the model.
 */
HistoryView.prototype.getRangeInDays = function() {
  return this.model_.rangeInDays;
};

/**
 * Set the current offset for grouped results.
 * @param {number} offset Offset to set.
 */
HistoryView.prototype.setOffset = function(offset) {
  // If there is another query already in flight wait for that to complete.
  if (this.model_.inFlight_)
    return;
  this.model_.offset = offset;
  this.model_.reload();
  pageState.setUIState(this.model_.getSearchText(),
                       this.pageIndex_,
                       this.model_.getGroupByDomain(),
                       this.getRangeInDays(),
                       this.getOffset());
};

/**
 * Get the current offset.
 * @return {number} Current offset from the model.
 */
HistoryView.prototype.getOffset = function() {
  return this.model_.offset;
};

/**
 * Callback for the history model to let it know that it has data ready for us
 * to view.
 * @param {boolean} doneLoading Whether the current request is complete.
 */
HistoryView.prototype.onModelReady = function(doneLoading) {
  this.displayResults_(doneLoading);
  this.updateNavBar_();
};

/**
 * Enables or disables the buttons that control editing entries depending on
 * whether there are any checked boxes.
 */
HistoryView.prototype.updateSelectionEditButtons = function() {
  var anyChecked = document.querySelector('.entry input:checked') != null;
  $('remove-selected').disabled = !anyChecked;
  $('allow-selected').disabled = !anyChecked;
  $('block-selected').disabled = !anyChecked;
};

/**
 * Callback triggered by the backend after the manual allow or block changes
 * have been commited. Once the changes are commited the backend builds an
 * updated set of data which contains the new managed mode status and passes
 * it through this function to the client. The function takes that data and
 * updates the individiual host/URL elements with their new managed mode status.
 * @param {Array} entries List of two dictionaries which contain the updated
 *     information for both the hosts and the URLs.
 */
HistoryView.prototype.updateManagedEntries = function(entries) {
  // |hostEntries| and |urlEntries| are dictionaries which have hosts and URLs
  // as keys and dictionaries with two values (|inContentPack| and
  // |manualBehavior|) as values.
  var hostEntries = entries[0];
  var urlEntries = entries[1];
  var hostElements = document.querySelectorAll('.site-domain');
  for (var i = 0; i < hostElements.length; i++) {
    var host = hostElements[i].firstChild.textContent;
    var siteDomainWrapperDiv = hostElements[i].parentNode;
    siteDomainWrapperDiv.querySelector('input[type=checkbox]').checked = false;
    var filterStatusDiv = hostElements[i].nextSibling;
    if (host in hostEntries)
      updateHostStatus(filterStatusDiv, hostEntries[host]);
  }

  var urlElements = document.querySelectorAll('.entry-box .title');
  for (var i = 0; i < urlElements.length; i++) {
    var url = urlElements[i].querySelector('a').href;
    var entry = findAncestorByClass(urlElements[i], 'entry');
    var filterStatusDiv = entry.querySelector('.filter-status');
    entry.querySelector('input[type=checkbox]').checked = false;
    if (url in urlEntries)
      updateHostStatus(filterStatusDiv, urlEntries[url]);
  }
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
 * Generates and adds the grouped visits DOM for a certain domain. This
 * includes the clickable arrow and domain name and the visit entries for
 * that domain.
 * @param {Element} results DOM object to which to add the elements.
 * @param {string} domain Current domain name.
 * @param {Array} domainVisits Array of visits for this domain.
 * @private
 */
HistoryView.prototype.getGroupedVisitsDOM_ = function(
    results, domain, domainVisits) {
  // Add a new domain entry.
  var siteResults = results.appendChild(
      createElementWithClassName('li', 'site-entry'));
  var siteDomainCheckbox =
      createElementWithClassName('input', 'domain-checkbox');
  siteDomainCheckbox.type = 'checkbox';
  siteDomainCheckbox.addEventListener('click', domainCheckboxClicked);
  siteDomainCheckbox.domain_ = domain;
  // Make a wrapper that will contain the arrow, the favicon and the domain.
  var siteDomainWrapper = siteResults.appendChild(
      createElementWithClassName('div', 'site-domain-wrapper'));
  siteDomainWrapper.appendChild(siteDomainCheckbox);
  var siteArrow = siteDomainWrapper.appendChild(
      createElementWithClassName('div', 'site-domain-arrow collapse'));
  var siteDomain = siteDomainWrapper.appendChild(
      createElementWithClassName('div', 'site-domain'));
  var siteDomainLink = siteDomain.appendChild(
      createElementWithClassName('button', 'link-button'));
  siteDomainLink.addEventListener('click', function(e) { e.preventDefault(); });
  siteDomainLink.textContent = domain;
  var numberOfVisits = createElementWithClassName('span', 'number-visits');
  var domainElement = document.createElement('span');

  numberOfVisits.textContent = loadTimeData.getStringF('numbervisits',
                                                       domainVisits.length);
  siteDomain.appendChild(numberOfVisits);

  domainVisits[0].addFaviconToElement_(siteDomain);

  siteDomainWrapper.addEventListener('click', toggleHandler);

  if (this.model_.isManagedProfile) {
    siteDomainWrapper.appendChild(getManagedStatusDOM(
        domainVisits[0].hostManualBehavior, domainVisits[0].hostInContentPack));
  }

  siteResults.appendChild(siteDomainWrapper);
  var resultsList = siteResults.appendChild(
      createElementWithClassName('ol', 'site-results'));

  // Collapse until it gets toggled.
  resultsList.style.height = 0;

  // Add the results for each of the domain.
  var isMonthGroupedResult = this.getRangeInDays() == HistoryModel.Range.MONTH;
  for (var j = 0, visit; visit = domainVisits[j]; j++) {
    resultsList.appendChild(visit.getResultDOM({
      useMonthDate: isMonthGroupedResult
    }));
    this.setVisitRendered_(visit);
  }
};

/**
 * Enables or disables the time range buttons.
 * @private
 */
HistoryView.prototype.updateRangeButtons_ = function() {
  // The enabled state for the previous, today and next buttons.
  var previousState = false;
  var todayState = false;
  var nextState = false;
  var usePage = (this.getRangeInDays() == HistoryModel.Range.ALL_TIME);

  // Use pagination for most recent visits, offset otherwise.
  // TODO(sergiu): Maybe send just one variable in the future.
  if (usePage) {
    if (this.getPage() != 0) {
      nextState = true;
      todayState = true;
    }
    previousState = this.model_.hasMoreResults();
  } else {
    if (this.getOffset() != 0) {
      nextState = true;
      todayState = true;
    }
    previousState = !this.model_.isQueryFinished_;
  }

  $('range-previous').disabled = !previousState;
  $('range-today').disabled = !todayState;
  $('range-next').disabled = !nextState;
};

/**
 * Groups visits by domain, sorting them by the number of visits.
 * @param {Array} visits Visits received from the query results.
 * @param {Element} results Object where the results are added to.
 * @private
 */
HistoryView.prototype.groupVisitsByDomain_ = function(visits, results) {
  var visitsByDomain = {};
  var domains = [];

  // Group the visits into a dictionary and generate a list of domains.
  for (var i = 0, visit; visit = visits[i]; i++) {
    var domain = visit.getDomainFromURL_(visit.url_);
    if (!visitsByDomain[domain]) {
      visitsByDomain[domain] = [];
      domains.push(domain);
    }
    visitsByDomain[domain].push(visit);
  }
  var sortByVisits = function(a, b) {
    return visitsByDomain[b].length - visitsByDomain[a].length;
  };
  domains.sort(sortByVisits);

  for (var i = 0, domain; domain = domains[i]; i++)
    this.getGroupedVisitsDOM_(results, domain, visitsByDomain[domain]);
};

/**
 * Adds the results for a month.
 * @param {Array} visits Visits returned by the query.
 * @param {Element} parentElement Element to which to add the results to.
 * @private
 */
HistoryView.prototype.addMonthResults_ = function(visits, parentElement) {
  if (visits.length == 0)
    return;

  var monthResults = parentElement.appendChild(
      createElementWithClassName('ol', 'month-results'));
  this.groupVisitsByDomain_(visits, monthResults);
};

/**
 * Adds the results for a certain day. This includes a title with the day of
 * the results and the results themselves, grouped or not.
 * @param {Array} visits Visits returned by the query.
 * @param {Element} parentElement Element to which to add the results to.
 * @private
 */
HistoryView.prototype.addDayResults_ = function(visits, parentElement) {
  if (visits.length == 0)
    return;

  var firstVisit = visits[0];
  var day = parentElement.appendChild(createElementWithClassName('h3', 'day'));
  day.appendChild(document.createTextNode(firstVisit.dateRelativeDay));
  if (firstVisit.continued) {
    day.appendChild(document.createTextNode(' ' +
                                            loadTimeData.getString('cont')));
  }
  var dayResults = parentElement.appendChild(
      createElementWithClassName('ol', 'day-results'));

  if (this.model_.getGroupByDomain()) {
    this.groupVisitsByDomain_(visits, dayResults);
  } else {
    var lastTime;

    for (var i = 0, visit; visit = visits[i]; i++) {
      // If enough time has passed between visits, indicate a gap in browsing.
      var thisTime = visit.date.getTime();
      if (lastTime && lastTime - thisTime > BROWSING_GAP_TIME)
        dayResults.appendChild(createElementWithClassName('li', 'gap'));

      // Insert the visit into the DOM.
      dayResults.appendChild(visit.getResultDOM({ addTitleFavicon: true }));
      this.setVisitRendered_(visit);

      lastTime = thisTime;
    }
  }
};

/**
 * Update the page with results.
 * @param {boolean} doneLoading Whether the current request is complete.
 * @private
 */
HistoryView.prototype.displayResults_ = function(doneLoading) {
  // Either show a page of results received for the all time results or all the
  // received results for the weekly and monthly view.
  var results = this.model_.visits_;
  if (this.getRangeInDays() == HistoryModel.Range.ALL_TIME) {
    var rangeStart = this.pageIndex_ * RESULTS_PER_PAGE;
    var rangeEnd = rangeStart + RESULTS_PER_PAGE;
    results = this.model_.getNumberedRange(rangeStart, rangeEnd);
  }
  var searchText = this.model_.getSearchText();
  var groupByDomain = this.model_.getGroupByDomain();

  if (searchText) {
    // Add a header for the search results, if there isn't already one.
    if (!this.resultDiv_.querySelector('h3')) {
      var header = document.createElement('h3');
      header.textContent = loadTimeData.getStringF('searchresultsfor',
                                                   searchText);
      this.resultDiv_.appendChild(header);
    }

    var searchResults = createElementWithClassName('ol', 'search-results');
    if (results.length == 0 && doneLoading) {
      var noSearchResults = document.createElement('div');
      noSearchResults.textContent = loadTimeData.getString('nosearchresults');
      searchResults.appendChild(noSearchResults);
    } else {
      for (var i = 0, visit; visit = results[i]; i++) {
        if (!visit.isRendered) {
          searchResults.appendChild(visit.getResultDOM({
            isSearchResult: true,
            addTitleFavicon: true
          }));
          this.setVisitRendered_(visit);
        }
      }
    }
    this.resultDiv_.appendChild(searchResults);
  } else {
    var resultsFragment = document.createDocumentFragment();

    if (this.getRangeInDays() != HistoryModel.Range.ALL_TIME) {
      // If this is a time range result add some text that shows what is the
      // time range for the results the user is viewing.
      var timeFrame = resultsFragment.appendChild(
          createElementWithClassName('h2', 'timeframe'));
      // TODO(sergiu): Figure the best way to show this for the first day of
      // the month.
      timeFrame.appendChild(document.createTextNode(loadTimeData.getStringF(
          'historyinterval',
          this.model_.queryStartTime,
          this.model_.queryEndTime)));
    }

    if (results.length == 0 && doneLoading) {
      var noResults = resultsFragment.appendChild(
          document.createElement('div'));
      noResults.textContent = loadTimeData.getString('noresults');
      this.resultDiv_.appendChild(resultsFragment);
      this.updateNavBar_();
      return;
    }

    if (this.getRangeInDays() == HistoryModel.Range.MONTH &&
        groupByDomain) {
      // Group everything together in the month view.
      this.addMonthResults_(results, resultsFragment);
    } else {
      var dayStart = 0;
      var dayEnd = 0;
      // Go through all of the visits and process them in chunks of one day.
      while (dayEnd < results.length) {
        // Skip over the ones that are already rendered.
        while (dayStart < results.length && results[dayStart].isRendered)
          ++dayStart;
        var dayEnd = dayStart + 1;
        while (dayEnd < results.length && results[dayEnd].continued)
          ++dayEnd;

        this.addDayResults_(
            results.slice(dayStart, dayEnd), resultsFragment, groupByDomain);
      }
    }

    // Add all the days and their visits to the page.
    this.resultDiv_.appendChild(resultsFragment);
  }
  this.updateNavBar_();
};

/**
 * Update the visibility of the page navigation buttons.
 * @private
 */
HistoryView.prototype.updateNavBar_ = function() {
  this.updateRangeButtons_();
  $('newest-button').hidden = this.pageIndex_ == 0;
  $('newer-button').hidden = this.pageIndex_ == 0;
  $('older-button').hidden =
      this.model_.rangeInDays_ != HistoryModel.Range.ALL_TIME ||
      !this.model_.hasMoreResults();
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
    var isGroupedByDomain = (hashData.g == 'true');
    if (hashData.q != state_obj.model.getSearchText()) {
      state_obj.view.setSearch(hashData.q, parseInt(hashData.p, 10));
    } else if (parseInt(hashData.p, 10) != state_obj.view.getPage()) {
      state_obj.view.setPage(hashData.p);
    } else if (isGroupedByDomain != state_obj.view.model_.getGroupByDomain()) {
      state_obj.view.setGroupByDomain(isGroupedByDomain);
    } else if (parseInt(hashData.r, 10) != state_obj.model.rangeInDays) {
      state_obj.view.setRangeInDays(parseInt(hashData.r, 10));
    } else if (parseInt(hashData.o, 10) != state_obj.model.offset) {
      state_obj.view.setOffset(parseInt(hashData.o, 10));
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
    p: 0,
    g: false,
    r: 0,
    o: 0
  };

  if (!window.location.hash)
    return result;

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
 * @param {number} page The page currently being viewed.
 * @param {boolean} grouped Whether the results are grouped or not.
 * @param {HistoryModel.Range} range The range to view or search over.
 * @param {number} offset Set the begining of the query to the specific offset.
 */
PageState.prototype.setUIState = function(term, page, grouped, range, offset) {
  // Make sure the form looks pretty.
  $('search-field').value = term;
  $('display-filter-sites').checked = grouped;
  var hash = this.getHashData();
  if (hash.q != term || hash.p != page || hash.g != grouped ||
      hash.r != range || hash.o != offset) {
    window.location.hash = PageState.getHashString(
        term, page, grouped, range, offset);
  }
};

/**
 * Static method to get the hash string for a specified state
 * @param {string} term The current search string.
 * @param {number} page The page currently being viewed.
 * @param {boolean} grouped Whether the results are grouped or not.
 * @param {HistoryModel.Range} range The range to view or search over.
 * @param {number} offset Set the begining of the query to the specific offset.
 * @return {string} The string to be used in a hash.
 */
PageState.getHashString = function(term, page, grouped, range, offset) {
  // Omit elements that are empty.
  var newHash = [];

  if (term)
    newHash.push('q=' + encodeURIComponent(term));

  if (page)
    newHash.push('p=' + page);

  if (grouped)
    newHash.push('g=' + grouped);

  if (range)
    newHash.push('r=' + range);

  if (offset)
    newHash.push('o=' + offset);

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

  // Only show the controls if the command line switch is activated.
  if (loadTimeData.getBoolean('groupByDomain') ||
      loadTimeData.getBoolean('isManagedProfile')) {
    $('filter-controls').hidden = false;
  }
  if (loadTimeData.getBoolean('isManagedProfile')) {
    $('allow-selected').hidden = false;
    $('block-selected').hidden = false;
    $('lock-unlock-button').classList.add('profile-is-managed');

    $('lock-unlock-button').addEventListener('click', function(e) {
      var isLocked = document.body.classList.contains('managed-user-locked');
      chrome.send('setManagedUserElevated', [isLocked]);
    });

    chrome.send('getManagedUserElevated');
  }

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
 * Adds manual rules for allowing or blocking the the selected items.
 * @param {boolean} allow Whether to allow (for true) or block (for false) the
 *    selected items.
 */
function processManagedList(allow) {
  var hosts = $('results-display').querySelectorAll(
      '.site-domain-wrapper input[type=checkbox]');
  var urls = $('results-display').querySelectorAll(
      '.site-results input[type=checkbox]');
  // Get each domain and whether it is checked or not.
  var hostsToSend = [];
  for (var i = 0; i < hosts.length; i++) {
    var hostParent = findAncestorByClass(hosts[i], 'site-domain-wrapper');
    var host = hostParent.querySelector('button').textContent;
    hostsToSend.push([hosts[i].checked, host]);
  }
  // Get each URL and whether it is checked or not.
  var urlsToSend = [];
  for (var i = 0; i < urls.length; i++) {
    var urlParent = findAncestorByClass(urls[i], 'site-entry');
    var urlChecked =
        urlParent.querySelector('.site-domain-wrapper input').checked;
    urlsToSend.push([urls[i].checked, urlChecked,
        findAncestorByClass(urls[i], 'entry-box').querySelector('a').href]);
  }
  chrome.send('processManagedUrls', [allow, hostsToSend, urlsToSend]);
}

/**
 * Updates the whitelist status labels of a host/URL entry to the current
 * value.
 * @param {Element} statusElement The div which contains the status labels.
 * @param {Object} newStatus A dictionary with two entries:
 *     - |inContentPack|: whether the current domain/URL is allowed by a
 *     content pack.
 *     - |manualBehavior|: The manual status of the current domain/URL.
 */
function updateHostStatus(statusElement, newStatus) {
  var inContentPackDiv = statusElement.querySelector('.in-content-pack');
  inContentPackDiv.className = 'in-content-pack';
  if (newStatus['inContentPack']) {
    if (newStatus['manualBehavior'] != ManagedModeManualBehavior.NONE)
      inContentPackDiv.classList.add('in-content-pack-passive');
    else
      inContentPackDiv.classList.add('in-content-pack-active');
  }

  var manualBehaviorDiv = statusElement.querySelector('.manual-behavior');
  manualBehaviorDiv.className = 'manual-behavior';
  switch (newStatus['manualBehavior']) {
  case ManagedModeManualBehavior.NONE:
    manualBehaviorDiv.textContent = '';
    break;
  case ManagedModeManualBehavior.ALLOW:
    manualBehaviorDiv.textContent = loadTimeData.getString('filterallowed');
    manualBehaviorDiv.classList.add('filter-allowed');
    break;
  case ManagedModeManualBehavior.BLOCK:
    manualBehaviorDiv.textContent = loadTimeData.getString('filterblocked');
    manualBehaviorDiv.classList.add('filter-blocked');
    break;
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
    chrome.send('removeUrlsOnOneDay',
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
  var checked = $('results-display').querySelectorAll(
      '.entry-box input[type=checkbox]:checked:not([disabled])');
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
  updateParentCheckbox(checkbox);
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
      if (checkbox) {
        checkbox.checked = checked;
        updateParentCheckbox(checkbox);
      }
    }
  }
  selectionAnchor = id;

  historyView.updateSelectionEditButtons();
}

/**
 * Handler for the 'click' event on a domain checkbox. Checkes or unchecks the
 * checkboxes of the visits to this domain in the respective group.
 * @param {Event} e The click event.
 */
function domainCheckboxClicked(e) {
  var siteEntry = findAncestorByClass(e.currentTarget, 'site-entry');
  var checkboxes =
      siteEntry.querySelectorAll('.site-results input[type=checkbox]');
  for (var i = 0; i < checkboxes.length; i++)
    checkboxes[i].checked = e.currentTarget.checked;
  historyView.updateSelectionEditButtons();
  // Stop propagation as clicking the checkbox would otherwise trigger the
  // group to collapse/expand.
  e.stopPropagation();
}

/**
 * Updates the domain checkbox for this visit checkbox if it has been
 * unchecked.
 * @param {Element} checkbox The checkbox that has been clicked.
 */
function updateParentCheckbox(checkbox) {
  if (checkbox.checked)
    return;

  var entry = findAncestorByClass(checkbox, 'site-entry');
  if (!entry)
    return;

  var group_checkbox = entry.querySelector('.site-domain-wrapper input');
  if (group_checkbox)
      group_checkbox.checked = false;
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

/**
 * Toggles an element in the grouped history.
 * @param {Element} e The element which was clicked on.
 */
function toggleHandler(e) {
  var innerResultList = e.currentTarget.parentElement.querySelector(
      '.site-results');
  var innerArrow = e.currentTarget.parentElement.querySelector(
      '.site-domain-arrow');
  if (innerArrow.classList.contains('collapse')) {
    innerResultList.style.height = 'auto';
    // -webkit-transition does not work on height:auto elements so first set
    // the height to auto so that it is computed and then set it to the
    // computed value in pixels so the transition works properly.
    var height = innerResultList.clientHeight;
    innerResultList.style.height = height + 'px';
    innerArrow.classList.remove('collapse');
    innerArrow.classList.add('expand');
  } else {
    innerResultList.style.height = 0;
    innerArrow.classList.remove('expand');
    innerArrow.classList.add('collapse');
  }
}

/**
 * Builds the DOM elements to show the managed status of a domain/URL.
 * @param {ManagedModeManualBehavior} manualBehavior The manual behavior for
 *     this item.
 * @param {boolean} inContentPack Whether this element is in a content pack or
 *     not.
 * @return {Element} Returns the DOM elements which show the status.
 */
function getManagedStatusDOM(manualBehavior, inContentPack) {
  var filterStatusDiv = createElementWithClassName('div', 'filter-status');
  var inContentPackDiv = createElementWithClassName('div', 'in-content-pack');
  inContentPackDiv.textContent = loadTimeData.getString('incontentpack');
  var manualBehaviorDiv = createElementWithClassName('div', 'manual-behavior');
  filterStatusDiv.appendChild(inContentPackDiv);
  filterStatusDiv.appendChild(manualBehaviorDiv);

  updateHostStatus(filterStatusDiv, {
    'inContentPack' : inContentPack,
    'manualBehavior' : manualBehavior
  });
  return filterStatusDiv;
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

/**
 * Called when the allow/block changes have been commited. This leads to the
 * status of all the elements on the page being updated.
 * @param {Object} entries The new updated results.
 */
function updateEntries(entries) {
  historyView.updateManagedEntries(entries);
}

/**
 * Called when the page is initialized or when the authentication status
 * of the managed user changes.
 * @param {Object} isElevated Contains the information if the current profile is
 * in elevated state.
 */
function managedUserElevated(isElevated) {
  if (isElevated) {
    document.body.classList.remove('managed-user-locked');
    $('lock-unlock-button').textContent = loadTimeData.getString('lockButton');
  } else {
    document.body.classList.add('managed-user-locked');
    $('lock-unlock-button').textContent =
        loadTimeData.getString('unlockButton');
  }
}

// Add handlers to HTML elements.
document.addEventListener('DOMContentLoaded', load);

// This event lets us enable and disable menu items before the menu is shown.
document.addEventListener('canExecute', function(e) {
  e.canExecute = true;
});
