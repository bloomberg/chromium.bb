// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Inherit the prototype methods from one constructor into another.
 */
function inherits(childCtor, parentCtor) {
  function tempCtor() {};
  tempCtor.prototype = parentCtor.prototype;
  childCtor.superClass_ = parentCtor.prototype;
  childCtor.prototype = new tempCtor();
  childCtor.prototype.constructor = childCtor;
};

/**
 * Sets the width (in pixels) on a DOM node.
 */
function setNodeWidth(node, widthPx) {
  node.style.width = widthPx.toFixed(0) + 'px';
}

/**
 * Sets the height (in pixels) on a DOM node.
 */
function setNodeHeight(node, heightPx) {
  node.style.height = heightPx.toFixed(0) + 'px';
}

/**
 * Sets the position and size of a DOM node (in pixels).
 */
function setNodePosition(node, leftPx, topPx, widthPx, heightPx) {
  node.style.left = leftPx.toFixed(0) + 'px';
  node.style.top = topPx.toFixed(0) + 'px';
  setNodeWidth(node, widthPx);
  setNodeHeight(node, heightPx);
}

/**
 * Sets the visibility for a DOM node.
 */
function setNodeDisplay(node, isVisible) {
  node.style.display = isVisible ? '' : 'none';
}

/**
 * Adds a node to |parentNode|, of type |tagName|.
 */
function addNode(parentNode, tagName) {
  var elem = parentNode.ownerDocument.createElement(tagName);
  parentNode.appendChild(elem);
  return elem;
}

/**
 * Adds |text| to node |parentNode|.
 */
function addTextNode(parentNode, text) {
  var textNode = parentNode.ownerDocument.createTextNode(text);
  parentNode.appendChild(textNode);
  return textNode;
}

/**
 * Adds a node to |parentNode|, of type |tagName|.  Then adds
 * |text| to the new node.
 */
function addNodeWithText(parentNode, tagName, text) {
  var elem = parentNode.ownerDocument.createElement(tagName);
  parentNode.appendChild(elem);
  addTextNode(elem, text);
  return elem;
}

/**
 * Adds or removes a CSS class to |node|.
 */
function changeClassName(node, classNameToAddOrRemove, isAdd) {
  // Multiple classes can be separated by spaces.
  var currentNames = node.className.split(' ');

  if (isAdd) {
    if (!(classNameToAddOrRemove in currentNames)) {
      currentNames.push(classNameToAddOrRemove);
    }
  } else {
    for (var i = 0; i < currentNames.length; ++i) {
      if (currentNames[i] == classNameToAddOrRemove) {
        currentNames.splice(i, 1);
        break;
      }
    }
  }

  node.className = currentNames.join(' ');
}

function getKeyWithValue(map, value) {
  for (key in map) {
    if (map[key] == value)
      return key;
  }
  return '?';
}

/**
 * Looks up |key| in |map|, and returns the resulting entry, if  there is one.
 * Otherwise, returns |key|.  Intended primarily for use with incomplete
 * tables, and for reasonable behavior with system enumerations that may be
 * extended in the future.
 */
function tryGetValueWithKey(map, key) {
  if (key in map)
    return map[key];
  return key;
}

/**
 * Builds a string by repeating |str| |count| times.
 */
function makeRepeatedString(str, count) {
  var out = [];
  for (var i = 0; i < count; ++i)
    out.push(str);
  return out.join('');
}

/**
 * TablePrinter is a helper to format a table as ascii art or an HTML table.
 *
 * Usage: call addRow() and addCell() repeatedly to specify the data.
 *
 * addHeaderCell() can optionally be called to specify header cells for a
 * single header row.  The header row appears at the top of an HTML formatted
 * table, and uses thead and th tags.  In ascii tables, the header is separated
 * from the table body by a partial row of dashes.
 *
 * setTitle() can optionally be used to set a title that is displayed before
 * the header row.  In HTML tables, it uses the title class and in ascii tables
 * it's between two rows of dashes.
 *
 * Once all the fields have been input, call toText() to format it as text or
 * toHTML() to format it as HTML.
 */
function TablePrinter() {
  this.rows_ = [];
  this.hasHeaderRow_ = false;
  this.title_ = null;
}

/**
 * Links are only used in HTML tables.
 */
function TablePrinterCell(value) {
  this.text = '' + value;
  this.link = null;
  this.alignRight = false;
  this.allowOverflow = false;
}

/**
 * Starts a new row.
 */
TablePrinter.prototype.addRow = function() {
  this.rows_.push([]);
};

/**
 * Adds a column to the current row, setting its value to cellText.
 *
 * @returns {!TablePrinterCell} the cell that was added.
 */
TablePrinter.prototype.addCell = function(cellText) {
  var r = this.rows_[this.rows_.length - 1];
  var cell = new TablePrinterCell(cellText);
  r.push(cell);
  return cell;
};

TablePrinter.prototype.setTitle = function(title) {
  this.title_ = title;
};

/**
 * Adds a header row, if not already present, and adds a new column to it,
 * setting its contents to |headerText|.
 *
 * @returns {!TablePrinterCell} the cell that was added.
 */
