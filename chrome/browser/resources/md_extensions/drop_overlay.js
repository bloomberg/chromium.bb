// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

Polymer({
  is: 'extensions-drop-overlay',
  created: function() {
    this.hidden = true;
    const dragTarget = document.documentElement;
    this.dragWrapperHandler_ =
        new extensions.DragAndDropHandler(true, dragTarget);
    dragTarget.addEventListener('extension-drag-started', () => {
      this.hidden = false;
    });
    dragTarget.addEventListener('extension-drag-ended', () => {
      this.hidden = true;
    });
    this.dragWrapper_ =
        new cr.ui.DragWrapper(dragTarget, this.dragWrapperHandler_);
  },
});
})();
