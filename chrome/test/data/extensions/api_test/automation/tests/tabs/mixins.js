// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ActiveDescendantMixin = [ 'activedescendant' ];
var LinkMixins = [ 'url' ];
var DocumentMixins = [ 'docUrl',
                       'docTitle',
                       'docLoaded',
                       'docLoadingProgress' ];
var ScrollableMixins = [ 'scrollX',
                         'scrollXMin',
                         'scrollXMax',
                         'scrollY',
                         'scrollYMin',
                         'scrollYMax' ];
var EditableTextMixins = [ 'textSelStart',
                           'textSelEnd' ];
var RangeMixins = [ 'valueForRange',
                    'minValueForRange',
                    'maxValueForRange' ];
var TableMixins = [ 'tableRowCount',
                    'tableColumnCount' ];
var TableCellMixins = [ 'tableCellColumnIndex',
                        'tableCellColumnSpan',
                        'tableCellRowIndex',
                        'tableCellRowSpan' ];

var allTests = [
  function testDocumentAndScrollMixins() {
    for (var i = 0; i < DocumentMixins.length; i++) {
      var mixinAttribute = DocumentMixins[i];
      assertTrue(mixinAttribute in rootNode,
                 'rootNode should have a ' + mixinAttribute + ' attribute');
    }
    for (var i = 0; i < ScrollableMixins.length; i++) {
      var mixinAttribute = ScrollableMixins[i];
      assertTrue(mixinAttribute in rootNode,
                 'rootNode should have a ' + mixinAttribute + ' attribute');
    }

    assertEq(url, rootNode.docUrl);
    assertEq('Automation Tests - Mixin attributes', rootNode.docTitle);
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

  function testActiveDescendantAndOwns() {
    var combobox = rootNode.find({ role: 'comboBox' });

    assertTrue('owns' in combobox, 'combobox should have an owns attribute');
    assertEq(1, combobox.owns.length);
    var listbox = rootNode.find({ role: 'listBox' });
    assertEq(listbox, combobox.owns[0]);

    assertTrue('activedescendant' in combobox,
               'combobox should have an activedescendant attribute');
    var opt6 = listbox.children[5];
    assertEq(opt6, combobox.activedescendant);
    chrome.test.succeed();
  },

  function testLinkMixins() {
    var links = rootNode.findAll({ role: 'link' });
    assertEq(2, links.length);

    var realLink = links[0];
    assertEq('Real link', realLink.name);
    assertTrue('url' in realLink, 'realLink should have a url attribute');
    assertEq('about://blank', realLink.url);

    var ariaLink = links[1];
    assertEq('ARIA link', ariaLink.name);
    assertTrue('url' in ariaLink, 'ariaLink should have a url attribute');
    assertEq('', ariaLink.url);
    chrome.test.succeed();
  },

  function testEditableTextMixins() {
    var textFields = rootNode.findAll({ role: 'textField' });
    assertEq(2, textFields.length);
    var EditableTextMixins = [ 'textSelStart', 'textSelEnd' ];
    for (var i = 0; i < textFields.length; i++) {
      var textField = textFields[i];
      var id = textField.attributes.id;
      for (var j = 0; j < EditableTextMixins.length; j++) {
        var mixinAttribute = EditableTextMixins[j];
        assertTrue(mixinAttribute in textField,
                   'textField (' + id + ') should have a ' + mixinAttribute +
                   ' attribute');
      }
    }
    var input = textFields[0];
    assertEq('text-input', input.attributes.id);
    assertEq(2, input.textSelStart);
    assertEq(8, input.textSelEnd);

    var ariaTextbox = textFields[1];
    assertEq('textbox-role', ariaTextbox.attributes.id);
    assertEq(0, ariaTextbox.textSelStart);
    assertEq(0, ariaTextbox.textSelEnd);

    var textArea = rootNode.find({ role: 'textArea' });
    assertFalse(textArea == null, 'Should find a textArea in the page');
    assertEq('textarea', textArea.attributes.id);
    for (var i = 0; i < EditableTextMixins.length; i++) {
      var mixinAttribute = EditableTextMixins[i];
      assertTrue(mixinAttribute in textArea,
                 'textArea should have a ' + mixinAttribute + ' attribute');
    }
    assertEq(0, textArea.textSelStart);
    assertEq(0, textArea.textSelEnd);

    chrome.test.succeed();
  },

  function testRangeMixins() {
    var sliders = rootNode.findAll({ role: 'slider' });
    assertEq(2, sliders.length);
    var spinButtons = rootNode.findAll({ role: 'spinButton' });
    assertEq(1, spinButtons.length);
    var progressIndicators = rootNode.findAll({ role: 'progressIndicator' });
    assertEq(1, progressIndicators.length);
    assertEq('progressbar-role', progressIndicators[0].attributes.id);
    var scrollBars = rootNode.findAll({ role: 'scrollBar' });
    assertEq(1, scrollBars.length);

    var ranges = sliders.concat(spinButtons, progressIndicators, scrollBars);
    assertEq(5, ranges.length);

    for (var i = 0; i < ranges.length; i++) {
      var range = ranges[i];
      for (var j = 0; j < RangeMixins.length; j++) {
        var mixinAttribute = RangeMixins[j];
        assertTrue(mixinAttribute in range,
                   range.role + ' (' + range.attributes.id + ') should have a '
                   + mixinAttribute + ' attribute');
      }
    }

    var inputRange = sliders[0];
    assertEq('range-input', inputRange.attributes.id);
    assertEq(4, inputRange.valueForRange);
    assertEq(0, inputRange.minValueForRange);
    assertEq(5, inputRange.maxValueForRange);

    var ariaSlider = sliders[1];
    assertEq('slider-role', ariaSlider.attributes.id);
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

  function testTableMixins() {
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

  function testNoMixins() {
    var div = rootNode.find({ attributes: { id: 'main' } });
    assertTrue(div !== undefined);
    var allMixins = [].concat(ActiveDescendantMixin,
                              LinkMixins,
                              DocumentMixins,
                              ScrollableMixins,
                              EditableTextMixins,
                              RangeMixins,
                              TableMixins,
                              TableCellMixins);
    for (var mixinAttr in allMixins) {
      assertFalse(mixinAttr in div);
    }
    chrome.test.succeed();
  }
];

setUpAndRunTests(allTests, 'mixins.html');