TablePrinter.prototype.addHeaderCell = function(headerText) {
  // Insert empty new row at start of |rows_| if currently no header row.
  if (!this.hasHeaderRow_) {
    this.rows_.splice(0, 0, []);
    this.hasHeaderRow_ = true;
  }
  var cell = new TablePrinterCell(headerText);
  this.rows_[0].push(cell);
  return cell;
};

/**
 * Returns the maximum number of columns this table contains.
 */
TablePrinter.prototype.getNumColumns = function() {
  var numColumns = 0;
  for (var i = 0; i < this.rows_.length; ++i) {
    numColumns = Math.max(numColumns, this.rows_[i].length);
  }
  return numColumns;
}

/**
 * Returns the cell at position (rowIndex, columnIndex), or null if there is
 * no such cell.
 */
TablePrinter.prototype.getCell_ = function(rowIndex, columnIndex) {
  if (rowIndex >= this.rows_.length)
    return null;
  var row = this.rows_[rowIndex];
  if (columnIndex >= row.length)
    return null;
  return row[columnIndex];
};

/**
 * Returns a formatted text representation of the table data.
 * |spacing| indicates number of extra spaces, if any, to add between
 * columns.
 */
TablePrinter.prototype.toText = function(spacing) {
  var numColumns = this.getNumColumns();

  // Figure out the maximum width of each column.
  var columnWidths = [];
  columnWidths.length = numColumns;
  for (var i = 0; i < numColumns; ++i)
    columnWidths[i] = 0;

  // If header row is present, temporarily add a spacer row to |rows_|.
  if (this.hasHeaderRow_) {
    var headerSpacerRow = [];
    for (var c = 0; c < numColumns; ++c) {
      var cell = this.getCell_(0, c);
      if (!cell)
        continue;
      var spacerStr = makeRepeatedString('-', cell.text.length);
      headerSpacerRow.push(new TablePrinterCell(spacerStr));
    }
    this.rows_.splice(1, 0, headerSpacerRow);
  }

  var numRows = this.rows_.length;
  for (var c = 0; c < numColumns; ++c) {
    for (var r = 0; r < numRows; ++r) {
      var cell = this.getCell_(r, c);
      if (cell && !cell.allowOverflow) {
        columnWidths[c] = Math.max(columnWidths[c], cell.text.length);
      }
    }
  }

  var out = [];

  // Print title, if present.
  if (this.title_) {
    var titleSpacerStr = makeRepeatedString('-', this.title_.length);
    out.push(titleSpacerStr);
    out.push('\n');
    out.push(this.title_);
    out.push('\n');
    out.push(titleSpacerStr);
    out.push('\n');
  }

  // Print each row.
  var spacingStr = makeRepeatedString(' ', spacing);
  for (var r = 0; r < numRows; ++r) {
    for (var c = 0; c < numColumns; ++c) {
      var cell = this.getCell_(r, c);
      if (cell) {
        // Pad the cell with spaces to make it fit the maximum column width.
        var padding = columnWidths[c] - cell.text.length;
        var paddingStr = makeRepeatedString(' ', padding);

        if (cell.alignRight) {
          out.push(paddingStr);
          out.push(cell.text);
        } else {
          out.push(cell.text);
          out.push(paddingStr);
        }
        out.push(spacingStr);
      }
    }
    out.push('\n');
  }

  // Remove spacer row under the header row, if one was added.
  if (this.hasHeaderRow_)
    this.rows_.splice(1, 1);

  return out.join('');
};

/**
 * Adds a new HTML table to the node |parent| using the specified style.
 */
TablePrinter.prototype.toHTML = function(parent, style) {
  var numRows = this.rows_.length;
  var numColumns = this.getNumColumns();

  var table = addNode(parent, 'table');
  table.setAttribute('class', style);

  var thead = addNode(table, 'thead');
  var tbody = addNode(table, 'tbody');

  // Add title, if needed.
  if (this.title_) {
    var tableTitleRow = addNode(thead, 'tr');
    var tableTitle = addNodeWithText(tableTitleRow, 'th', this.title_);
    tableTitle.colSpan = numColumns;
    changeClassName(tableTitle, 'title', true);
  }

  // Fill table body, adding header row first, if needed.
  for (var r = 0; r < numRows; ++r) {
    var cellType;
    var row;
    if (r == 0 && this.hasHeaderRow_) {
      row = addNode(thead, 'tr');
      cellType = 'th';
    } else {
      row = addNode(tbody, 'tr');
      cellType = 'td';
    }
    for (var c = 0; c < numColumns; ++c) {
      var cell = this.getCell_(r, c);
      if (cell) {
        var tableCell = addNode(row, cellType, cell.text);
        if (cell.alignRight)
          tableCell.alignRight = true;
        // If allowing overflow on the rightmost cell of a row,
        // make the cell span the rest of the columns.  Otherwise,
        // ignore the flag.
        if (cell.allowOverflow && !this.getCell_(r, c + 1))
          tableCell.colSpan = numColumns - c;
        if (cell.link) {
          var linkNode = addNodeWithText(tableCell, 'a', cell.text);
          linkNode.href = cell.link;
        } else {
          addTextNode(tableCell, cell.text);
        }
      }
    }
  }
  return table;
};

