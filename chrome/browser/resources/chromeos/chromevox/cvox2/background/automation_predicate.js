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
var Role = chrome.automation.RoleType;

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
 * Constructs a predicate given a list of roles.
 * @param {!Array<Role>} roles
 * @return {AutomationPredicate.Unary}
 */
AutomationPredicate.roles = function(roles) {
  return AutomationPredicate.match({anyRole: roles });
};

/**
 * Constructs a predicate given a list of roles or predicates.
 * @param {{anyRole: (Array<Role>|undefined),
 *          anyPredicate: (Array<AutomationPredicate.Unary>|undefined)}} params
 * @return {AutomationPredicate.Unary}
 */
AutomationPredicate.match = function(params) {
  var anyRole = params.anyRole || [];
  var anyPredicate = params.anyPredicate || [];
  return function(node) {
    return anyRole.some(function(role) { return role == node.role; }) ||
        anyPredicate.some(function(p) { return p(node); });
  };
};

/** @type {AutomationPredicate.Unary} */
AutomationPredicate.checkBox = AutomationPredicate.roles([Role.checkBox]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.comboBox = AutomationPredicate.roles(
    [Role.comboBox, Role.popUpButton, Role.menuListPopup]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.heading = AutomationPredicate.roles([Role.heading]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.inlineTextBox =
    AutomationPredicate.roles([Role.inlineTextBox]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.link = AutomationPredicate.roles([Role.link]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.row = AutomationPredicate.roles([Role.row]);
/** @type {AutomationPredicate.Unary} */
AutomationPredicate.table = AutomationPredicate.roles([Role.grid, Role.table]);

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

/** @type {AutomationPredicate.Unary} */
AutomationPredicate.formField = AutomationPredicate.match({
  anyPredicate: [
    AutomationPredicate.button,
    AutomationPredicate.comboBox,
    AutomationPredicate.editText
  ],
  anyRole: [
    Role.checkBox,
    Role.colorWell,
    Role.listBox,
    Role.slider,
    Role.switch,
    Role.tree
  ]
});

/** @type {AutomationPredicate.Unary} */
AutomationPredicate.control = AutomationPredicate.match({
  anyPredicate: [
    AutomationPredicate.formField,
  ],
  anyRole: [
    Role.disclosureTriangle,
    Role.menuItem,
    Role.menuItemCheckBox,
    Role.menuItemRadio,
    Role.menuListOption,
    Role.scrollBar,
    Role.tab
  ]
});

/** @type {AutomationPredicate.Unary} */
AutomationPredicate.linkOrControl = AutomationPredicate.match({
  anyPredicate: [
    AutomationPredicate.control
  ],
  anyRole: [
    Role.link
  ]
});

/** @type {AutomationPredicate.Unary} */
AutomationPredicate.landmark = AutomationPredicate.roles([
    Role.application,
    Role.banner,
    Role.complementary,
    Role.contentInfo,
    Role.form,
    Role.main,
    Role.navigation,
    Role.region,
    Role.search]);

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
      node.role == Role.button ||
      node.role == Role.buttonDropDown ||
      node.role == Role.popUpButton ||
      node.role == Role.slider ||
      node.role == Role.textField ||
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
      node.role == Role.staticText;
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
  // Editable nodes are within a text-like field and don't make sense when
  // performing object navigation. Users should use line, word, or character
  // navigation. Only navigate to the top level node.
  if (node.parent && node.parent.state.editable)
    return false;

  return node.state.focusable ||
      (AutomationPredicate.leafOrStaticText(node) &&
       (/\S+/.test(node.name) ||
        (node.role != Role.lineBreak &&
         node.role != Role.staticText &&
         node.role != Role.inlineTextBox)));
};

/**
 * Matches against nodes visited during group navigation. An object as
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.group = AutomationPredicate.match({
  anyRole: [
    Role.heading,
    Role.list,
    Role.paragraph
  ],
  anyPredicate: [
    AutomationPredicate.editText,
    AutomationPredicate.formField,
    AutomationPredicate.object,
    AutomationPredicate.table
  ]
});

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
  return AutomationPredicate.match({
    anyRole: [
      Role.div,
      Role.document,
      Role.group,
      Role.listItem,
      Role.toolbar,
      Role.window],
    anyPredicate: [
      AutomationPredicate.landmark,
      AutomationPredicate.structuralContainer,
      function(node) {
        // For example, crosh.
        return (node.role == Role.textField && node.state.readOnly);
      },
      function(node) {
        return (node.state.editable &&
            node.parent &&
            !node.parent.state.editable);
      }]
  })(node);
};

/**
 * Matches against nodes that contain interesting nodes, but should never be
 * visited.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.structuralContainer = AutomationPredicate.roles([
    Role.rootWebArea,
    Role.embeddedObject,
    Role.iframe,
    Role.iframePresentational]);

/**
 * Returns whether the given node should not be crossed when performing
 * traversals up the ancestry chain.
 * @param {AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.root = function(node) {
  switch (node.role) {
    case Role.dialog:
    case Role.window:
      return true;
    case Role.toolbar:
      return node.root.role == Role.desktop;
    case Role.rootWebArea:
      return !node.parent || node.parent.root.role == Role.desktop;
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
  if (node.role == Role.listMarker)
    return true;

  // Don't ignore nodes with names.
  if (node.name || node.value || node.description)
    return false;

  // Ignore some roles.
  return AutomationPredicate.leaf(node) &&
      (AutomationPredicate.roles([Role.client,
                                  Role.column,
                                  Role.div,
                                  Role.group,
                                  Role.image,
                                  Role.staticText,
                                  Role.svgRoot,
                                  Role.tableHeaderContainer])(node));
};

/**
 * Returns if the node has a meaningful checked state.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.checkable = AutomationPredicate.roles([
  Role.checkBox,
  Role.radioButton,
  Role.menuItemCheckBox,
  Role.menuItemRadio,
  Role.treeItem]);

// Table related predicates.
/**
 * Returns if the node has a cell like role.
 * @param {!AutomationNode} node
 * @return {boolean}
 */
AutomationPredicate.cellLike = AutomationPredicate.roles([
  Role.cell,
  Role.rowHeader,
  Role.columnHeader]);

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
    return node.role == Role.heading && node.hierarchicalLevel == level;
  };
};

});  // goog.scope
