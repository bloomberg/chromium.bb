// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var AutomationEvent = require('automationEvent').AutomationEvent;
var automationInternal =
    require('binding').Binding.create('automationInternal').generate();
var IsInteractPermitted =
    requireNative('automationInternal').IsInteractPermitted;

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
  this.childIds = [];
  // Public attributes. No actual data gets set on this object.
  this.attributes = {};
  // Internal object holding all attributes.
  this.attributesInternal = {};
  this.listeners = {};
  this.location = { left: 0, top: 0, width: 0, height: 0 };
}

AutomationNodeImpl.prototype = {
  id: -1,
  role: '',
  state: { busy: true },
  isRootNode: false,

  get root() {
    return this.rootImpl.wrapper;
  },

  get parent() {
    return this.hostTree || this.rootImpl.get(this.parentID);
  },

  get firstChild() {
    return this.childTree || this.rootImpl.get(this.childIds[0]);
  },

  get lastChild() {
    var childIds = this.childIds;
    return this.childTree || this.rootImpl.get(childIds[childIds.length - 1]);
  },

  get children() {
    if (this.childTree)
      return [this.childTree];

    var children = [];
    for (var i = 0, childID; childID = this.childIds[i]; i++) {
      logging.CHECK(this.rootImpl.get(childID));
      children.push(this.rootImpl.get(childID));
    }
    return children;
  },

  get previousSibling() {
    var parent = this.parent;
    if (parent && this.indexInParent > 0)
      return parent.children[this.indexInParent - 1];
    return undefined;
  },

  get nextSibling() {
    var parent = this.parent;
    if (parent && this.indexInParent < parent.children.length)
      return parent.children[this.indexInParent + 1];
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
    return 'node id=' + impl.id +
        ' role=' + this.role +
        ' state=' + $JSON.stringify(this.state) +
        ' parentID=' + impl.parentID +
        ' childIds=' + $JSON.stringify(impl.childIds) +
        ' attributes=' + $JSON.stringify(this.attributes);
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
        console.error('Error in event handler for ' + event.type +
                      'during phase ' + eventPhase + ': ' +
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
        if (!(attribute in this.attributesInternal))
          return false;

        var attrValue = params.attributes[attribute];
        if (typeof attrValue != 'object') {
          if (this.attributesInternal[attribute] !== attrValue)
            return false;
        } else if (attrValue instanceof RegExp) {
          if (typeof this.attributesInternal[attribute] != 'string')
            return false;
          if (!attrValue.test(this.attributesInternal[attribute]))
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

// Maps an attribute to its default value in an invalidated node.
// These attributes are taken directly from the Automation idl.
var AutomationAttributeDefaults = {
  'id': -1,
  'role': '',
  'state': {},
  'location': { left: 0, top: 0, width: 0, height: 0 }
};


var AutomationAttributeTypes = [
  'boolAttributes',
  'floatAttributes',
  'htmlAttributes',
  'intAttributes',
  'intlistAttributes',
  'stringAttributes'
];

/**
 * Maps an attribute name to another attribute who's value is an id or an array
 * of ids referencing an AutomationNode.
 * @param {!Object<string, string>}
 * @const
 */
var ATTRIBUTE_NAME_TO_ID_ATTRIBUTE = {
  'aria-activedescendant': 'activedescendantId',
  'aria-controls': 'controlsIds',
  'aria-describedby': 'describedbyIds',
  'aria-flowto': 'flowtoIds',
  'aria-labelledby': 'labelledbyIds',
  'aria-owns': 'ownsIds'
};

/**
 * A set of attributes ignored in the automation API.
 * @param {!Object<string, boolean>}
 * @const
 */
var ATTRIBUTE_BLACKLIST = {'activedescendantId': true,
                           'childTreeId': true,
                           'controlsIds': true,
                           'describedbyIds': true,
                           'flowtoIds': true,
                           'labelledbyIds': true,
                           'ownsIds': true
};

function defaultStringAttribute(opt_defaultVal) {
  var defaultVal = (opt_defaultVal !== undefined) ? opt_defaultVal : '';
  return { default: defaultVal, reflectFrom: 'stringAttributes' };
}

function defaultIntAttribute(opt_defaultVal) {
  var defaultVal = (opt_defaultVal !== undefined) ? opt_defaultVal : 0;
  return { default: defaultVal, reflectFrom: 'intAttributes' };
}

function defaultFloatAttribute(opt_defaultVal) {
  var defaultVal = (opt_defaultVal !== undefined) ? opt_defaultVal : 0;
  return { default: defaultVal, reflectFrom: 'floatAttributes' };
}

function defaultBoolAttribute(opt_defaultVal) {
  var defaultVal = (opt_defaultVal !== undefined) ? opt_defaultVal : false;
  return { default: defaultVal, reflectFrom: 'boolAttributes' };
}

function defaultHtmlAttribute(opt_defaultVal) {
  var defaultVal = (opt_defaultVal !== undefined) ? opt_defaultVal : '';
  return { default: defaultVal, reflectFrom: 'htmlAttributes' };
}

function defaultIntListAttribute(opt_defaultVal) {
  var defaultVal = (opt_defaultVal !== undefined) ? opt_defaultVal : [];
  return { default: defaultVal, reflectFrom: 'intlistAttributes' };
}

function defaultNodeRefAttribute(idAttribute, opt_defaultVal) {
  var defaultVal = (opt_defaultVal !== undefined) ? opt_defaultVal : null;
  return { default: defaultVal,
           idFrom: 'intAttributes',
           idAttribute: idAttribute,
           isRef: true };
}

function defaultNodeRefListAttribute(idAttribute, opt_defaultVal) {
  var defaultVal = (opt_defaultVal !== undefined) ? opt_defaultVal : [];
  return { default: [],
           idFrom: 'intlistAttributes',
           idAttribute: idAttribute,
           isRef: true };
}

// Maps an attribute to its default value in an invalidated node.
// These attributes are taken directly from the Automation idl.
var DefaultMixinAttributes = {
  description: defaultStringAttribute(),
  help: defaultStringAttribute(),
  name: defaultStringAttribute(),
  value: defaultStringAttribute(),
  controls: defaultNodeRefListAttribute('controlsIds'),
  describedby: defaultNodeRefListAttribute('describedbyIds'),
  flowto: defaultNodeRefListAttribute('flowtoIds'),
  labelledby: defaultNodeRefListAttribute('labelledbyIds'),
  owns: defaultNodeRefListAttribute('ownsIds')
};

var ActiveDescendantMixinAttribute = {
  activedescendant: defaultNodeRefAttribute('activedescendantId')
};

var LinkMixinAttributes = {
  url: defaultStringAttribute()
};

var DocumentMixinAttributes = {
  docUrl: defaultStringAttribute(),
  docTitle: defaultStringAttribute(),
  docLoaded: defaultStringAttribute(),
  docLoadingProgress: defaultFloatAttribute()
};

var ScrollableMixinAttributes = {
  scrollX: defaultIntAttribute(),
  scrollXMin: defaultIntAttribute(),
  scrollXMax: defaultIntAttribute(),
  scrollY: defaultIntAttribute(),
  scrollYMin: defaultIntAttribute(),
  scrollYMax: defaultIntAttribute()
};

var EditableTextMixinAttributes = {
  textSelStart: defaultIntAttribute(-1),
  textSelEnd: defaultIntAttribute(-1)
};

var RangeMixinAttributes = {
  valueForRange: defaultFloatAttribute(),
  minValueForRange: defaultFloatAttribute(),
  maxValueForRange: defaultFloatAttribute()
};

var TableMixinAttributes = {
  tableRowCount: defaultIntAttribute(),
  tableColumnCount: defaultIntAttribute()
};

var TableCellMixinAttributes = {
  tableCellColumnIndex: defaultIntAttribute(),
  tableCellColumnSpan: defaultIntAttribute(1),
  tableCellRowIndex: defaultIntAttribute(),
  tableCellRowSpan: defaultIntAttribute(1)
};

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

AutomationRootNodeImpl.prototype = {
  __proto__: AutomationNodeImpl.prototype,

  isRootNode: true,
  treeID: -1,

  get: function(id) {
    if (id == undefined)
      return undefined;

    return this.axNodeDataCache_[id];
  },

  unserialize: function(update) {
    var updateState = { pendingNodes: {}, newNodes: {} };
    var oldRootId = this.id;

    if (update.nodeIdToClear < 0) {
        logging.WARNING('Bad nodeIdToClear: ' + update.nodeIdToClear);
        lastError.set('automation',
                      'Bad update received on automation tree',
                      null,
                      chrome);
        return false;
    } else if (update.nodeIdToClear > 0) {
      var nodeToClear = this.axNodeDataCache_[update.nodeIdToClear];
      if (!nodeToClear) {
        logging.WARNING('Bad nodeIdToClear: ' + update.nodeIdToClear +
                        ' (not in cache)');
        lastError.set('automation',
                      'Bad update received on automation tree',
                      null,
                      chrome);
        return false;
      }
      if (nodeToClear === this.wrapper) {
        this.invalidate_(nodeToClear);
      } else {
        var children = nodeToClear.children;
        for (var i = 0; i < children.length; i++)
          this.invalidate_(children[i]);
        var nodeToClearImpl = privates(nodeToClear).impl;
        nodeToClearImpl.childIds = []
        updateState.pendingNodes[nodeToClearImpl.id] = nodeToClear;
      }
    }

    for (var i = 0; i < update.nodes.length; i++) {
      if (!this.updateNode_(update.nodes[i], updateState))
        return false;
    }

    if (Object.keys(updateState.pendingNodes).length > 0) {
      logging.WARNING('Nodes left pending by the update: ' +
          $JSON.stringify(updateState.pendingNodes));
      lastError.set('automation',
                    'Bad update received on automation tree',
                    null,
                    chrome);
      return false;
    }
    return true;
  },

  destroy: function() {
    if (this.hostTree)
      this.hostTree.childTree = undefined;
    this.hostTree = undefined;

    this.dispatchEvent(schema.EventType.destroyed);
    this.invalidate_(this.wrapper);
  },

  onAccessibilityEvent: function(eventParams) {
    if (!this.unserialize(eventParams.update)) {
      logging.WARNING('unserialization failed');
      return false;
    }

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

  invalidate_: function(node) {
    if (!node)
      return;
    var children = node.children;

    for (var i = 0, child; child = children[i]; i++) {
      // Do not invalidate into subrooted nodes.
      // TODO(dtseng): Revisit logic once out of proc iframes land.
      if (child.root != node.root)
        continue;
      this.invalidate_(child);
    }

    // Retrieve the internal AutomationNodeImpl instance for this node.
    // This object is not accessible outside of bindings code, but we can access
    // it here.
    var nodeImpl = privates(node).impl;
    var id = nodeImpl.id;
    for (var key in AutomationAttributeDefaults) {
      nodeImpl[key] = AutomationAttributeDefaults[key];
    }

    nodeImpl.attributesInternal = {};
    for (var key in DefaultMixinAttributes) {
      var mixinAttribute = DefaultMixinAttributes[key];
      if (!mixinAttribute.isRef)
        nodeImpl.attributesInternal[key] = mixinAttribute.default;
    }
    nodeImpl.childIds = [];
    nodeImpl.id = id;
    delete this.axNodeDataCache_[id];
  },

  deleteOldChildren_: function(node, newChildIds) {
    // Create a set of child ids in |src| for fast lookup, and return false
    // if a duplicate is found;
    var newChildIdSet = {};
    for (var i = 0; i < newChildIds.length; i++) {
      var childId = newChildIds[i];
      if (newChildIdSet[childId]) {
        logging.WARNING('Node ' + privates(node).impl.id +
                        ' has duplicate child id ' + childId);
        lastError.set('automation',
                      'Bad update received on automation tree',
                      null,
                      chrome);
        return false;
      }
      newChildIdSet[newChildIds[i]] = true;
    }

    // Delete the old children.
    var nodeImpl = privates(node).impl;
    var oldChildIds = nodeImpl.childIds;
    for (var i = 0; i < oldChildIds.length;) {
      var oldId = oldChildIds[i];
      if (!newChildIdSet[oldId]) {
        this.invalidate_(this.axNodeDataCache_[oldId]);
        oldChildIds.splice(i, 1);
      } else {
        i++;
      }
    }
    nodeImpl.childIds = oldChildIds;

    return true;
  },

  createNewChildren_: function(node, newChildIds, updateState) {
    logging.CHECK(node);
    var success = true;

    for (var i = 0; i < newChildIds.length; i++) {
      var childId = newChildIds[i];
      var childNode = this.axNodeDataCache_[childId];
      if (childNode) {
        if (childNode.parent != node) {
          var parentId = -1;
          if (childNode.parent) {
            var parentImpl = privates(childNode.parent).impl;
            parentId = parentImpl.id;
          }
          // This is a serious error - nodes should never be reparented.
          // If this case occurs, continue so this node isn't left in an
          // inconsistent state, but return failure at the end.
          logging.WARNING('Node ' + childId + ' reparented from ' +
                          parentId + ' to ' + privates(node).impl.id);
          lastError.set('automation',
                        'Bad update received on automation tree',
                        null,
                        chrome);
          success = false;
          continue;
        }
      } else {
        childNode = new AutomationNode(this);
        this.axNodeDataCache_[childId] = childNode;
        privates(childNode).impl.id = childId;
        updateState.pendingNodes[childId] = childNode;
        updateState.newNodes[childId] = childNode;
      }
      privates(childNode).impl.indexInParent = i;
      privates(childNode).impl.parentID = privates(node).impl.id;
    }

    return success;
  },

  setData_: function(node, nodeData) {
    var nodeImpl = privates(node).impl;

    // TODO(dtseng): Make into set listing all hosting node roles.
    if (nodeData.role == schema.RoleType.webView) {
      if (nodeImpl.childTreeID !== nodeData.intAttributes.childTreeId)
        nodeImpl.pendingChildFrame = true;

      if (nodeImpl.pendingChildFrame) {
        nodeImpl.childTreeID = nodeData.intAttributes.childTreeId;
        automationUtil.storeTreeCallback(nodeImpl.childTreeID, function(root) {
          nodeImpl.pendingChildFrame = false;
          nodeImpl.childTree = root;
          privates(root).impl.hostTree = node;
          if (root.attributes.docLoadingProgress == 1)
            privates(root).impl.dispatchEvent(schema.EventType.loadComplete);
          nodeImpl.dispatchEvent(schema.EventType.childrenChanged);
        });
        automationInternal.enableFrame(nodeImpl.childTreeID);
      }
    }
    for (var key in AutomationAttributeDefaults) {
      if (key in nodeData)
        nodeImpl[key] = nodeData[key];
      else
        nodeImpl[key] = AutomationAttributeDefaults[key];
    }

    // Set all basic attributes.
    this.mixinAttributes_(nodeImpl, DefaultMixinAttributes, nodeData);

    // If this is a rootWebArea or webArea, set document attributes.
    if (nodeData.role == schema.RoleType.rootWebArea ||
        nodeData.role == schema.RoleType.webArea) {
      this.mixinAttributes_(nodeImpl, DocumentMixinAttributes, nodeData);
    }

    // If this is a scrollable area, set scrollable attributes.
    for (var scrollAttr in ScrollableMixinAttributes) {
      var spec = ScrollableMixinAttributes[scrollAttr];
      if (this.findAttribute_(scrollAttr, spec, nodeData) !== undefined) {
        this.mixinAttributes_(nodeImpl, ScrollableMixinAttributes, nodeData);
        break;
      }
    }

    // If this is a link, set link attributes
    if (nodeData.role == 'link') {
      this.mixinAttributes_(nodeImpl, LinkMixinAttributes, nodeData);
    }

    // If this is an editable text area, set editable text attributes.
    if (nodeData.role == schema.RoleType.textField ||
        nodeData.role == schema.RoleType.textArea) {
      this.mixinAttributes_(nodeImpl, EditableTextMixinAttributes, nodeData);
    }

    // If this is a range type, set range attributes.
    if (nodeData.role == schema.RoleType.progressIndicator ||
        nodeData.role == schema.RoleType.scrollBar ||
        nodeData.role == schema.RoleType.slider ||
        nodeData.role == schema.RoleType.spinButton) {
      this.mixinAttributes_(nodeImpl, RangeMixinAttributes, nodeData);
    }

    // If this is a table, set table attributes.
    if (nodeData.role == schema.RoleType.table) {
      this.mixinAttributes_(nodeImpl, TableMixinAttributes, nodeData);
    }

    // If this is a table cell, set table cell attributes.
    if (nodeData.role == schema.RoleType.cell) {
      this.mixinAttributes_(nodeImpl, TableCellMixinAttributes, nodeData);
    }

    // If this has an active descendant, expose it.
    if ('intAttributes' in nodeData &&
        'activedescendantId' in nodeData.intAttributes) {
      this.mixinAttributes_(nodeImpl, ActiveDescendantMixinAttribute, nodeData);
    }

    for (var i = 0; i < AutomationAttributeTypes.length; i++) {
      var attributeType = AutomationAttributeTypes[i];
      for (var attributeName in nodeData[attributeType]) {
        nodeImpl.attributesInternal[attributeName] =
            nodeData[attributeType][attributeName];
        if (ATTRIBUTE_BLACKLIST.hasOwnProperty(attributeName) ||
            nodeImpl.attributes.hasOwnProperty(attributeName)) {
          continue;
        } else if (
          ATTRIBUTE_NAME_TO_ID_ATTRIBUTE.hasOwnProperty(attributeName)) {
          this.defineReadonlyAttribute_(nodeImpl,
                                        nodeImpl.attributes,
                                        attributeName,
                                        true);
        } else {
          this.defineReadonlyAttribute_(nodeImpl,
                                        nodeImpl.attributes,
                                        attributeName);
        }
      }
    }
  },

  mixinAttributes_: function(nodeImpl, attributes, nodeData) {
    for (var attribute in attributes) {
      var spec = attributes[attribute];
      if (spec.isRef)
        this.mixinRelationshipAttribute_(nodeImpl, attribute, spec, nodeData);
      else
        this.mixinAttribute_(nodeImpl, attribute, spec, nodeData);
    }
  },

  mixinAttribute_: function(nodeImpl, attribute, spec, nodeData) {
    var value = this.findAttribute_(attribute, spec, nodeData);
    if (value === undefined)
      value = spec.default;
    nodeImpl.attributesInternal[attribute] = value;
    this.defineReadonlyAttribute_(nodeImpl, nodeImpl, attribute);
  },

  mixinRelationshipAttribute_: function(nodeImpl, attribute, spec, nodeData) {
    var idAttribute = spec.idAttribute;
    var idValue = spec.default;
    if (spec.idFrom in nodeData) {
      idValue = idAttribute in nodeData[spec.idFrom]
          ? nodeData[spec.idFrom][idAttribute] : idValue;
    }

    // Ok to define a list attribute with an empty list, but not a
    // single ref with a null ID.
    if (idValue === null)
      return;

    nodeImpl.attributesInternal[idAttribute] = idValue;
    this.defineReadonlyAttribute_(
      nodeImpl, nodeImpl, attribute, true, idAttribute);
  },

  findAttribute_: function(attribute, spec, nodeData) {
    if (!('reflectFrom' in spec))
      return;
    var attributeGroup = spec.reflectFrom;
    if (!(attributeGroup in nodeData))
      return;

    return nodeData[attributeGroup][attribute];
  },

  defineReadonlyAttribute_: function(
      node, object, attributeName, opt_isIDRef, opt_idAttribute) {
    if (attributeName in object)
      return;

    if (opt_isIDRef) {
      $Object.defineProperty(object, attributeName, {
        enumerable: true,
        get: function() {
          var idAttribute = opt_idAttribute ||
                            ATTRIBUTE_NAME_TO_ID_ATTRIBUTE[attributeName];
          var idValue = node.attributesInternal[idAttribute];
          if (Array.isArray(idValue)) {
            return idValue.map(function(current) {
              return node.rootImpl.get(current);
            }, this);
          }
          return node.rootImpl.get(idValue);
        }.bind(this),
      });
    } else {
      $Object.defineProperty(object, attributeName, {
        enumerable: true,
        get: function() {
          return node.attributesInternal[attributeName];
        }.bind(this),
      });
    }

    if (object instanceof AutomationNodeImpl) {
      // Also expose attribute publicly on the wrapper.
      $Object.defineProperty(object.wrapper, attributeName, {
        enumerable: true,
        get: function() {
          return object[attributeName];
        },
      });

    }
  },

  updateNode_: function(nodeData, updateState) {
    var node = this.axNodeDataCache_[nodeData.id];
    var didUpdateRoot = false;
    if (node) {
      delete updateState.pendingNodes[privates(node).impl.id];
    } else {
      if (nodeData.role != schema.RoleType.rootWebArea &&
          nodeData.role != schema.RoleType.desktop) {
        logging.WARNING(String(nodeData.id) +
                     ' is not in the cache and not the new root.');
        lastError.set('automation',
                      'Bad update received on automation tree',
                      null,
                      chrome);
        return false;
      }
      // |this| is an AutomationRootNodeImpl; retrieve the
      // AutomationRootNode instance instead.
      node = this.wrapper;
      didUpdateRoot = true;
      updateState.newNodes[this.id] = this.wrapper;
    }
    this.setData_(node, nodeData);

    // TODO(aboxhall): send onChanged event?
    logging.CHECK(node);
    if (!this.deleteOldChildren_(node, nodeData.childIds)) {
      if (didUpdateRoot) {
        this.invalidate_(this.wrapper);
      }
      return false;
    }
    var nodeImpl = privates(node).impl;

    var success = this.createNewChildren_(node,
                                          nodeData.childIds,
                                          updateState);
    nodeImpl.childIds = nodeData.childIds;
    this.axNodeDataCache_[nodeImpl.id] = node;

    return success;
  }
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
                                                'addEventListener',
                                                'removeEventListener',
                                                'domQuerySelector',
                                                'toJSON' ],
                                    readonly: ['parent',
                                               'firstChild',
                                               'lastChild',
                                               'children',
                                               'previousSibling',
                                               'nextSibling',
                                               'isRootNode',
                                               'role',
                                               'state',
                                               'location',
                                               'attributes',
                                               'indexInParent',
                                               'root'] });

var AutomationRootNode = utils.expose('AutomationRootNode',
                                      AutomationRootNodeImpl,
                                      { superclass: AutomationNode });

exports.AutomationNode = AutomationNode;
exports.AutomationRootNode = AutomationRootNode;
