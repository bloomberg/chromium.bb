// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Each row in the filtered items list is backed by a SourceEntry. This
 * instance contains all of the data pertaining to that row, and notifies
 * its parent view (the EventsView) whenever its data changes.
 *
 * @constructor
 */
function SourceEntry(parentView, maxPreviousSourceId) {
  this.maxPreviousSourceId_ = maxPreviousSourceId;
  this.entries_ = [];
  this.parentView_ = parentView;
  this.isSelected_ = false;
  this.isMatchedByFilter_ = false;
  // If the first entry is a BEGIN_PHASE, set to true.
  // Set to false when an END_PHASE matching the first entry is encountered.
  this.isActive_ = false;
}

SourceEntry.prototype.isSelected = function() {
  return this.isSelected_;
};

SourceEntry.prototype.setSelectedStyles = function(isSelected) {
  changeClassName(this.row_, 'selected', isSelected);
  this.getSelectionCheckbox().checked = isSelected;
};

SourceEntry.prototype.setMouseoverStyle = function(isMouseOver) {
  changeClassName(this.row_, 'mouseover', isMouseOver);
};

SourceEntry.prototype.setIsMatchedByFilter = function(isMatchedByFilter) {
  if (this.isMatchedByFilter() == isMatchedByFilter)
    return;  // No change.

  this.isMatchedByFilter_ = isMatchedByFilter;

  this.setFilterStyles(isMatchedByFilter);

  if (isMatchedByFilter) {
    this.parentView_.incrementPostfilterCount(1);
  } else {
    this.parentView_.incrementPostfilterCount(-1);
    // If we are filtering an entry away, make sure it is no longer
    // part of the selection.
    this.setSelected(false);
  }
};

SourceEntry.prototype.isMatchedByFilter = function() {
  return this.isMatchedByFilter_;
};

SourceEntry.prototype.setFilterStyles = function(isMatchedByFilter) {
  // Hide rows which have been filtered away.
  if (isMatchedByFilter) {
    this.row_.style.display = '';
  } else {
    this.row_.style.display = 'none';
  }
};

SourceEntry.prototype.update = function(logEntry) {
  if (logEntry.phase == LogEventPhase.PHASE_BEGIN &&
      this.entries_.length == 0)
    this.isActive_ = true;

  // Only the last event should have the same type first event,
  if (this.isActive_ &&
      logEntry.phase == LogEventPhase.PHASE_END &&
      logEntry.type == this.entries_[0].type)
    this.isActive_ = false;

  var prevStartEntry = this.getStartEntry_();
  this.entries_.push(logEntry);
  var curStartEntry = this.getStartEntry_();

  // If we just got the first entry for this source.
  if (prevStartEntry != curStartEntry) {
    if (!prevStartEntry)
      this.createRow_();
    else
      this.updateDescription_();
  }

  // Update filters.
  var matchesFilter = this.matchesFilter(this.parentView_.currentFilter_);
  this.setIsMatchedByFilter(matchesFilter);
};

SourceEntry.prototype.onCheckboxToggled_ = function() {
  this.setSelected(this.getSelectionCheckbox().checked);
};

SourceEntry.prototype.matchesFilter = function(filter) {
  // Safety check.
  if (this.row_ == null)
    return false;

  if (filter.isActive && !this.isActive_)
    return false;
  if (filter.isInactive && this.isActive_)
    return false;

  // Check source type, if needed.
  if (filter.type) {
    var sourceType = this.getSourceTypeString().toLowerCase();
    if (filter.type.indexOf(sourceType) == -1)
      return false;
  }

  // Check source ID, if needed.
  if (filter.id) {
    if (filter.id.indexOf(this.getSourceId() + '') == -1)
      return false;
  }

  if (filter.text == '')
    return true;

  var filterText = filter.text;
  var entryText = PrintSourceEntriesAsText(this.entries_).toLowerCase();

  return entryText.indexOf(filterText) != -1;
};

