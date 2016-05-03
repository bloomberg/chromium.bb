// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ActiveDescendantAttribute = [ 'activeDescendant' ];
var LinkAttributes = [ 'url' ];
var DocumentAttributes = [ 'docUrl',
                           'docTitle',
                           'docLoaded',
                           'docLoadingProgress' ];
var ScrollableAttributes = [ 'scrollX',
                             'scrollXMin',
                             'scrollXMax',
                             'scrollY',
                             'scrollYMin',
                             'scrollYMax' ];
var EditableTextAttributes = [ 'textSelStart',
                               'textSelEnd' ];
var RangeAttributes = [ 'valueForRange',
                        'minValueForRange',
                        'maxValueForRange' ];
var TableAttributes = [ 'tableRowCount',
                        'tableColumnCount' ];
var TableCellAttributes = [ 'tableCellColumnIndex',
                            'tableCellColumnSpan',
                            'tableCellRowIndex',
                            'tableCellRowSpan' ];

var allTests = [
  function testDocumentAndScrollAttributes() {
    for (var i = 0; i < DocumentAttributes.length; i++) {
      var attribute = DocumentAttributes[i];
      assertTrue(attribute in rootNode,
                 'rootNode should have a ' + attribute + ' attribute');
    }
    for (var i = 0; i < ScrollableAttributes.length; i++) {
      var attribute = ScrollableAttributes[i];
      assertTrue(attribute in rootNode,
                 'rootNode should have a ' + attribute + ' attribute');
    }

    assertEq(url, rootNode.docUrl);
    assertEq('Automation Tests - Attributes', rootNode.docTitle);
    assertEq(true, rootNode.docLoaded);
    assertEq(1, rootNode.docLoadingProgress);
    assertEq(0, rootNode.scrollX);
    assertEq(0, rootNode.scrollXMin);
    assertEq(0, rootNode.scrollXMax);
    assertEq(0, rootNode.scrollY);
    assertEq(0, rootNode.scrollYMin);
    assertEq(0, rootNode.scrollYMax);
    chrome.test.succeed();
  },

  function testActiveDescendant() {
    var combobox = rootNode.find({ role: 'comboBox' });
    assertTrue('activeDescendant' in combobox,
               'combobox should have an activedescendant attribute');
    var listbox = rootNode.find({ role: 'listBox' });
    var opt6 = listbox.children[5];
    assertEq(opt6, combobox.activeDescendant);
    chrome.test.succeed();
  },

  function testLinkAttributes() {
    var links = rootNode.findAll({ role: 'link' });
    assertEq(2, links.length);

    var realLink = links[0];
    assertEq('Real link', realLink.name);
    assertTrue('url' in realLink, 'realLink should have a url attribute');
    assertEq('about://blank', realLink.url);

    var ariaLink = links[1];
    assertEq('ARIA link', ariaLink.name);
    assertTrue('url' in ariaLink, 'ariaLink should have an empty url');
    assertEq(undefined, ariaLink.url);
    chrome.test.succeed();
  },

  function testEditableTextAttributes() {
    var textFields = rootNode.findAll({ role: 'textField' });
    assertEq(3, textFields.length);
    var EditableTextAttributes = [ 'textSelStart', 'textSelEnd' ];
    for (var i = 0; i < textFields.length; i++) {
      var textField = textFields[i];
      var description = textField.description;
      for (var j = 0; j < EditableTextAttributes.length; j++) {
        var attribute = EditableTextAttributes[j];
        assertTrue(attribute in textField,
                   'textField (' + description + ') should have a ' +
                   attribute + ' attribute');
      }
    }
    var input = textFields[0];
    assertEq('text-input', input.name);
    assertEq(2, input.textSelStart);
    assertEq(8, input.textSelEnd);

    var textArea = textFields[1];
    assertEq('textarea', textArea.name);
    for (var i = 0; i < EditableTextAttributes.length; i++) {
      var attribute = EditableTextAttributes[i];
      assertTrue(attribute in textArea,
                 'textArea should have a ' + attribute + ' attribute');
    }
    assertEq(0, textArea.textSelStart);
    assertEq(0, textArea.textSelEnd);

    var ariaTextbox = textFields[2];
    assertEq('textbox-role', ariaTextbox.name);
    assertEq(0, ariaTextbox.textSelStart);
    assertEq(0, ariaTextbox.textSelEnd);

    chrome.test.succeed();
  },

  function testRangeAttributes() {
    var sliders = rootNode.findAll({ role: 'slider' });
    assertEq(2, sliders.length);
    var spinButtons = rootNode.findAll({ role: 'spinButton' });
    assertEq(1, spinButtons.length);
    var progressIndicators = rootNode.findAll({ role: 'progressIndicator' });
    assertEq(1, progressIndicators.length);
    assertEq('progressbar-role', progressIndicators[0].name);
    var scrollBars = rootNode.findAll({ role: 'scrollBar' });
    assertEq(1, scrollBars.length);

    var ranges = sliders.concat(spinButtons, progressIndicators, scrollBars);
    assertEq(5, ranges.length);

    for (var i = 0; i < ranges.length; i++) {
      var range = ranges[i];
      for (var j = 0; j < RangeAttributes.length; j++) {
        var attribute = RangeAttributes[j];
        assertTrue(attribute in range,
                   range.role + ' (' + range.description + ') should have a '
                   + attribute + ' attribute');
      }
    }

    var inputRange = sliders[0];
    assertEq('range-input', inputRange.name);
    assertEq(4, inputRange.valueForRange);
    assertEq(0, inputRange.minValueForRange);
    assertEq(5, inputRange.maxValueForRange);

    var ariaSlider = sliders[1];
    assertEq('slider-role', ariaSlider.name);
    assertEq(7, ariaSlider.valueForRange);
    assertEq(1, ariaSlider.minValueForRange);
    assertEq(10, ariaSlider.maxValueForRange);

    var spinButton = spinButtons[0];
    assertEq(14, spinButton.valueForRange);
    assertEq(1, spinButton.minValueForRange);
    assertEq(31, spinButton.maxValueForRange);

    assertEq('0.9', progressIndicators[0].valueForRange.toPrecision(1));
    assertEq(0, progressIndicators[0].minValueForRange);
    assertEq(1, progressIndicators[0].maxValueForRange);

    assertEq(0, scrollBars[0].valueForRange);
    assertEq(0, scrollBars[0].minValueForRange);
    assertEq(1, scrollBars[0].maxValueForRange);

    chrome.test.succeed();
  },

  function testTableAttributes() {
    var table = rootNode.find({ role: 'table' });;
    assertEq(3, table.tableRowCount);
    assertEq(3, table.tableColumnCount);

    var row1 = table.firstChild;
    var cell1 = row1.firstChild;
    assertEq(0, cell1.tableCellColumnIndex);
    assertEq(1, cell1.tableCellColumnSpan);
    assertEq(0, cell1.tableCellRowIndex);
    assertEq(1, cell1.tableCellRowSpan);

    var cell2 = cell1.nextSibling;
    assertEq(1, cell2.tableCellColumnIndex);
    assertEq(1, cell2.tableCellColumnSpan);
    assertEq(0, cell2.tableCellRowIndex);
    assertEq(1, cell2.tableCellRowSpan);

    var cell3 = cell2.nextSibling;
    assertEq(2, cell3.tableCellColumnIndex);
    assertEq(1, cell3.tableCellColumnSpan);
    assertEq(0, cell3.tableCellRowIndex);
    assertEq(1, cell3.tableCellRowSpan);

    var row2 = row1.nextSibling;
    var cell4 = row2.firstChild;
    assertEq(0, cell4.tableCellColumnIndex);
    assertEq(2, cell4.tableCellColumnSpan);
    assertEq(1, cell4.tableCellRowIndex);
    assertEq(1, cell4.tableCellRowSpan);

    var cell5 = cell4.nextSibling;
    assertEq(2, cell5.tableCellColumnIndex);
    assertEq(1, cell5.tableCellColumnSpan);
    assertEq(1, cell5.tableCellRowIndex);
    assertEq(2, cell5.tableCellRowSpan);

    var row3 = row2.nextSibling;
    var cell6 = row3.firstChild;
    assertEq(0, cell6.tableCellColumnIndex);
    assertEq(1, cell6.tableCellColumnSpan);
    assertEq(2, cell6.tableCellRowIndex);
    assertEq(1, cell6.tableCellRowSpan);

    var cell7 = cell6.nextSibling;
    assertEq(1, cell7.tableCellColumnIndex);
    assertEq(1, cell7.tableCellColumnSpan);
    assertEq(2, cell7.tableCellRowIndex);
    assertEq(1, cell7.tableCellRowSpan);

    chrome.test.succeed();
  },

  function testNoAttributes() {
    var div = rootNode.find({ attributes: { name: 'main' } });
    assertTrue(div !== undefined);
    var allAttributes = [].concat(ActiveDescendantAttribute,
                              LinkAttributes,
                              DocumentAttributes,
                              ScrollableAttributes,
                              EditableTextAttributes,
                              RangeAttributes,
                              TableAttributes,
                              TableCellAttributes);
    for (var attributeAttr in allAttributes) {
      assertFalse(attributeAttr in div);
    }
    chrome.test.succeed();
  }
];

setUpAndRunTests(allTests, 'attributes.html');
