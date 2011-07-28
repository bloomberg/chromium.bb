// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A SourceRow represents the row corresponding to a single SourceEntry
 * displayed by the EventsView.
 *
 * @constructor
 */
function SourceRow(parentView, sourceEntry) {
  this.parentView_ = parentView;

  this.sourceEntry_ = sourceEntry;
  this.isSelected_ = false;
  this.isMatchedByFilter_ = false;

  // Used to set CSS class for display.  Must only be modified by calling
  // corresponding set functions.
  this.isSelected_ = false;
  this.isMouseOver_ = false;

  // Mirror sourceEntry's values, so we only update the DOM when necessary.
  this.isError_ = sourceEntry.isError();
  this.isInactive_ = sourceEntry.isInactive();
  this.description_ = sourceEntry.getDescription();

  this.createRow_();
  this.onSourceUpdated();
}

SourceRow.prototype.createRow_ = function() {
  // Create a row.
  var tr = addNode(this.parentView_.tableBody_, 'tr');
  tr._id = this.sourceEntry_.getSourceId();
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
  if (this.sourceEntry_.getSourceId() >= 0) {
    addTextNode(idCell, this.sourceEntry_.getSourceId());
  } else {
    addTextNode(idCell, '-');
  }
  var sourceTypeString = this.sourceEntry_.getSourceTypeString();
  addTextNode(typeCell, sourceTypeString);
  this.updateDescription_();

  // Add a CSS classname specific to this source type (so CSS can specify
  // different stylings for different types).
  changeClassName(this.row_, 'source_' + sourceTypeString, true);

  this.updateClass_();
};

SourceRow.prototype.onSourceUpdated = function() {
  if (this.sourceEntry_.isInactive() != this.isInactive_ ||
      this.sourceEntry_.isError() != this.isError_) {
    this.updateClass_();
  }

  if (this.description_ != this.sourceEntry_.getDescription())
    this.updateDescription_();

  // Update filters.
  var matchesFilter = this.matchesFilter(this.parentView_.currentFilter_);
  this.setIsMatchedByFilter(matchesFilter);
};

/**
 * Changes |row_|'s class based on currently set flags.  Clears any previous
 * class set by this method.  This method is needed so that some styles
 * override others.
 */
SourceRow.prototype.updateClass_ = function() {
  this.isInactive_ = this.sourceEntry_.isInactive();
  this.isError_ = this.sourceEntry_.isError();

  // Each element of this list contains a property of |this| and the
  // corresponding class name to set if that property is true.  Entries earlier
  // in the list take precedence.
  var propertyNames = [
    ['isSelected_', 'selected'],
    ['isMouseOver_', 'mouseover'],
    ['isError_', 'error'],
    ['isInactive_', 'inactive'],
  ];

  // Loop through |propertyNames| in order, checking if each property
  // is true.  For the first such property found, if any, add the
  // corresponding class to the SourceEntry's row.  Remove classes
  // that correspond to any other property.
  var noStyleSet = true;
  for (var i = 0; i < propertyNames.length; ++i) {
    var setStyle = noStyleSet && this[propertyNames[i][0]];
    changeClassName(this.row_, propertyNames[i][1], setStyle);
    if (setStyle)
      noStyleSet = false;
  }
};

SourceRow.prototype.getSourceEntry = function() {
  return this.sourceEntry_;
};

SourceRow.prototype.setIsMatchedByFilter = function(isMatchedByFilter) {
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

SourceRow.prototype.isMatchedByFilter = function() {
  return this.isMatchedByFilter_;
};

SourceRow.prototype.setFilterStyles = function(isMatchedByFilter) {
  // Hide rows which have been filtered away.
  if (isMatchedByFilter) {
    this.row_.style.display = '';
  } else {
    this.row_.style.display = 'none';
  }
};

SourceRow.prototype.matchesFilter = function(filter) {
  if (filter.isActive && this.isInactive_)
    return false;
  if (filter.isInactive && !this.isInactive_)
    return false;
  if (filter.isError && !this.isError_)
    return false;
  if (filter.isNotError && this.isError_)
    return false;

  // Check source type, if needed.
  if (filter.type) {
    var sourceType = this.sourceEntry_.getSourceTypeString().toLowerCase();
    if (filter.type.indexOf(sourceType) == -1)
      return false;
  }

  // Check source ID, if needed.
  if (filter.id) {
    if (filter.id.indexOf(this.sourceEntry_.getSourceId() + '') == -1)
      return false;
  }

  if (filter.text == '')
    return true;

  var filterText = filter.text;
  var entryText = this.sourceEntry_.PrintEntriesAsText().toLowerCase();

  return entryText.indexOf(filterText) != -1;
};

SourceRow.prototype.isSelected = function() {
  return this.isSelected_;
};

SourceRow.prototype.setSelected = function(isSelected) {
  if (isSelected == this.isSelected())
    return;

  this.isSelected_ = isSelected;

  this.setSelectedStyles(isSelected);
  this.parentView_.modifySelectionArray(this, isSelected);
  this.parentView_.onSelectionChanged();
};

SourceRow.prototype.setSelectedStyles = function(isSelected) {
  this.isSelected_ = isSelected;
  this.getSelectionCheckbox().checked = isSelected;
  this.updateClass_();
};

SourceRow.prototype.setMouseoverStyle = function(isMouseOver) {
  this.isMouseOver_ = isMouseOver;
  this.updateClass_();
};

SourceRow.prototype.onClicked_ = function() {
  this.parentView_.clearSelection();
  this.setSelected(true);
};

SourceRow.prototype.onMouseover_ = function() {
  this.setMouseoverStyle(true);
};

SourceRow.prototype.onMouseout_ = function() {
  this.setMouseoverStyle(false);
};

SourceRow.prototype.updateDescription_ = function() {
  this.description_ = this.sourceEntry_.getDescription();
  this.descriptionCell_.innerHTML = '';
  addTextNode(this.descriptionCell_, this.description_);
};

SourceRow.prototype.onCheckboxToggled_ = function() {
  this.setSelected(this.getSelectionCheckbox().checked);
};

SourceRow.prototype.getSelectionCheckbox = function() {
  return this.row_.childNodes[0].firstChild;
};

/**
 * Returns source ID of the entry whose row is currently above this one's.
 * Returns null if no such node exists.
 */
SourceRow.prototype.getPreviousNodeSourceId = function() {
  var prevNode = this.row_.previousSibling;
  if (prevNode == null)
    return null;
  return prevNode._id;
};

/**
 * Returns source ID of the entry whose row is currently below this one's.
 * Returns null if no such node exists.
 */
SourceRow.prototype.getNextNodeSourceId = function() {
  var nextNode = this.row_.nextSibling;
  if (nextNode == null)
    return null;
  return nextNode._id;
};

/**
 * Moves current object's row before |entry|'s row.
 */
SourceRow.prototype.moveBefore = function(entry) {
  this.row_.parentNode.insertBefore(this.row_, entry.row_);
};

/**
 * Moves current object's row after |entry|'s row.
 */
SourceRow.prototype.moveAfter = function(entry) {
  this.row_.parentNode.insertBefore(this.row_, entry.row_.nextSibling);
};

SourceRow.prototype.remove = function() {
  this.setSelected(false);
  this.setIsMatchedByFilter(false);
  this.row_.parentNode.removeChild(this.row_);
};
