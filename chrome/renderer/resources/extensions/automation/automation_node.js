// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AutomationEvent = require('automationEvent').AutomationEvent;
var automationInternal =
    require('binding').Binding.create('automationInternal').generate();
var IsInteractPermitted =
    requireNative('automationInternal').IsInteractPermitted;

/**
 * @param {number} axTreeID The id of the accessibility tree.
 * @return {?number} The id of the root node.
 */
var GetRootID = requireNative('automationInternal').GetRootID;

/**
 * @param {number} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?number} The id of the node's parent, or undefined if it's the
 *    root of its tree or if the tree or node wasn't found.
 */
var GetParentID = requireNative('automationInternal').GetParentID;

/**
 * @param {number} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?number} The number of children of the node, or undefined if
 *     the tree or node wasn't found.
 */
var GetChildCount = requireNative('automationInternal').GetChildCount;

/**
 * @param {number} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {number} childIndex An index of a child of this node.
 * @return {?number} The id of the child at the given index, or undefined
 *     if the tree or node or child at that index wasn't found.
 */
var GetChildIDAtIndex = requireNative('automationInternal').GetChildIDAtIndex;

/**
 * @param {number} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?number} The index of this node in its parent, or undefined if
 *     the tree or node or node parent wasn't found.
 */
var GetIndexInParent = requireNative('automationInternal').GetIndexInParent;

/**
 * @param {number} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?Object} An object with a string key for every state flag set,
 *     or undefined if the tree or node or node parent wasn't found.
 */
var GetState = requireNative('automationInternal').GetState;

/**
 * @param {number} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {string} The role of the node, or undefined if the tree or
 *     node wasn't found.
 */
var GetRole = requireNative('automationInternal').GetRole;

/**
 * @param {number} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?automation.Rect} The location of the node, or undefined if
 *     the tree or node wasn't found.
 */
var GetLocation = requireNative('automationInternal').GetLocation;

/**
 * @param {number} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of a string attribute.
 * @return {?string} The value of this attribute, or undefined if the tree,
 *     node, or attribute wasn't found.
 */
var GetStringAttribute = requireNative('automationInternal').GetStringAttribute;

/**
 * @param {number} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?boolean} The value of this attribute, or undefined if the tree,
 *     node, or attribute wasn't found.
 */
var GetBoolAttribute = requireNative('automationInternal').GetBoolAttribute;

/**
 * @param {number} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?number} The value of this attribute, or undefined if the tree,
 *     node, or attribute wasn't found.
 */
var GetIntAttribute = requireNative('automationInternal').GetIntAttribute;

/**
 * @param {number} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?number} The value of this attribute, or undefined if the tree,
 *     node, or attribute wasn't found.
 */
var GetFloatAttribute = requireNative('automationInternal').GetFloatAttribute;

/**
 * @param {number} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?Array.<number>} The value of this attribute, or undefined
 *     if the tree, node, or attribute wasn't found.
 */
var GetIntListAttribute =
    requireNative('automationInternal').GetIntListAttribute;

/**
 * @param {number} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an HTML attribute.
 * @return {?string} The value of this attribute, or undefined if the tree,
 *     node, or attribute wasn't found.
 */
var GetHtmlAttribute = requireNative('automationInternal').GetHtmlAttribute;

var lastError = require('lastError');
var logging = requireNative('logging');
var schema = requireNative('automationInternal').GetSchemaAdditions();
var utils = require('utils');

/**
 * A single node in the Automation tree.
 * @param {AutomationRootNodeImpl} root The root of the tree.
 * @constructor
 */
function AutomationNodeImpl(root) {
  this.rootImpl = root;
  // Public attributes. No actual data gets set on this object.
  this.listeners = {};
}