SourceEntry.prototype.setSelected = function(isSelected) {
  if (isSelected == this.isSelected())
    return;

  this.isSelected_ = isSelected;

  this.setSelectedStyles(isSelected);
  this.parentView_.modifySelectionArray(this, isSelected);
  this.parentView_.onSelectionChanged();
};

SourceEntry.prototype.onClicked_ = function() {
  this.parentView_.clearSelection();
  this.setSelected(true);
};

SourceEntry.prototype.onMouseover_ = function() {
  this.setMouseoverStyle(true);
};

SourceEntry.prototype.onMouseout_ = function() {
  this.setMouseoverStyle(false);
};

SourceEntry.prototype.updateDescription_ = function() {
  this.descriptionCell_.innerHTML = '';
  addTextNode(this.descriptionCell_, this.getDescription());
};

SourceEntry.prototype.createRow_ = function() {
  // Create a row.
  var tr = addNode(this.parentView_.tableBody_, 'tr');
  tr._id = this.getSourceId();
  tr.style.display = 'none';
  this.row_ = tr;

  var selectionCol = addNode(tr, 'td');
  var checkbox = addNode(selectionCol, 'input');
  checkbox.type = 'checkbox';

  var idCell = addNode(tr, 'td');
  idCell.style.textAlign = 'right';

  var typeCell = addNode(tr, 'td');
  var descriptionCell = addNode(tr, 'td');
  this.descriptionCell_ = descriptionCell;

  // Connect listeners.
  checkbox.onchange = this.onCheckboxToggled_.bind(this);

  var onclick = this.onClicked_.bind(this);
  idCell.onclick = onclick;
  typeCell.onclick = onclick;
  descriptionCell.onclick = onclick;

  tr.onmouseover = this.onMouseover_.bind(this);
  tr.onmouseout = this.onMouseout_.bind(this);

  // Set the cell values to match this source's data.
  if (this.getSourceId() >= 0)
    addTextNode(idCell, this.getSourceId());
  else
    addTextNode(idCell, '-');
  var sourceTypeString = this.getSourceTypeString();
  addTextNode(typeCell, sourceTypeString);
  this.updateDescription_();

  // Add a CSS classname specific to this source type (so CSS can specify
  // different stylings for different types).
  changeClassName(this.row_, 'source_' + sourceTypeString, true);
};

/**
 * Returns a description for this source log stream, which will be displayed
 * in the list view. Most often this is a URL that identifies the request,
 * or a hostname for a connect job, etc...
 */
SourceEntry.prototype.getDescription = function() {
  var e = this.getStartEntry_();
  if (!e)
    return '';

  if (e.source.type == LogSourceType.NONE) {
    // NONE is what we use for global events that aren't actually grouped
    // by a "source ID", so we will just stringize the event's type.
    return getKeyWithValue(LogEventType, e.type);
  }

  if (e.params == undefined)
    return '';

  var description = '';
  switch (e.source.type) {
    case LogSourceType.URL_REQUEST:
    case LogSourceType.SOCKET_STREAM:
    case LogSourceType.HTTP_STREAM_JOB:
      description = e.params.url;
      break;
    case LogSourceType.CONNECT_JOB:
      description = e.params.group_name;
      break;
    case LogSourceType.HOST_RESOLVER_IMPL_REQUEST:
    case LogSourceType.HOST_RESOLVER_IMPL_JOB:
      description = e.params.host;
      break;
    case LogSourceType.DISK_CACHE_ENTRY:
    case LogSourceType.MEMORY_CACHE_ENTRY:
      description = e.params.key;
      break;
    case LogSourceType.SPDY_SESSION:
      if (e.params.host)
        description = e.params.host + ' (' + e.params.proxy + ')';
      break;
    case LogSourceType.SOCKET:
      if (e.params.source_dependency != undefined) {
        var connectJobSourceEntry =
            this.parentView_.getSourceEntry(e.params.source_dependency.id);
        if (connectJobSourceEntry)
          description = connectJobSourceEntry.getDescription();
      }
      break;
  }

  if (description == undefined)
    return '';
  return description;
};

