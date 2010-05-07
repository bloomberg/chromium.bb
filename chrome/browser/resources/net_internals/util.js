// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Helper that binds the |this| object to a method to create a callback.
 */
Function.prototype.bind = function(thisObj) {
  var func = this;
  var args = Array.prototype.slice.call(arguments, 1);
  return function() {
    return func.apply(thisObj,
    args.concat(Array.prototype.slice.call(arguments, 0)))
  };
};

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
  node.style.width = widthPx.toFixed(0) + "px";
}

/**
 * Sets the height (in pixels) on a DOM node.
 */
function setNodeHeight(node, heightPx) {
  node.style.height = heightPx.toFixed(0) + "px";
}

/**
 * Sets the position and size of a DOM node (in pixels).
 */
function setNodePosition(node, leftPx, topPx, widthPx, heightPx) {
  node.style.left = leftPx.toFixed(0) + "px";
  node.style.top = topPx.toFixed(0) + "px";
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
 * Adds text to node |parentNode|.
 */
function addTextNode(parentNode, text) {
  var textNode = parentNode.ownerDocument.createTextNode(text);
  parentNode.appendChild(textNode);
  return textNode;
}

/**
 * Adds or removes a CSS class to |node|.
 */
function changeClassName(node, classNameToAddOrRemove, isAdd) {
  // Multiple classes can be separated by spaces.
  var currentNames = node.className.split(" ");

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

  node.className = currentNames.join(" ");
}

function getKeyWithValue(map, value) {
  for (key in map) {
    if (map[key] == value)
      return key;
  }
  return '?';
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
 * TablePrinter is a helper to format a table as ascii art.
 *
 * Usage: call addRow() and addCell() repeatedly to specify the data. Ones
 * all the fields have been inputted, call toText() to format it as text.
 */
function TablePrinter() {
  this.rows_ = [];
}

function TablePrinterCell(value) {
  this.text = '' + value;
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
 */
TablePrinter.prototype.toText = function() {
  var numRows = this.rows_.length;
  var numColumns = this.getNumColumns();

  // Figure out the maximum width of each column.
  var columnWidths = [];
  columnWidths.length = numColumns;
  for (var i = 0; i < numColumns; ++i)
    columnWidths[i] = 0;

  for (var c = 0; c < numColumns; ++c) {
    for (var r = 0; r < numRows; ++r) {
      var cell = this.getCell_(r, c);
      if (cell && !cell.allowOverflow) {
        columnWidths[c] = Math.max(columnWidths[c], cell.text.length);
      }
    }
  }

  // Print each row.
  var out = [];
  for (var r = 0; r < numRows; ++r) {
    for (var c = 0; c < numColumns; ++c) {
      var cell = this.getCell_(r, c);
      if (cell) {
        // Padd the cell with spaces to make it fit the maximum column width.
        var padding = columnWidths[c] - cell.text.length;
        var paddingStr = makeRepeatedString(' ', padding);

        if (cell.alignRight) {
          out.push(paddingStr);
          out.push(cell.text);
        } else {
          out.push(cell.text);
          out.push(paddingStr);
        }
      }
    }
    out.push('\n');
  }

  return out.join('');
};