AutomationNodeImpl.prototype = {
  treeID: -1,
  id: -1,
  role: '',
  state: { busy: true },
  isRootNode: false,

  get root() {
    return this.rootImpl.wrapper;
  },

  get parent() {
    if (this.hostNode_)
      return this.hostNode_;
    var parentID = GetParentID(this.treeID, this.id);
    return this.rootImpl.get(parentID);
  },

  get state() {
    return GetState(this.treeID, this.id);
  },

  get role() {
    return GetRole(this.treeID, this.id);
  },

  get location() {
    return GetLocation(this.treeID, this.id);
  },

  get indexInParent() {
    return GetIndexInParent(this.treeID, this.id);
  },

  get childTree() {
    var childTreeID = GetIntAttribute(this.treeID, this.id, 'childTreeId');
    if (childTreeID)
      return AutomationRootNodeImpl.get(childTreeID);
  },

  get firstChild() {
    if (this.childTree)
      return this.childTree;
    if (!GetChildCount(this.treeID, this.id))
      return undefined;
    var firstChildID = GetChildIDAtIndex(this.treeID, this.id, 0);
    return this.rootImpl.get(firstChildID);
  },

  get lastChild() {
    if (this.childTree)
      return this.childTree;
    var count = GetChildCount(this.treeID, this.id);
    if (!count)
      return undefined;
    var lastChildID = GetChildIDAtIndex(this.treeID, this.id, count - 1);
    return this.rootImpl.get(lastChildID);
  },

  get children() {
    if (this.childTree)
      return [this.childTree];

    var children = [];
    var count = GetChildCount(this.treeID, this.id);
    for (var i = 0; i < count; ++i) {
      var childID = GetChildIDAtIndex(this.treeID, this.id, i);
      var child = this.rootImpl.get(childID);
      children.push(child);
    }
    return children;
  },

  get previousSibling() {
    var parent = this.parent;
    var indexInParent = GetIndexInParent(this.treeID, this.id);
    if (parent && indexInParent > 0)
      return parent.children[indexInParent - 1];
    return undefined;
  },

  get nextSibling() {
    var parent = this.parent;
    var indexInParent = GetIndexInParent(this.treeID, this.id);
    if (parent && indexInParent < parent.children.length)
      return parent.children[indexInParent + 1];
    return undefined;
  },

  doDefault: function() {
    this.performAction_('doDefault');
  },

  focus: function() {
    this.performAction_('focus');
  },

  makeVisible: function() {
    this.performAction_('makeVisible');
  },

  setSelection: function(startIndex, endIndex) {
    this.performAction_('setSelection',
                        { startIndex: startIndex,
                          endIndex: endIndex });
  },

  showContextMenu: function() {
    this.performAction_('showContextMenu');
  },

  domQuerySelector: function(selector, callback) {
    automationInternal.querySelector(
      { treeID: this.rootImpl.treeID,
        automationNodeID: this.id,
        selector: selector },
      this.domQuerySelectorCallback_.bind(this, callback));
  },

  find: function(params) {
    return this.findInternal_(params);
  },

  findAll: function(params) {
    return this.findInternal_(params, []);
  },

  matches: function(params) {
    return this.matchInternal_(params);
  },

  addEventListener: function(eventType, callback, capture) {
    this.removeEventListener(eventType, callback);
    if (!this.listeners[eventType])
      this.listeners[eventType] = [];
    this.listeners[eventType].push({callback: callback, capture: !!capture});
  },

  // TODO(dtseng/aboxhall): Check this impl against spec.
  removeEventListener: function(eventType, callback) {
    if (this.listeners[eventType]) {
      var listeners = this.listeners[eventType];
      for (var i = 0; i < listeners.length; i++) {
        if (callback === listeners[i].callback)
          listeners.splice(i, 1);
      }
    }
  },

  toJSON: function() {
    return { treeID: this.treeID,
             id: this.id,
             role: this.role,
             attributes: this.attributes };
  },

  dispatchEvent: function(eventType) {
    var path = [];
    var parent = this.parent;
    while (parent) {
      path.push(parent);
      parent = parent.parent;
    }
    var event = new AutomationEvent(eventType, this.wrapper);

    // Dispatch the event through the propagation path in three phases:
    // - capturing: starting from the root and going down to the target's parent
    // - targeting: dispatching the event on the target itself
    // - bubbling: starting from the target's parent, going back up to the root.
    // At any stage, a listener may call stopPropagation() on the event, which
    // will immediately stop event propagation through this path.
    if (this.dispatchEventAtCapturing_(event, path)) {
      if (this.dispatchEventAtTargeting_(event, path))
        this.dispatchEventAtBubbling_(event, path);
    }
  },

  toString: function() {
    var impl = privates(this).impl;
    if (!impl)
      impl = this;

    var parentID = GetParentID(this.treeID, this.id);
    var count = GetChildCount(this.treeID, this.id);
    var childIDs = [];
    for (var i = 0; i < count; ++i) {
      var childID = GetChildIDAtIndex(this.treeID, this.id, i);
      childIDs.push(childID);
    }

    return 'node id=' + impl.id +
        ' role=' + this.role +
        ' state=' + $JSON.stringify(this.state) +
        ' parentID=' + parentID +
        ' childIds=' + $JSON.stringify(childIDs);
  },

  dispatchEventAtCapturing_: function(event, path) {
    privates(event).impl.eventPhase = Event.CAPTURING_PHASE;
    for (var i = path.length - 1; i >= 0; i--) {
      this.fireEventListeners_(path[i], event);
      if (privates(event).impl.propagationStopped)
        return false;
    }
    return true;
  },

  dispatchEventAtTargeting_: function(event) {
    privates(event).impl.eventPhase = Event.AT_TARGET;
    this.fireEventListeners_(this.wrapper, event);
    return !privates(event).impl.propagationStopped;
  },

  dispatchEventAtBubbling_: function(event, path) {
    privates(event).impl.eventPhase = Event.BUBBLING_PHASE;
    for (var i = 0; i < path.length; i++) {
      this.fireEventListeners_(path[i], event);
      if (privates(event).impl.propagationStopped)
        return false;
    }
    return true;
  },

  fireEventListeners_: function(node, event) {
    var nodeImpl = privates(node).impl;
    var listeners = nodeImpl.listeners[event.type];
    if (!listeners)
      return;
    var eventPhase = event.eventPhase;
    for (var i = 0; i < listeners.length; i++) {
      if (eventPhase == Event.CAPTURING_PHASE && !listeners[i].capture)
        continue;
      if (eventPhase == Event.BUBBLING_PHASE && listeners[i].capture)
        continue;

      try {
        listeners[i].callback(event);
      } catch (e) {
        logging.WARNING('Error in event handler for ' + event.type +
                        ' during phase ' + eventPhase + ': ' +
                        e.message + '\nStack trace: ' + e.stack);
      }
    }
  },

  performAction_: function(actionType, opt_args) {
    // Not yet initialized.
    if (this.rootImpl.treeID === undefined ||
        this.id === undefined) {
      return;
    }

    // Check permissions.
    if (!IsInteractPermitted()) {
      throw new Error(actionType + ' requires {"desktop": true} or' +
          ' {"interact": true} in the "automation" manifest key.');
    }

    automationInternal.performAction({ treeID: this.rootImpl.treeID,
                                       automationNodeID: this.id,
                                       actionType: actionType },
                                     opt_args || {});
  },

  domQuerySelectorCallback_: function(userCallback, resultAutomationNodeID) {
    // resultAutomationNodeID could be zero or undefined or (unlikely) null;
    // they all amount to the same thing here, which is that no node was
    // returned.
    if (!resultAutomationNodeID) {
      userCallback(null);
      return;
    }
    var resultNode = this.rootImpl.get(resultAutomationNodeID);
    if (!resultNode) {
      logging.WARNING('Query selector result not in tree: ' +
                      resultAutomationNodeID);
      userCallback(null);
    }
    userCallback(resultNode);
  },

  findInternal_: function(params, opt_results) {
    var result = null;
    this.forAllDescendants_(function(node) {
      if (privates(node).impl.matchInternal_(params)) {
        if (opt_results)
          opt_results.push(node);
        else
          result = node;
        return !opt_results;
      }
    });
    if (opt_results)
      return opt_results;
    return result;
  },

  /**
   * Executes a closure for all of this node's descendants, in pre-order.
   * Early-outs if the closure returns true.
   * @param {Function(AutomationNode):boolean} closure Closure to be executed
   *     for each node. Return true to early-out the traversal.
   */
  forAllDescendants_: function(closure) {
    var stack = this.wrapper.children.reverse();
    while (stack.length > 0) {
      var node = stack.pop();
      if (closure(node))
        return;

      var children = node.children;
      for (var i = children.length - 1; i >= 0; i--)
        stack.push(children[i]);
    }
  },

  matchInternal_: function(params) {
    if (Object.keys(params).length == 0)
      return false;

    if ('role' in params && this.role != params.role)
        return false;

    if ('state' in params) {
      for (var state in params.state) {
        if (params.state[state] != (state in this.state))
          return false;
      }
    }
    if ('attributes' in params) {
      for (var attribute in params.attributes) {
        var attrValue = params.attributes[attribute];
        if (typeof attrValue != 'object') {
          if (this[attribute] !== attrValue)
            return false;
        } else if (attrValue instanceof RegExp) {
          if (typeof this[attribute] != 'string')
            return false;
          if (!attrValue.test(this[attribute]))
            return false;
        } else {
          // TODO(aboxhall): handle intlist case.
          return false;
        }
      }
    }
    return true;
  }
};

