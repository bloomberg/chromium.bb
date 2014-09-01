// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runWithModuleSystem(function(moduleSystem) {
  window.AutomationRootNode =
      moduleSystem.require('automationNode').AutomationRootNode;
  window.privates = moduleSystem.privates;
});

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;
var assertIsDef = function(obj) {
  assertTrue(obj !== null && obj !== undefined);
}
var assertIsNotDef = function(obj) {
  assertTrue(obj === null || obj === undefined);
}
var succeed = chrome.test.succeed;

var tests = [
  function testAutomationRootNode() {
    var root = new AutomationRootNode();
    assertTrue(root.isRootNode);

    succeed();
  },

  function testAriaRelationshipAttributes() {
    var data = {
      'eventType': 'loadComplete',
          'processID': 17, 'routingID': 2, 'targetID': 1,
      'update': { 'nodeIdToClear': 0, 'nodes': [
        {
          'id': 1, 'role': 'rootWebArea',
          'boolAttributes': {'docLoaded': true},
          'childIds': [5, 6, 7, 8, 9, 10, 11]
        },
        {
          'id': 11,        'role': 'div',
          'childIds': [],
          'htmlAttributes': {'id': 'target' }
        },
        {
          'id': 5, 'role': 'div',
          'childIds': [],
          'htmlAttributes': {'aria-activedescendant': 'target',
                             'id': 'activedescendant'},
          'intAttributes': {'activedescendantId': 11},
        },
        {
          'id': 6, 'role': 'div',
          'childIds': [],
          'htmlAttributes': {'aria-controls': 'target', 'id': 'controlledBy'},
          'intlistAttributes': {'controlsIds': [11]},
        },
        {
          'id': 7, 'role': 'div',
          'childIds': [],
          'htmlAttributes': {'aria-describedby': 'target', 'id': 'describedBy'},
          'intlistAttributes': {'describedbyIds': [11, 6]},
        },
        {
          'id': 8, 'role': 'div',
          'childIds': [],
          'htmlAttributes': {'aria-flowto': 'target', 'id': 'flowTo'},
          'intlistAttributes': {'flowtoIds': [11]},
        },
        {
          'id': 9, 'role': 'div',
          'childIds': [],
          'htmlAttributes': {'aria-labelledby': 'target', 'id': 'labelledBy'},
          'intlistAttributes': {'labelledbyIds': [11]}
        },
        {
          'id': 10, 'role': 'div',
          'childIds': [],
          'htmlAttributes': {'aria-owns': 'target', 'id': 'owns'},
          'intlistAttributes': {'ownsIds': [11]},
        }
      ]}
    }

    var tree = new AutomationRootNode();
    privates(tree).impl.onAccessibilityEvent(data);

    var activedescendant = tree.firstChild();
    assertIsDef(activedescendant);
    assertEq('activedescendant', activedescendant.attributes.id);
    assertEq(
        'target',
        activedescendant.attributes['aria-activedescendant'].attributes.id);
    assertIsNotDef(activedescendant.attributes.activedescendantId);

    var controlledBy = activedescendant.nextSibling();
    assertIsDef(controlledBy);
    assertEq('controlledBy', controlledBy.attributes.id);
    assertEq('target',
             controlledBy.attributes['aria-controls'][0].attributes.id);
    assertEq(1, controlledBy.attributes['aria-controls'].length);
    assertIsNotDef(controlledBy.attributes.controlledbyIds);

    var describedBy = controlledBy.nextSibling();
    assertIsDef(describedBy);
    assertEq('describedBy', describedBy.attributes.id);
    assertEq('target',
             describedBy.attributes['aria-describedby'][0].attributes.id);
    assertEq('controlledBy',
             describedBy.attributes['aria-describedby'][1].attributes.id);
    assertEq(2, describedBy.attributes['aria-describedby'].length);
    assertIsNotDef(describedBy.attributes.describedbyIds);

    var flowTo = describedBy.nextSibling();
    assertIsDef(flowTo);
    assertEq('flowTo', flowTo.attributes.id);
    assertEq('target',
             flowTo.attributes['aria-flowto'][0].attributes.id);
    assertEq(1, flowTo.attributes['aria-flowto'].length);
    assertIsNotDef(flowTo.attributes.flowtoIds);

    var labelledBy = flowTo.nextSibling();
    assertIsDef(labelledBy);
    assertEq('labelledBy', labelledBy.attributes.id);
    assertEq('target',
             labelledBy.attributes['aria-labelledby'][0].attributes.id);
    assertEq(1, labelledBy.attributes['aria-labelledby'].length);
    assertIsNotDef(labelledBy.attributes.labelledbyIds);

    var owns = labelledBy.nextSibling();
    assertIsDef(owns);
    assertEq('owns', owns.attributes.id);
    assertEq('target', owns.attributes['aria-owns'][0].attributes.id);
    assertEq(1, owns.attributes['aria-owns'].length);
    assertIsNotDef(owns.attributes.ownsIds);

    succeed();
  },

  function testCannotSetAttribute() {
    var update =
        {
          'nodeIdToClear': 0, 'nodes': [
            {
              'id': 1, 'role': 'rootWebArea',
              'boolAttributes': {'docLoaded': true},
              'childIds': [11]
            },
            {
              'id': 11, 'role': 'button',
              'stringAttributes': {'name': 'foo'},
              'childIds': []
            }]
        }

    var tree = new AutomationRootNode();
    assertTrue(privates(tree).impl.unserialize(update));
    var button = tree.firstChild();
    assertEq('button', button.role);
    assertEq('foo', button.attributes.name);
    button.attributes.name = 'bar';
    assertEq('foo', button.attributes.name);

    succeed();
  },

  function testBadUpdateInvalidChildIds() {
    var update =
        {
          'nodeIdToClear': 0, 'nodes': [
            {
              'id': 1, 'role': 'rootWebArea',
              'boolAttributes': {'docLoaded': true},
              'childIds': [5, 6, 7, 8, 9, 10, 11]
            }]
        }

    var tree = new AutomationRootNode();

    // This is a bad update because the root references non existent child ids.
    assertFalse(privates(tree).impl.unserialize(update));

    succeed();
  },

  function testMultipleUpdateNameChanged() {
    var update =
        {
          'nodeIdToClear': 0, 'nodes': [
            {
              'id': 1, 'role': 'rootWebArea',
              'boolAttributes': {'docLoaded': true},
              'childIds': [11]
            },
            {
              'id': 11, 'role': 'button',
              'stringAttributes': {'name': 'foo'},
              'childIds': []
            }]
        }

    // First, setup the initial tree.
    var tree = new AutomationRootNode();
    assertTrue(privates(tree).impl.unserialize(update));
    var button = tree.firstChild();
    assertEq('button', button.role);
    assertEq('foo', button.attributes.name);

    // Now, apply an update that changes the button's name.
    // Remove the root since the native serializer stops at the LCA.
    update.nodes.splice(0, 1);
    update.nodes[0].stringAttributes.name = 'bar';

    // Make sure the name changes.
    assertTrue(privates(tree).impl.unserialize(update));
    assertEq('bar', button.attributes.name);

    succeed();
  }
];

chrome.test.runTests(tests);
