// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('nux', function() {
  /** @interface */
  class NuxEmailProxy {
    /** @param {string} id ID provided by callback when bookmark was added. */
    removeBookmark(id) {}

    /**
     * @param {!bookmarkData} data
     * @param {!Function} callback
     */
    addBookmark(data, callback) {}
  }

  /** @implements {NuxEmailProxy} */
  class NuxEmailProxyImpl {
    /** @override */
    removeBookmark(id) {
      chrome.bookmarks.remove(id);
    }

    /** @override */
    addBookmark(data, callback) {
      chrome.bookmarks.create(data, callback);
      // TODO(scottchen): request C++ to cache favicon
    }
  }

  cr.addSingletonGetter(NuxEmailProxyImpl);

  return {
    NuxEmailProxy: NuxEmailProxy,
    NuxEmailProxyImpl: NuxEmailProxyImpl,
  };
});