var stringAttributes = [
    'accessKey',
    'action',
    'ariaInvalidValue',
    'autoComplete',
    'containerLiveRelevant',
    'containerLiveStatus',
    'description',
    'display',
    'docDoctype',
    'docMimetype',
    'docTitle',
    'docUrl',
    'dropeffect',
    'help',
    'htmlTag',
    'liveRelevant',
    'liveStatus',
    'name',
    'placeholder',
    'shortcut',
    'textInputType',
    'url',
    'value'];

var boolAttributes = [
    'ariaReadonly',
    'buttonMixed',
    'canSetValue',
    'canvasHasFallback',
    'containerLiveAtomic',
    'containerLiveBusy',
    'docLoaded',
    'grabbed',
    'isAxTreeHost',
    'liveAtomic',
    'liveBusy',
    'updateLocationOnly'];

var intAttributes = [
    'anchorOffset',
    'backgroundColor',
    'color',
    'colorValue',
    'focusOffset',
    'hierarchicalLevel',
    'invalidState',
    'posInSet',
    'scrollX',
    'scrollXMax',
    'scrollXMin',
    'scrollY',
    'scrollYMax',
    'scrollYMin',
    'setSize',
    'sortDirection',
    'tableCellColumnIndex',
    'tableCellColumnSpan',
    'tableCellRowIndex',
    'tableCellRowSpan',
    'tableColumnCount',
    'tableColumnIndex',
    'tableRowCount',
    'tableRowIndex',
    'textDirection',
    'textSelEnd',
    'textSelStart',
    'textStyle'];

