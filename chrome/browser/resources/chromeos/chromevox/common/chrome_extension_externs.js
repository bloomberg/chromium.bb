// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Definitions for the Chromium extensions API used by ChromeVox.
 *
 * @externs
 */


// TODO: Move these to //third_party/closure_compiler/externs.

// Begin auto generated externs; do not edit.
// The following was generated from:
//
// python tools/json_schema_compiler/compiler.py
//     -g externs
//     chrome/common/extensions/api/automation.idl

/**
 * @const
 */
chrome.automation = {};

/**
 * @enum {string}
 */
chrome.automation.EventType = {
  activedescendantchanged: 'activedescendantchanged',
  alert: 'alert',
  ariaAttributeChanged: 'ariaAttributeChanged',
  autocorrectionOccured: 'autocorrectionOccured',
  blur: 'blur',
  checkedStateChanged: 'checkedStateChanged',
  childrenChanged: 'childrenChanged',
  focus: 'focus',
  hide: 'hide',
  hover: 'hover',
  invalidStatusChanged: 'invalidStatusChanged',
  layoutComplete: 'layoutComplete',
  liveRegionChanged: 'liveRegionChanged',
  loadComplete: 'loadComplete',
  locationChanged: 'locationChanged',
  menuEnd: 'menuEnd',
  menuListItemSelected: 'menuListItemSelected',
  menuListValueChanged: 'menuListValueChanged',
  menuPopupEnd: 'menuPopupEnd',
  menuPopupStart: 'menuPopupStart',
  menuStart: 'menuStart',
  rowCollapsed: 'rowCollapsed',
  rowCountChanged: 'rowCountChanged',
  rowExpanded: 'rowExpanded',
  scrollPositionChanged: 'scrollPositionChanged',
  scrolledToAnchor: 'scrolledToAnchor',
  selectedChildrenChanged: 'selectedChildrenChanged',
  selection: 'selection',
  selectionAdd: 'selectionAdd',
  selectionRemove: 'selectionRemove',
  show: 'show',
  textChanged: 'textChanged',
  textSelectionChanged: 'textSelectionChanged',
  treeChanged: 'treeChanged',
  valueChanged: 'valueChanged',
};

/**
 * @enum {string}
 */
chrome.automation.RoleType = {
  alertDialog: 'alertDialog',
  alert: 'alert',
  annotation: 'annotation',
  application: 'application',
  article: 'article',
  banner: 'banner',
  blockquote: 'blockquote',
  busyIndicator: 'busyIndicator',
  button: 'button',
  buttonDropDown: 'buttonDropDown',
  canvas: 'canvas',
  caption: 'caption',
  cell: 'cell',
  checkBox: 'checkBox',
  client: 'client',
  colorWell: 'colorWell',
  columnHeader: 'columnHeader',
  column: 'column',
  comboBox: 'comboBox',
  complementary: 'complementary',
  contentInfo: 'contentInfo',
  date: 'date',
  dateTime: 'dateTime',
  definition: 'definition',
  descriptionListDetail: 'descriptionListDetail',
  descriptionList: 'descriptionList',
  descriptionListTerm: 'descriptionListTerm',
  desktop: 'desktop',
  details: 'details',
  dialog: 'dialog',
  directory: 'directory',
  disclosureTriangle: 'disclosureTriangle',
  div: 'div',
  document: 'document',
  embeddedObject: 'embeddedObject',
  figcaption: 'figcaption',
  figure: 'figure',
  footer: 'footer',
  form: 'form',
  grid: 'grid',
  group: 'group',
  heading: 'heading',
  iframe: 'iframe',
  iframePresentational: 'iframePresentational',
  ignored: 'ignored',
  imageMapLink: 'imageMapLink',
  imageMap: 'imageMap',
  image: 'image',
  inlineTextBox: 'inlineTextBox',
  labelText: 'labelText',
  legend: 'legend',
  lineBreak: 'lineBreak',
  link: 'link',
  listBoxOption: 'listBoxOption',
  listBox: 'listBox',
  listItem: 'listItem',
  listMarker: 'listMarker',
  list: 'list',
  locationBar: 'locationBar',
  log: 'log',
  main: 'main',
  marquee: 'marquee',
  math: 'math',
  menuBar: 'menuBar',
  menuButton: 'menuButton',
  menuItem: 'menuItem',
  menuItemCheckBox: 'menuItemCheckBox',
  menuItemRadio: 'menuItemRadio',
  menuListOption: 'menuListOption',
  menuListPopup: 'menuListPopup',
  menu: 'menu',
  meter: 'meter',
  navigation: 'navigation',
  note: 'note',
  outline: 'outline',
  pane: 'pane',
  paragraph: 'paragraph',
  popUpButton: 'popUpButton',
  pre: 'pre',
  presentational: 'presentational',
  progressIndicator: 'progressIndicator',
  radioButton: 'radioButton',
  radioGroup: 'radioGroup',
  region: 'region',
  rootWebArea: 'rootWebArea',
  rowHeader: 'rowHeader',
  row: 'row',
  ruby: 'ruby',
  ruler: 'ruler',
  svgRoot: 'svgRoot',
  scrollArea: 'scrollArea',
  scrollBar: 'scrollBar',
  seamlessWebArea: 'seamlessWebArea',
  search: 'search',
  searchBox: 'searchBox',
  slider: 'slider',
  sliderThumb: 'sliderThumb',
  spinButtonPart: 'spinButtonPart',
  spinButton: 'spinButton',
  splitter: 'splitter',
  staticText: 'staticText',
  status: 'status',
  switch: 'switch',
  tabGroup: 'tabGroup',
  tabList: 'tabList',
  tabPanel: 'tabPanel',
  tab: 'tab',
  tableHeaderContainer: 'tableHeaderContainer',
  table: 'table',
  textField: 'textField',
  time: 'time',
  timer: 'timer',
  titleBar: 'titleBar',
  toggleButton: 'toggleButton',
  toolbar: 'toolbar',
  treeGrid: 'treeGrid',
  treeItem: 'treeItem',
  tree: 'tree',
  unknown: 'unknown',
  tooltip: 'tooltip',
  webArea: 'webArea',
  webView: 'webView',
  window: 'window',
};