/**
 * Returns the starting entry for this source. Conceptually this is the
 * first entry that was logged to this source. However, we skip over the
 * TYPE_REQUEST_ALIVE entries which wrap TYPE_URL_REQUEST_START_JOB /
 * TYPE_SOCKET_STREAM_CONNECT.
 */
SourceEntry.prototype.getStartEntry_ = function() {
  if (this.entries_.length < 1)
    return undefined;
  if (this.entries_.length >= 2) {
    if (this.entries_[0].type == LogEventType.REQUEST_ALIVE ||
        this.entries_[0].type == LogEventType.SOCKET_POOL_CONNECT_JOB)
      return this.entries_[1];
  }
  return this.entries_[0];
};

SourceEntry.prototype.getLogEntries = function() {
  return this.entries_;
};

SourceEntry.prototype.getSourceTypeString = function() {
  return getKeyWithValue(LogSourceType, this.entries_[0].source.type);
};

SourceEntry.prototype.getSelectionCheckbox = function() {
  return this.row_.childNodes[0].firstChild;
};

SourceEntry.prototype.getSourceId = function() {
  return this.entries_[0].source.id;
};

/**
 * Returns the largest source ID seen before this object was received.
 * Used only for sorting SourceEntries without a source by source ID.
 */
SourceEntry.prototype.getMaxPreviousEntrySourceId = function() {
  return this.maxPreviousSourceId_;
};

SourceEntry.prototype.isActive = function() {
  return this.isActive_;
};

/**
 * Returns time of last event if inactive.  Returns current time otherwise.
 */
SourceEntry.prototype.getEndTime = function() {
  if (this.isActive_) {
    return (new Date()).getTime();
  }
  else {
    var endTicks = this.entries_[this.entries_.length - 1].time;
    return g_browser.convertTimeTicksToDate(endTicks).getTime();
  }
};

/**
 * Returns the time between the first and last events with a matching
 * source ID.  If source is still active, uses the current time for the
 * last event.
 */
SourceEntry.prototype.getDuration = function() {
  var startTicks = this.entries_[0].time;
  var startTime = g_browser.convertTimeTicksToDate(startTicks).getTime();
  var endTime = this.getEndTime();
  return endTime - startTime;
};

/**
 * Returns source ID of the entry whose row is currently above this one's.
 * Returns null if no such node exists.
 */
SourceEntry.prototype.getPreviousNodeSourceId = function() {
  if (!this.hasRow())
    return null;
  var prevNode = this.row_.previousSibling;
  if (prevNode == null)
    return null;
  return prevNode._id;
};

/**
 * Returns source ID of the entry whose row is currently below this one's.
 * Returns null if no such node exists.
 */
SourceEntry.prototype.getNextNodeSourceId = function() {
  if (!this.hasRow())
    return null;
  var nextNode = this.row_.nextSibling;
  if (nextNode == null)
    return null;
  return nextNode._id;
};

SourceEntry.prototype.hasRow = function() {
  return this.row_ != null;
};

/**
 * Moves current object's row before |entry|'s row.
 */
SourceEntry.prototype.moveBefore = function(entry) {
  if (this.hasRow() && entry.hasRow()) {
    this.row_.parentNode.insertBefore(this.row_, entry.row_);
  }
};

/**
 * Moves current object's row after |entry|'s row.
 */
SourceEntry.prototype.moveAfter = function(entry) {
  if (this.hasRow() && entry.hasRow()) {
    this.row_.parentNode.insertBefore(this.row_, entry.row_.nextSibling);
  }
};

SourceEntry.prototype.remove = function() {
  this.setSelected(false);
  this.setIsMatchedByFilter(false);
  this.row_.parentNode.removeChild(this.row_);
};