var nodeRefAttributes = [
    ['activedescendantId', 'activedescendant'],
    ['anchorObjectId', 'anchorObject'],
    ['focusObjectId', 'focusObject'],
    ['tableColumnHeaderId', 'tableColumnHeader'],
    ['tableHeaderId', 'tableHeader'],
    ['tableRowHeaderId', 'tableRowHeader'],
    ['titleUiElement', 'titleUIElement']];

var intListAttributes = [
    'characterOffsets',
    'lineBreaks',
    'wordEnds',
    'wordStarts'];

var nodeRefListAttributes = [
    ['cellIds', 'cells'],
    ['controlsIds', 'controls'],
    ['describedbyIds', 'describedBy'],
    ['flowtoIds', 'flowTo'],
    ['labelledbyIds', 'labelledBy'],
    ['uniqueCellIds', 'uniqueCells']];

var floatAttributes = [
    'docLoadingProgress',
    'valueForRange',
    'minValueForRange',
    'maxValueForRange',
    'fontSize'];

var htmlAttributes = [
    ['type', 'inputType']];

var publicAttributes = [];

stringAttributes.forEach(function (attributeName) {
  publicAttributes.push(attributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    get: function() {
      return GetStringAttribute(this.treeID, this.id, attributeName);
    }
  });
});