/**
 * @enum {string}
 */
chrome.automation.StateType = {
  busy: 'busy',
  checked: 'checked',
  collapsed: 'collapsed',
  default: 'default',
  disabled: 'disabled',
  editable: 'editable',
  enabled: 'enabled',
  expanded: 'expanded',
  focusable: 'focusable',
  focused: 'focused',
  haspopup: 'haspopup',
  horizontal: 'horizontal',
  hovered: 'hovered',
  indeterminate: 'indeterminate',
  invisible: 'invisible',
  linked: 'linked',
  multiselectable: 'multiselectable',
  offscreen: 'offscreen',
  pressed: 'pressed',
  protected: 'protected',
  readOnly: 'readOnly',
  required: 'required',
  selectable: 'selectable',
  selected: 'selected',
  vertical: 'vertical',
  visited: 'visited',
};

/**
 * @enum {string}
 */
chrome.automation.TreeChangeType = {
  nodeCreated: 'nodeCreated',
  subtreeCreated: 'subtreeCreated',
  nodeChanged: 'nodeChanged',
  nodeRemoved: 'nodeRemoved',
};

/**
 * @typedef {{
 *   left: number,
 *   top: number,
 *   width: number,
 *   height: number
 * }}
 */
chrome.automation.Rect;

/**
 * @typedef {{
 *   role: (!chrome.automation.RoleType|undefined),
 *   state: (Object|undefined),
 *   attributes: (Object|undefined)
 * }}
 */
chrome.automation.FindParams;

/**
 * @constructor
 */
chrome.automation.AutomationEvent = function() {};

/**
 * @typedef {{
 *   target: chrome.automation.AutomationNode,
 *   type: !chrome.automation.TreeChangeType
 * }}
 */
chrome.automation.TreeChange;

/**
 * @constructor
 */
chrome.automation.AutomationNode = function() {};


/**
 * Get the automation tree for the tab with the given tabId, or the current tab
 * if no tabID is given, enabling automation if necessary. Returns a tree with a
 * placeholder root node; listen for the "loadComplete" event to get a
 * notification that the tree has fully loaded (the previous root node reference
 * will stop working at or before this point).
 * @param {number} tabId
 * @param {function(chrome.automation.AutomationNode):void} callback
 *     Called when the <code>AutomationNode</code> for the page is available.
 */
chrome.automation.getTree = function(tabId, callback) {};

/**
 * Get the automation tree for the whole desktop which consists of all on screen
 * views. Note this API is currently only supported on Chrome OS.
 * @param {function(!chrome.automation.AutomationNode):void} callback
 *     Called when the <code>AutomationNode</code> for the page is available.
 */
