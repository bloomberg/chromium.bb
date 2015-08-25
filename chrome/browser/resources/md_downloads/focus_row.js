// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  /**
   * @param {!Element} root
   * @param {?Node} boundary
   * @constructor
   * @extends {cr.ui.FocusRow}
   */
  function FocusRow(root, boundary) {
    cr.ui.FocusRow.call(this, root, boundary);

    this.addItem('name-file-link',
                 '#content.is-active:not(.show-progress) #name');
    assert(this.addItem('name-file-link', '#file-link'));
    assert(this.addItem('url', '#url'));
    assert(this.addItem('show-retry', '#show'));
    assert(this.addItem('show-retry', '#retry'));
    assert(this.addItem('pause-resume', '#pause'));
    assert(this.addItem('pause-resume', '#resume'));
    assert(this.addItem('cancel', '#cancel'));
    this.addItem('controlled-by', '#controlled-by a');
    assert(this.addItem('danger-remove-discard', '#discard'));
    assert(this.addItem('restore-save', '#save'));
    assert(this.addItem('danger-remove-discard', '#danger-remove'));
    assert(this.addItem('restore-save', '#restore'));
    assert(this.addItem('remove', '#remove'));
  }

  FocusRow.prototype = {__proto__: cr.ui.FocusRow.prototype};

  return {FocusRow: FocusRow};
});