boolAttributes.forEach(function (attributeName) {
  publicAttributes.push(attributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    get: function() {
      return GetBoolAttribute(this.treeID, this.id, attributeName);
    }
  });
});

intAttributes.forEach(function (attributeName) {
  publicAttributes.push(attributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    get: function() {
      return GetIntAttribute(this.treeID, this.id, attributeName);
    }
  });
});

nodeRefAttributes.forEach(function (params) {
  var srcAttributeName = params[0];
  var dstAttributeName = params[1];
  publicAttributes.push(dstAttributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, dstAttributeName, {
    get: function() {
      var id = GetIntAttribute(this.treeID, this.id, srcAttributeName);
      if (id)
        return this.rootImpl.get(id);
      else
        return undefined;
    }
  });
});

intListAttributes.forEach(function (attributeName) {
  publicAttributes.push(attributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    get: function() {
      return GetIntListAttribute(this.treeID, this.id, attributeName);
    }
  });
});

nodeRefListAttributes.forEach(function (params) {
  var srcAttributeName = params[0];
  var dstAttributeName = params[1];
  publicAttributes.push(dstAttributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, dstAttributeName, {
    get: function() {
      var ids = GetIntListAttribute(this.treeID, this.id, srcAttributeName);
      if (!ids)
        return undefined;
      var result = [];
      for (var i = 0; i < ids.length; ++i) {
        var node = this.rootImpl.get(ids[i]);
        if (node)
          result.push(node);
      }
      return result;
    }
  });
});

floatAttributes.forEach(function (attributeName) {
  publicAttributes.push(attributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    get: function() {
      return GetFloatAttribute(this.treeID, this.id, attributeName);
    }
  });
});

htmlAttributes.forEach(function (params) {
  var srcAttributeName = params[0];
  var dstAttributeName = params[1];
  publicAttributes.push(dstAttributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, dstAttributeName, {
    get: function() {
      return GetHtmlAttribute(this.treeID, this.id, srcAttributeName);
    }
  });
});

/**
 * AutomationRootNode.
 *
 * An AutomationRootNode is the javascript end of an AXTree living in the
 * browser. AutomationRootNode handles unserializing incremental updates from
 * the source AXTree. Each update contains node data that form a complete tree
 * after applying the update.
 *
 * A brief note about ids used through this class. The source AXTree assigns
 * unique ids per node and we use these ids to build a hash to the actual
 * AutomationNode object.
 * Thus, tree traversals amount to a lookup in our hash.
 *
 * The tree itself is identified by the accessibility tree id of the
 * renderer widget host.
 * @constructor
 */
function AutomationRootNodeImpl(treeID) {
  AutomationNodeImpl.call(this, this);
  this.treeID = treeID;
  this.axNodeDataCache_ = {};
}

AutomationRootNodeImpl.idToAutomationRootNode_ = {};

AutomationRootNodeImpl.get = function(treeID) {
  var result = AutomationRootNodeImpl.idToAutomationRootNode_[treeID];
  return result || undefined;
}