chrome.automation.getDesktop = function(callback) {};

/**
 * Add a tree change observer. Tree change observers are static/global,
 * they listen to tree changes across all trees.
 * @param {function(chrome.automation.TreeChange):void} observer
 *     A listener for tree changes on the <code>AutomationNode</code> tree.
 */
chrome.automation.addTreeChangeObserver = function(observer) {};

/**
 * Remove a tree change observer.
 * @param {function(chrome.automation.TreeChange):void} observer
 *     A listener for tree changes on the <code>AutomationNode</code> tree.
 */
chrome.automation.removeTreeChangeObserver = function(observer) {};

//
// End auto generated externs; do not edit.
//



/**
 * @type {chrome.automation.RoleType}
 */
chrome.automation.AutomationNode.prototype.role;


/**
 * @type {!Object<chrome.automation.StateType, boolean>}
 */
chrome.automation.AutomationNode.prototype.state;


/**
 * @type {number}
 */
chrome.automation.AutomationNode.prototype.indexInParent;


/**
 * @type {string}
 */
chrome.automation.AutomationNode.prototype.name;

/**
 * @type {string}
 */
chrome.automation.AutomationNode.prototype.description;


/**
 * @type {string}
 */
chrome.automation.AutomationNode.prototype.url;


/**
 * @type {string}
 */
chrome.automation.AutomationNode.prototype.docUrl;


/**
 * @type {string}
 */
chrome.automation.AutomationNode.prototype.value;


/**
 * @type {number}
 */
chrome.automation.AutomationNode.prototype.textSelStart;


/**
 * @type {number}
 */
chrome.automation.AutomationNode.prototype.textSelEnd;


/**
 * @type {Array<number>}
 */
chrome.automation.AutomationNode.prototype.wordStarts;


/**
 * @type {Array<number>}
 */
chrome.automation.AutomationNode.prototype.wordEnds;


/**
 * @type {!chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.root;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.firstChild;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.lastChild;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.nextSibling;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.previousSibling;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.parent;


/**
 * @type {!Array<chrome.automation.AutomationNode>}
 */
chrome.automation.AutomationNode.prototype.children;


/**
 * @type {{top: number, left: number, height: number, width: number}}
 */
chrome.automation.AutomationNode.prototype.location;


/**
 * @param {chrome.automation.EventType} eventType
 * @param {function(!chrome.automation.AutomationEvent) : void} callback
 * @param {boolean} capture
 */
chrome.automation.AutomationNode.prototype.addEventListener =
    function(eventType, callback, capture) {};


/**
 * @param {chrome.automation.EventType} eventType
 * @param {function(!chrome.automation.AutomationEvent) : void} callback
 * @param {boolean} capture
 */
chrome.automation.AutomationNode.prototype.removeEventListener =
    function(eventType, callback, capture) {};


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.TreeChange.prototype.target;


/**
 * @type {chrome.automation.TreeChangeType}
 */
chrome.automation.TreeChange.prototype.type;


/**
 * @param {function(chrome.automation.TreeChange) : void}
 *    callback
 */
chrome.automation.AutomationNode.prototype.addTreeChangeObserver =
    function(callback) {};


/**
 * @param {function(chrome.automation.TreeChange) : void}
 *    callback
 */
chrome.automation.AutomationNode.prototype.removeTreeChangeObserver =
    function(callback) {};


chrome.automation.AutomationNode.prototype.doDefault = function() {};


chrome.automation.AutomationNode.prototype.focus = function() {};


chrome.automation.AutomationNode.prototype.showContextMenu = function() {};


/**
 * @param {number} start
 * @param {number} end
 */
chrome.automation.AutomationNode.prototype.setSelection =
    function(start, end) {};


/** @type {string} */
chrome.automation.AutomationNode.prototype.containerLiveStatus;

/** @type {string} */
chrome.automation.AutomationNode.prototype.containerLiveRelevant;

/** @type {boolean} */
chrome.automation.AutomationNode.prototype.containerLiveAtomic;

/** @type {boolean} */
chrome.automation.AutomationNode.prototype.containerLiveBusy;


/**
 * @param {Object} findParams
 */
chrome.automation.AutomationNode.prototype.find = function(findParams) {};

/**
 * @type {string}
 */
chrome.automation.AutomationNode.prototype.inputType;
