// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TODO(eroman): This needs better presentation, and cleaner code. This
 *               implementation is more of a transitionary step as
 *               the old net-internals is replaced.
 */

var PaintLogView;

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

  var groupedEntries = LogGroupEntry.createArrayFrom(
      sourceEntry.getLogEntries());

  makeLoadLogTable_(div, groupedEntries);
}

function makeLoadLogTable_(node, entries) {
  var table = addNode(node, 'table');
  var tbody = addNode(node, 'tbody');

  for (var i = 0; i < entries.length; ++i) {
    var entry = entries[i];

    // Avoid printing the END for a BEGIN that was immediately before.
    if (entry.isEnd() && entry.begin && entry.begin.index == i - 1) {
      continue;
    }

    var tr = addNode(node, 'tr');

    var timeLabelCell = addNode(tr, 'td');
    addTextNode(timeLabelCell, 't=');

    var timeCell = addNode(tr, 'td');
    timeCell.style.textAlign = 'right';
    timeCell.style.paddingRight = '5px';
    addTextNode(timeCell, entry.orig.time);

    var mainCell = addNode(tr, 'td');
    mainCell.style.paddingRight = '5px';
    var dtLabelCell = addNode(tr, 'td');
    var dtCell = addNode(tr, 'td');
    dtCell.style.textAlign = 'right';

    mainCell.style.paddingLeft = (INDENTATION_PX * entry.getDepth()) + "px";

    addTextNode(mainCell, getTextForEvent(entry));

    // Get the elapsed time.
    if (entry.isBegin()) {
      addTextNode(dtLabelCell, '[dt=');

      // Definite time.
      if (entry.end) {
        var dt = entry.end.orig.time - entry.orig.time;
        addTextNode(dtCell, dt + ']');
      } else {
        addTextNode(dtCell, '?]');
      }
    } else {
      mainCell.colSpan = '3';
    }

    // Output the extra parameters.
    // TODO(eroman): Do type-specific formatting.
    if (entry.orig.extra_parameters != undefined) {
       addNode(mainCell, 'br');
      addTextNode(mainCell, 'params: ' + entry.orig.extra_parameters);
    }
  }
}

function getTextForEvent(entry) {
  var text = '';

  if (entry.isBegin()) {
    text = '+' + text;
  } else if (entry.isEnd()) {
    text = '-' + text;
  } else {
    text = '.';
  }

  text += getKeyWithValue(LogEventType, entry.orig.type);
  return text;
}

// End of anonymous namespace.
})();

