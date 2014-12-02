// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runWithModuleSystem(function(moduleSystem) {
  window.AutomationRootNode =
      moduleSystem.require('automationNode').AutomationRootNode;
  window.privates = moduleSystem.privates;
  // Unused.
  window.automationUtil = function() {};
  window.automationUtil.storeTreeCallback = function() {};
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
          'boolAttributes': {'docLoaded': true },
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
    try {
    privates(tree).impl.onAccessibilityEvent(data);
    } catch (e) {
      console.log(e.stack);
    }

    var activedescendant = tree.firstChild;
    assertIsDef(activedescendant);
    assertEq('activedescendant', activedescendant.attributes.id);
    assertEq(
        'target',
        activedescendant.attributes['aria-activedescendant'].attributes.id);

    assertFalse(activedescendant.activedescendant == null,
                'activedescendant should not be null');
    assertEq(
        'target',
        activedescendant.activedescendant.attributes.id);
    assertIsNotDef(activedescendant.attributes.activedescendantId);

    var controlledBy = activedescendant.nextSibling;
    assertIsDef(controlledBy);
    assertEq('controlledBy', controlledBy.attributes.id);
    assertEq(1, controlledBy.attributes['aria-controls'].length);
    assertEq('target',
             controlledBy.attributes['aria-controls'][0].attributes.id);
    assertEq(1, controlledBy.controls.length);
    assertEq('target', controlledBy.controls[0].attributes.id);
    assertIsNotDef(controlledBy.attributes.controlledbyIds);

    var describedBy = controlledBy.nextSibling;
    assertIsDef(describedBy);
    assertEq('describedBy', describedBy.attributes.id);
    assertEq(2, describedBy.attributes['aria-describedby'].length);
    assertEq('target',
             describedBy.attributes['aria-describedby'][0].attributes.id);
    assertEq('controlledBy',
             describedBy.attributes['aria-describedby'][1].attributes.id);
    assertEq(2, describedBy.describedby.length);
    assertEq('target', describedBy.describedby[0].attributes.id);
    assertEq('controlledBy',
             describedBy.describedby[1].attributes.id);
    assertIsNotDef(describedBy.attributes.describedbyIds);

    var flowTo = describedBy.nextSibling;
    assertIsDef(flowTo);
    assertEq('flowTo', flowTo.attributes.id);
    assertEq(1, flowTo.attributes['aria-flowto'].length);
    assertEq('target',
             flowTo.attributes['aria-flowto'][0].attributes.id);
    assertEq(1, flowTo.flowto.length);
    assertEq('target', flowTo.flowto[0].attributes.id);
    assertIsNotDef(flowTo.attributes.flowtoIds);

    var labelledBy = flowTo.nextSibling;
    assertIsDef(labelledBy);
    assertEq('labelledBy', labelledBy.attributes.id);
    assertEq(1, labelledBy.attributes['aria-labelledby'].length);
    assertEq('target',
             labelledBy.attributes['aria-labelledby'][0].attributes.id);
    assertEq(1, labelledBy.labelledby.length);
    assertEq('target',
             labelledBy.labelledby[0].attributes.id);
    assertIsNotDef(labelledBy.attributes.labelledbyIds);

    var owns = labelledBy.nextSibling;
    assertIsDef(owns);
    assertEq('owns', owns.attributes.id);
    assertEq(1, owns.attributes['aria-owns'].length);
    assertEq('target', owns.attributes['aria-owns'][0].attributes.id);
    assertEq(1, owns.owns.length);
    assertEq('target', owns.owns[0].attributes.id);
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
    var button = tree.firstChild;
    assertEq('button', button.role);
    assertEq('foo', button.name);
    button.name = 'bar';
    assertEq('foo', button.name);

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
    var button = tree.firstChild;
    assertEq('button', button.role);
    assertEq('foo', button.name);

    // Now, apply an update that changes the button's name.
    // Remove the root since the native serializer stops at the LCA.
    update.nodes.splice(0, 1);
    update.nodes[0].stringAttributes.name = 'bar';

    // Make sure the name changes.
    assertTrue(privates(tree).impl.unserialize(update));
    assertEq('bar', button.name);

    succeed();
  },

  function testDocumentAndScrollableMixins() {
    var update = { 'nodeIdToClear': 0, 'nodes': [
        {
          'id': 1, 'role': 'rootWebArea',
          'boolAttributes': { 'docLoaded': false },
          'stringAttributes': { 'docUrl': 'chrome://terms',
                                'docTitle': 'Google Chrome Terms of Service' },
          'intAttributes': { 'scrollY': 583,
                             'scrollYMax': 9336 },
          'floatAttributes': { 'docLoadingProgress': 0.9 },
          'childIds': [2]
        },
        {
          'id': 2, 'role': 'div',
          'childIds': [],
          'htmlAttributes': { 'id': 'child' },
        },
    ] };

    var tree = new AutomationRootNode();
    assertTrue(privates(tree).impl.unserialize(update));
    assertEq(false, tree.docLoaded);
    assertEq('chrome://terms', tree.docUrl);
    assertEq('Google Chrome Terms of Service', tree.docTitle);
    assertEq('0.9', tree.docLoadingProgress.toPrecision(1));
    assertEq(583, tree.scrollY);
    assertEq(9336, tree.scrollYMax);
    // Default values will be set for mixin attributes even if not in data.
    assertEq(0, tree.scrollYMin);
    assertEq(0, tree.scrollX);
    assertEq(0, tree.scrollXMin);
    assertEq(0, tree.scrollXMax);

    succeed();
  },

  function testEditableTextMixins() {
    var update = { 'nodeIdToClear': 0, 'nodes': [
        {
          'id': 1, 'role': 'rootWebArea',
          'boolAttributes': { 'docLoaded': true },
          'stringAttributes': { 'docUrl': 'chrome://terms',
                                'docTitle': 'Google Chrome Terms of Service' },
          'intAttributes': { 'scrollY': 583,
                             'scrollYMax': 9336 },
          'childIds': [2, 3]
        },
        {
          'id': 2, 'role': 'textField',
          'intAttributes': { 'textSelStart': 10, 'textSelEnd': 20 },
          'childIds': []
        },
        {
          'id': 3, 'role': 'textArea',
          'childIds': []
        },

    ] };

    var tree = new AutomationRootNode();
    assertTrue(privates(tree).impl.unserialize(update));
    assertEq(true, tree.docLoaded);
    assertFalse('textSelStart' in tree);
    assertFalse('textSelEnd' in tree);
    var textField = tree.firstChild;
    assertEq(10, textField.textSelStart);
    assertEq(20, textField.textSelEnd);
    var textArea = textField.nextSibling;
    assertEq(-1, textArea.textSelStart);
    assertEq(-1, textArea.textSelEnd);

    succeed();
  },

  function testRangeMixins() {
    var update = { 'nodeIdToClear': 0, 'nodes': [
        {
          'id': 1, 'role': 'rootWebArea',
          'boolAttributes': { 'docLoaded': true },
          'stringAttributes': { 'docUrl': 'chrome://terms',
                                'docTitle': 'Google Chrome Terms of Service' },
          'intAttributes': { 'scrollY': 583,
                             'scrollYMax': 9336 },
          'childIds': [2, 3, 4, 5]
        },
        {
          'id': 2, 'role': 'progressIndicator',
          'floatAttributes': { 'valueForRange': 1.0,
                               'minValueForRange': 0.0,
                               'maxValueForRange': 1.0 },
          'childIds': []
        },
        {
          'id': 3, 'role': 'scrollBar',
          'floatAttributes': { 'valueForRange': 0.3,
                               'minValueForRange': 0.0,
                               'maxValueForRange': 1.0 },
          'childIds': []
        },
        {
          'id': 4, 'role': 'slider',
          'floatAttributes': { 'valueForRange': 3.0,
                               'minValueForRange': 1.0,
                               'maxValueForRange': 5.0 },
          'childIds': []
        },
        {
          'id': 5, 'role': 'spinButton',
          'floatAttributes': { 'valueForRange': 14.0,
                               'minValueForRange': 1.0,
                               'maxValueForRange': 31.0 },
          'childIds': []
        }
    ] };

    var tree = new AutomationRootNode();
    assertTrue(privates(tree).impl.unserialize(update));
    assertEq(true, tree.docLoaded);
    assertFalse('valueForRange' in tree);
    assertFalse('minValueForRange' in tree);
    assertFalse('maxValueForRange' in tree);

    var progressIndicator = tree.firstChild;
    assertEq(1.0, progressIndicator.valueForRange);
    assertEq(0.0, progressIndicator.minValueForRange);
    assertEq(1.0, progressIndicator.maxValueForRange);

    var scrollBar = progressIndicator.nextSibling;
    assertEq(0.3, scrollBar.valueForRange);
    assertEq(0.0, scrollBar.minValueForRange);
    assertEq(1.0, scrollBar.maxValueForRange);

    var slider = scrollBar.nextSibling;
    assertEq(3.0, slider.valueForRange);
    assertEq(1.0, slider.minValueForRange);
    assertEq(5.0, slider.maxValueForRange);

    var spinButton = slider.nextSibling;
    assertEq(14.0, spinButton.valueForRange);
    assertEq(1.0, spinButton.minValueForRange);
    assertEq(31.0, spinButton.maxValueForRange);

    succeed();
  },

  function testTableMixins() {
        var update = { 'nodeIdToClear': 0, 'nodes': [
        {
          'id': 1, 'role': 'rootWebArea',
          'boolAttributes': { 'docLoaded': true },
          'stringAttributes': { 'docUrl': 'chrome://terms',
                                'docTitle': 'Google Chrome Terms of Service' },
          'intAttributes': { 'scrollY': 583,
                             'scrollYMax': 9336 },
          'childIds': [2]
        },
        {
          'id': 2, 'role': 'table',
          'childIds': [3, 6],
          'intAttributes': { tableRowCount: 2, tableColumnCount: 3 }
        },
        {
          'id': 3, 'role': 'row',
          'childIds': [4, 5]
        },
        {
          'id': 4, 'role': 'cell',
          'intAttributes': { 'tableCellColumnIndex': 0,
                             'tableCellColumnSpan': 2,
                             'tableCellRowIndex': 0,
                             'tableCellRowSpan': 1 },
          'childIds': []
        },
        {
          'id': 5, 'role': 'cell',
          'intAttributes': { 'tableCellColumnIndex': 2,
                             'tableCellColumnSpan': 1,
                             'tableCellRowIndex': 0,
                             'tableCellRowSpan': 2 },
          'childIds': []
        },
        {
          'id': 6, 'role': 'row',
          'childIds': [7, 8]
        },
        {
          'id': 7, 'role': 'cell',
          'intAttributes': { 'tableCellColumnIndex': 0,
                             'tableCellColumnSpan': 1,
                             'tableCellRowIndex': 1,
                             'tableCellRowSpan': 1 },
          'childIds': []
        },
        {
          'id': 8, 'role': 'cell',
          'intAttributes': { 'tableCellColumnIndex': 1,
                             'tableCellColumnSpan': 1,
                             'tableCellRowIndex': 1,
                             'tableCellRowSpan': 1 },
          'childIds': []
        }
    ] };

    var tree = new AutomationRootNode();
    try {
    assertTrue(privates(tree).impl.unserialize(update));
    } catch (e) {
      console.log(e.stack);
    }
    var TableMixinAttributes = {
      tableRowCount: 0,
      tableColumnCount: 0
    };
    for (var attribute in TableMixinAttributes)
      assertFalse(attribute in tree);

    var TableCellMixinAttributes = {
      tableCellColumnIndex: 0,
      tableCellColumnSpan: 1,
      tableCellRowIndex: 0,
      tableCellRowSpan: 1
    };
    for (var attribute in TableCellMixinAttributes)
      assertFalse(attribute in tree);

    var table = tree.firstChild;
    assertEq(2, table.tableRowCount);
    assertEq(3, table.tableColumnCount);

    var row1 = table.firstChild;
    var cell1 = row1.firstChild;
    assertEq(0, cell1.tableCellColumnIndex);
    assertEq(2, cell1.tableCellColumnSpan);
    assertEq(0, cell1.tableCellRowIndex);
    assertEq(1, cell1.tableCellRowSpan);

    var cell2 = cell1.nextSibling;
    assertEq(2, cell2.tableCellColumnIndex);
    assertEq(1, cell2.tableCellColumnSpan);
    assertEq(0, cell2.tableCellRowIndex);
    assertEq(2, cell2.tableCellRowSpan);

    var row2 = row1.nextSibling;
    var cell3 = row2.firstChild;
    assertEq(0, cell3.tableCellColumnIndex);
    assertEq(1, cell3.tableCellColumnSpan);
    assertEq(1, cell3.tableCellRowIndex);
    assertEq(1, cell3.tableCellRowSpan);

    var cell4 = cell3.nextSibling;
    assertEq(1, cell4.tableCellColumnIndex);
    assertEq(1, cell4.tableCellColumnSpan);
    assertEq(1, cell4.tableCellRowIndex);
    assertEq(1, cell4.tableCellRowSpan);

    succeed();
  }
];

chrome.test.runTests(tests);
