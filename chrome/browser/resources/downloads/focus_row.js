// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  /**
   * @param {!Element} root
   * @param {?Element} boundary
   * @constructor
   * @extends {cr.ui.FocusRow}
   */
  function FocusRow(root, boundary) {
    cr.ui.FocusRow.call(this, root, boundary);

    assert(this.addItem('name', '[is="action-link"].name'));
    assert(this.addItem('url', '.src-url'));
    assert(this.addItem('show-retry', '.safe .controls .show'));
    assert(this.addItem('show-retry', '.retry'));
    assert(this.addItem('pause-resume', '.pause'));
    assert(this.addItem('pause-resume', '.resume'));
    assert(this.addItem('remove', '.remove'));
    assert(this.addItem('cancel', '.cancel'));
    assert(this.addItem('restore-save', '.restore'));
    assert(this.addItem('restore-save', '.save'));
    assert(this.addItem('remove-discard', '.remove'));
    assert(this.addItem('remove-discard', '.discard'));
  }

  FocusRow.prototype = {__proto__: cr.ui.FocusRow.prototype};

  return {FocusRow: FocusRow};
});
