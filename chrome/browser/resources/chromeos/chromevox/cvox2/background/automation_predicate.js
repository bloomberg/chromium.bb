// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox predicates for the automation extension API.
 */

goog.provide('AutomationPredicate');
goog.provide('AutomationPredicate.Binary');
goog.provide('AutomationPredicate.Unary');

goog.require('constants');

goog.scope(function() {
var AutomationNode = chrome.automation.AutomationNode;
var Dir = constants.Dir;
var RoleType = chrome.automation.RoleType;

/**
 * @constructor
 */
AutomationPredicate = function() {};

/**
 * @typedef {function(!AutomationNode) : boolean}
 */
AutomationPredicate.Unary;

/**
 * @typedef {function(!AutomationNode, !AutomationNode) : boolean}
 */
AutomationPredicate.Binary;

/**
 * Constructs a predicate given a role.
 * @param {RoleType} role
 * @return {AutomationPredicate.Unary}
 */
AutomationPredicate.withRole = function(role) {
  return function(node) {
    return node.role == role;
  };
};

/** @type {AutomationPredicate.Unary} */
AutomationPredicate.checkBox = AutomationPredicate.withRole(RoleType.checkBox);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.comboBox = AutomationPredicate.withRole(RoleType.comboBox);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.heading = AutomationPredicate.withRole(RoleType.heading);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.inlineTextBox =
    AutomationPredicate.withRole(RoleType.inlineTextBox);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.link = AutomationPredicate.withRole(RoleType.link);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.row = AutomationPredicate.withRole(RoleType.row);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.table = AutomationPredicate.withRole(RoleType.table);

/**
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.button = function(node) {
  return /button/i.test(node.role);
};


/**
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.editText = function(node) {
  return node.state.editable &&
      node.parent &&
      !node.parent.state.editable;
};

/**
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.formField = function(node) {
  switch (node.role) {
    case 'button':
    case 'buttonDropDown':
    case 'checkBox':
    case 'comboBox':
    case 'date':
    case 'dateTime':
    case 'details':
    case 'disclosureTriangle':
    case 'form':
    case 'menuButton':
    case 'menuListPopup':
    case 'popUpButton':
    case 'radioButton':
    case 'searchBox':
    case 'slider':
    case 'spinButton':
    case 'switch':
    case 'tab':
    case 'textField':
    case 'time':
    case 'toggleButton':
    case 'tree':
      return true;
  }
  return false;
};

/**
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.landmark = function(node) {
  switch (node.role) {
    case 'application':
    case 'banner':
    case 'complementary':
    case 'contentInfo':
    case 'form':
    case 'main':
    case 'navigation':
    case 'search':
      return true;
  }
  return false;
};

/**
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.visitedLink = function(node) {
  return node.state.visited;
};

/**
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.focused = function(node) {
  return node.state.focused;
};

/**
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.leaf = function(node) {
  return !node.firstChild ||
      node.role == RoleType.button ||
      node.role == RoleType.buttonDropDown ||
      node.role == RoleType.popUpButton ||
      node.role == RoleType.slider ||
      node.role == RoleType.textField ||
      node.state.invisible ||
      node.children.every(function(n) {
        return n.state.invisible;
      });
};

/**
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.leafWithText = function(node) {
  return AutomationPredicate.leaf(node) &&
      !!(node.name || node.value);
};

/**
 * Matches against leaves or static text nodes. Useful when restricting
 * traversal to non-inline textboxes while still allowing them if navigation
 * already entered into an inline textbox.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.leafOrStaticText = function(node) {
  return AutomationPredicate.leaf(node) ||
      node.role == RoleType.staticText;
};

/**
 * Matches against nodes visited during object navigation. An object as
 * defined below, are all nodes that are focusable or static text. When used in
 * tree walking, it should visit all nodes that tab traversal would as well as
 * non-focusable static text.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.object = function(node) {
  return node.state.focusable ||
      (AutomationPredicate.leafOrStaticText(node) &&
       (/\S+/.test(node.name) ||
        (node.role != RoleType.lineBreak &&
         node.role != RoleType.staticText &&
         node.role != RoleType.inlineTextBox)));
};

/**
 * @param {!AutomationNode} first
 * @param {!AutomationNode} second
 * @return {boolean}
 */
AutomationPredicate.linebreak = function(first, second) {
  // TODO(dtseng): Use next/previousOnLin once available.
  var fl = first.location;
  var sl = second.location;
  return fl.top != sl.top ||
      (fl.top + fl.height != sl.top + sl.height);
};

/**
 * Matches against a node that contains other interesting nodes.
 * These nodes should always have their subtrees scanned when navigating.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.container = function(node) {
  return AutomationPredicate.structuralContainer(node) ||
      node.role == RoleType.div ||
      node.role == RoleType.document ||
      node.role == RoleType.group ||
      node.role == RoleType.listItem ||
      node.role == RoleType.toolbar ||
      node.role == RoleType.window ||
      // For example, crosh.
      (node.role == RoleType.textField && node.state.readOnly) ||
      (node.state.editable && node.parent && !node.parent.state.editable);
};

/**
 * Matches against nodes that contain interesting nodes, but should never be
 * visited.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.structuralContainer = function(node) {
  return node.role == RoleType.rootWebArea ||
      node.role == RoleType.embeddedObject ||
      node.role == RoleType.iframe ||
      node.role == RoleType.iframePresentational;
};

/**
 * Returns whether the given node should not be crossed when performing
 * traversals up the ancestry chain.
 * @param {AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.root = function(node) {
  switch (node.role) {
    case RoleType.dialog:
    case RoleType.window:
      return true;
    case RoleType.toolbar:
      return node.root.role == RoleType.desktop;
    case RoleType.rootWebArea:
      return !node.parent || node.parent.root.role == RoleType.desktop;
    default:
      return false;
  }
};

/**
 * Nodes that should be ignored while traversing the automation tree. For
 * example, apply this predicate when moving to the next object.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.shouldIgnoreNode = function(node) {
  // Ignore invisible nodes.
  if (node.state.invisible ||
      (node.location.height == 0 && node.location.width == 0))
    return true;

  // Ignore structural containres.
  if (AutomationPredicate.structuralContainer(node))
    return true;

  // Ignore list markers since we already announce listitem role.
  if (node.role == RoleType.listMarker)
    return true;

  // Don't ignore nodes with names.
  if (node.name || node.value || node.description)
    return false;

  // Ignore some roles.
  return AutomationPredicate.leaf(node) &&
      (node.role == RoleType.client ||
       node.role == RoleType.column ||
       node.role == RoleType.div ||
       node.role == RoleType.group ||
       node.role == RoleType.image ||
       node.role == RoleType.staticText ||
       node.role == RoleType.tableHeaderContainer);
};


/**
 * Returns if the node has a meaningful checked state.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.checkable = function(node) {
  return node.role == RoleType.checkBox ||
      node.role == RoleType.radioButton ||
      node.role == RoleType.menuItemCheckBox ||
      node.role == RoleType.menuItemRadio ||
      node.role == RoleType.treeItem;
};

// Table related predicates.
/**
 * Returns if the node has a cell like role.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.cellLike = function(node) {
  return node.role == RoleType.cell ||
      node.role == RoleType.rowHeader ||
      node.role == RoleType.columnHeader;
};

/**
 * Returns a predicate that will match against the directed next cell taking
 * into account the current ancestor cell's position in the table.
 * @param {AutomationNode} start
 * @param {{dir: (Dir|undefined),
*           row: (boolean|undefined),
 *          col: (boolean|undefined)}} opts
 * |dir|, specifies direction for |row or/and |col| movement by one cell.
 *     |dir| defaults to forward.
 *     |row| and |col| are both false by default.
 *     |end| defaults to false. If set to true, |col| must also be set to true.
 * It will then return the first or last cell in the current column.
 * @return {?AutomationPredicate.Unary} Returns null if not in a table.
 */
AutomationPredicate.makeTableCellPredicate = function(start, opts) {
  if (!opts.row && !opts.col)
    throw new Error('You must set either row or col to true');

  var dir = opts.dir || Dir.FORWARD;

  // Compute the row/col index defaulting to 0.
  var rowIndex = 0, colIndex = 0;
  var tableNode = start;
  while (tableNode) {
    if (AutomationPredicate.table(tableNode))
      break;

    if (AutomationPredicate.cellLike(tableNode)) {
      rowIndex = tableNode.tableCellRowIndex;
      colIndex = tableNode.tableCellColumnIndex;
    }

    tableNode = tableNode.parent;
  }
  if (!tableNode)
    return null;

  // Only support making a predicate for column ends.
  if (opts.end) {
    if (!opts.col)
      throw 'Unsupported option.';

    if (dir == Dir.FORWARD) {
      return function(node) {
        return AutomationPredicate.cellLike(node) &&
            node.tableCellColumnIndex == colIndex &&
            node.tableCellRowIndex >= 0;
      };
    } else {
      return function(node) {
        return AutomationPredicate.cellLike(node) &&
            node.tableCellColumnIndex == colIndex &&
            node.tableCellRowIndex < tableNode.tableRowCount;
      };
    }
  }

  // Adjust for the next/previous row/col.
  if (opts.row)
    rowIndex = dir == Dir.FORWARD ? rowIndex + 1 : rowIndex - 1;
  if (opts.col)
    colIndex = dir == Dir.FORWARD ? colIndex + 1 : colIndex - 1;

  return function(node) {
    return AutomationPredicate.cellLike(node) &&
        node.tableCellColumnIndex == colIndex &&
        node.tableCellRowIndex == rowIndex;
  };
};

/**
 * Returns a predicate that will match against a heading with a specific
 * hierarchical level.
 * @param {number} level 1-6
 * @return {AutomationPredicate.Unary}
 */
AutomationPredicate.makeHeadingPredicate = function(level) {
  return function(node) {
    return node.role == RoleType.heading && node.hierarchicalLevel == level;
  };
};

});  // goog.scope
