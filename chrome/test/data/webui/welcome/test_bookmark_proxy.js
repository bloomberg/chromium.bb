// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {nux.BookmarkProxy} */
class TestBookmarkProxy extends TestBrowserProxy {
  constructor() {
    super([
      'addBookmark',
      'isBookmarkBarShown',
      'removeBookmark',
      'toggleBookmarkBar',
    ]);

    this.fakeBookmarkId = 1;
  }

  /** @override */
  addBookmark(data, callback) {
    this.methodCalled('addBookmark', data);
    callback({id: this.fakeBookmarkId++});
  }

  /** @override */
  isBookmarkBarShown() {
    this.methodCalled('isBookmarkBarShown');

    // TODO(scottchen): make changable to test both true/false cases.
    return Promise.resolve(true);
  }

  /** @override */
  removeBookmark(id) {
    this.methodCalled('removeBookmark', id);
  }

  /** @override */
  toggleBookmarkBar(show) {
    this.methodCalled('toggleBookmarkBar', show);
  }
}
