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

    /** @param {boolean} show */
    toggleBookmarkBar(show) {}

    /** @return {!Array<Object>} Array of email providers. */
    getEmailList() {}
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

    /** @override */
    toggleBookmarkBar(show) {
      chrome.send('toggleBookmarkBar', [show]);
    }

    /** @override */
    getEmailList() {
      let emailCount = loadTimeData.getInteger('email_count');
      let emailList = [];
      for (let i = 0; i < emailCount; ++i) {
        emailList.push({
          name: loadTimeData.getString(`email_name_${i}`),
          icon: loadTimeData.getString(`email_name_${i}`).toLowerCase(),
          url: loadTimeData.getString(`email_url_${i}`)
        })
      }
      return emailList;
    }
  }

  cr.addSingletonGetter(NuxEmailProxyImpl);

  return {
    NuxEmailProxy: NuxEmailProxy,
    NuxEmailProxyImpl: NuxEmailProxyImpl,
  };
});