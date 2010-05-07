// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TODO(eroman): This needs better presentation, and cleaner code. This
 *               implementation is more of a transitionary step as
 *               the old net-internals is replaced.
 */

var PaintLogView;
var PrintSourceEntriesAsText;

// Start of anonymous namespace.
(function() {

PaintLogView = function(sourceEntries, node) {
  for (var i = 0; i < sourceEntries.length; ++i) {
    if (i != 0)
      addNode(node, 'hr');
    addSourceEntry_(node, sourceEntries[i]);
  }
}

const INDENTATION_PX = 20;

function addSourceEntry_(node, sourceEntry) {
  var div = addNode(node, 'div');
  div.className = 'logSourceEntry';

  var p = addNode(div, 'p');
  var nobr = addNode(p, 'nobr');

  addTextNode(nobr, sourceEntry.getDescription());

  var pre = addNode(div, 'pre');
  addTextNode(pre, PrintSourceEntriesAsText(sourceEntry.getLogEntries()));
}

PrintSourceEntriesAsText = function(sourceEntries) {
  var entries = LogGroupEntry.createArrayFrom(sourceEntries);

  var tablePrinter = new TablePrinter();

  for (var i = 0; i < entries.length; ++i) {
    var entry = entries[i];

    // Avoid printing the END for a BEGIN that was immediately before.
    if (entry.isEnd() && entry.begin && entry.begin.index == i - 1) {
      continue;
    }

    tablePrinter.addRow();

    tablePrinter.addCell('t=');
    var tCell = tablePrinter.addCell(entry.orig.time);
    tCell.alignRight = true;
    tablePrinter.addCell('  ');

    var indentationStr = makeRepeatedString(' ', entry.getDepth() * 3);
    var mainCell =
        tablePrinter.addCell(indentationStr + getTextForEvent(entry));
    tablePrinter.addCell('  ');

    // Get the elapsed time.
    if (entry.isBegin()) {
      tablePrinter.addCell('[dt=');
      var dt = '?';
      // Definite time.
      if (entry.end) {
        dt = entry.end.orig.time - entry.orig.time;
      }
      var dtCell = tablePrinter.addCell(dt);
      dtCell.alignRight = true;

      tablePrinter.addCell(']');
    } else {
      mainCell.allowOverflow = true;
    }

    // Output the extra parameters.
    if (entry.orig.extra_parameters != undefined) {
      // Add a continuation row for each line of text from the extra parameters.
      var extraParamsText = getTextForExtraParams(entry.orig);
      var extraParamsTextLines = extraParamsText.split('\n');

      for (var j = 0; j < extraParamsTextLines.length; ++j) {
        tablePrinter.addRow();
        tablePrinter.addCell('');  // No t=.
        tablePrinter.addCell('');
        tablePrinter.addCell('  ');

        var mainExtraCell =
            tablePrinter.addCell(indentationStr + extraParamsTextLines[j]);
        mainExtraCell.allowOverflow = true;
      }
    }
  }

  // Format the table for fixed-width text.
  return tablePrinter.toText();
}

function getTextForExtraParams(entry) {
  var out = [];
  for (var k in entry.extra_parameters) {
    out.push(' --> ' + k + ' = ' + JSON.stringify(entry.extra_parameters[k]));
  }
  return out.join('\n');
}

function getTextForEvent(entry) {
  var text = '';

  if (entry.isBegin()) {
    text = '+' + text;
  } else if (entry.isEnd()) {
    text = '-' + text;
  } else {
    text = ' ';
  }

  text += getKeyWithValue(LogEventType, entry.orig.type);
  return text;
}

// End of anonymous namespace.
})();