AutomationRootNodeImpl.getOrCreate = function(treeID) {
  if (AutomationRootNodeImpl.idToAutomationRootNode_[treeID])
    return AutomationRootNodeImpl.idToAutomationRootNode_[treeID];
  var result = new AutomationRootNode(treeID);
  AutomationRootNodeImpl.idToAutomationRootNode_[treeID] = result;
  return result;
}

AutomationRootNodeImpl.destroy = function(treeID) {
  delete AutomationRootNodeImpl.idToAutomationRootNode_[treeID];
}

AutomationRootNodeImpl.prototype = {
  __proto__: AutomationNodeImpl.prototype,

  /**
   * @type {boolean}
   */
  isRootNode: true,

  /**
   * @type {number}
   */
  treeID: -1,

  /**
   * The parent of this node from a different tree.
   * @type {?AutomationNode}
   * @private
   */
  hostNode_: null,

  /**
   * A map from id to AutomationNode.
   * @type {Object.<number, AutomationNode>}
   * @private
   */
  axNodeDataCache_: null,

  get id() {
    return GetRootID(this.treeID);
  },

  get: function(id) {
    if (id == undefined)
      return undefined;

    if (id == this.id)
      return this.wrapper;

    var obj = this.axNodeDataCache_[id];
    if (obj)
      return obj;

    obj = new AutomationNode(this);
    privates(obj).impl.treeID = this.treeID;
    privates(obj).impl.id = id;
    this.axNodeDataCache_[id] = obj;

    return obj;
  },

  remove: function(id) {
    delete this.axNodeDataCache_[id];
  },

  destroy: function() {
    this.dispatchEvent(schema.EventType.destroyed);
  },

  setHostNode(hostNode) {
    this.hostNode_ = hostNode;
  },

  onAccessibilityEvent: function(eventParams) {
    var targetNode = this.get(eventParams.targetID);
    if (targetNode) {
      var targetNodeImpl = privates(targetNode).impl;
      targetNodeImpl.dispatchEvent(eventParams.eventType);
    } else {
      logging.WARNING('Got ' + eventParams.eventType +
                      ' event on unknown node: ' + eventParams.targetID +
                      '; this: ' + this.id);
    }
    return true;
  },

  toString: function() {
    function toStringInternal(node, indent) {
      if (!node)
        return '';
      var output =
          new Array(indent).join(' ') +
          AutomationNodeImpl.prototype.toString.call(node) +
          '\n';
      indent += 2;
      for (var i = 0; i < node.children.length; i++)
        output += toStringInternal(node.children[i], indent);
      return output;
    }
    return toStringInternal(this, 0);
  },
};

var AutomationNode = utils.expose('AutomationNode',
                                  AutomationNodeImpl,
                                  { functions: ['doDefault',
                                                'find',
                                                'findAll',
                                                'focus',
                                                'makeVisible',
                                                'matches',
                                                'setSelection',
                                                'showContextMenu',
                                                'addEventListener',
                                                'removeEventListener',
                                                'domQuerySelector',
                                                'toString' ],
                                    readonly: publicAttributes.concat(
                                              ['parent',
                                               'firstChild',
                                               'lastChild',
                                               'children',
                                               'previousSibling',
                                               'nextSibling',
                                               'isRootNode',
                                               'role',
                                               'state',
                                               'location',
                                               'indexInParent',
                                               'root']) });

var AutomationRootNode = utils.expose('AutomationRootNode',
                                      AutomationRootNodeImpl,
                                      { superclass: AutomationNode });

AutomationRootNode.get = function(treeID) {
  return AutomationRootNodeImpl.get(treeID);
}

AutomationRootNode.getOrCreate = function(treeID) {
  return AutomationRootNodeImpl.getOrCreate(treeID);
}

AutomationRootNode.destroy = function(treeID) {
  AutomationRootNodeImpl.destroy(treeID);
}

exports.AutomationNode = AutomationNode;
exports.AutomationRootNode = AutomationRootNode;
