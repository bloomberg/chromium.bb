// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var PaintTimelineView;

(function() {
'use strict';

PaintTimelineView = function(sourceEntries, node) {
  addTextNode(node, 'TODO(eroman): Draw some sort of waterfall.');

  addNode(node, 'br');
  addNode(node, 'br');

  addTextNode(node, 'Selected nodes (' + sourceEntries.length + '):');
  addNode(node, 'br');

}

})();
