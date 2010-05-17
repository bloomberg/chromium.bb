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
    // (Except if the END contains any extra parameters).
    if (entry.isEnd() && !entry.orig.params && entry.begin &&
        entry.begin.index == i - 1) {
      continue;
    }

    tablePrinter.addRow();

    tablePrinter.addCell('t=');
    var tCell = tablePrinter.addCell(
        g_browser.convertTimeTicksToDate(entry.orig.time).getTime());
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
    if (entry.orig.params != undefined) {
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
  // Format the extra parameters (use a custom formatter for certain types,
  // but default to displaying as JSON).
  switch (entry.type) {
    case LogEventType.HTTP_TRANSACTION_SEND_REQUEST_HEADERS:
    case LogEventType.HTTP_TRANSACTION_SEND_TUNNEL_HEADERS:
      return getTextForRequestHeadersExtraParam(entry);

    case LogEventType.HTTP_TRANSACTION_READ_RESPONSE_HEADERS:
    case LogEventType.HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS:
      return getTextForResponseHeadersExtraParam(entry);

    default:
      var out = [];
      for (var k in entry.params) {
        out.push(' --> ' + k + ' = ' +
                 JSON.stringify(entry.params[k]));
      }
      return out.join('\n');
  }
}

/**
 * Indent |lines| by |start|.
 *
 * For example, if |start| = ' -> ' and |lines| = ['line1', 'line2', 'line3']
 * the output will be:
 *
 *   " -> line1\n" +
 *   "    line2\n" +
 *   "    line3"
 */
function indentLines(start, lines) {
  return start + lines.join('\n' + makeRepeatedString(' ', start.length));
}

function getTextForRequestHeadersExtraParam(entry) {
  var params = entry.params;

  // Strip the trailing CRLF that params.line contains.
  var lineWithoutCRLF = params.line.replace(/\r\n$/g, '');

  return indentLines(' --> ', [lineWithoutCRLF].concat(params.headers));
}

function getTextForResponseHeadersExtraParam(entry) {
  return indentLines(' --> ', entry.params.headers);
}

function getTextForEvent(entry) {
  var text = '';

  if (entry.isBegin() && entry.end && entry.end.index == entry.index + 1 &&
      !entry.end.orig.params) {
    // Don't prefix with '+' if we are going to collapse the END event.
    text = ' ';
  } else if (entry.isBegin()) {
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

