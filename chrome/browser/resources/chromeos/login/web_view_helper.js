// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Web view helper.
 */

/** Web view helper shared between OOBE screens. */
class WebViewHelper {
  /**
   * Loads text/html contents from the given url into the given webview. The
   * content is loaded via XHR and is sent to webview via data url so that it
   * is properly sandboxed.
   *
   * @param {!WebView} webView web view element to host the text/html content.
   * @param {string} url URL to load text/html from.
   */
  static loadUrlToWebview(webView, url) {
    assert(webView.tagName === 'WEBVIEW');

    const onError = function() {
      webView.src = 'about:blank';
    };

    const setContents = function(contents) {
      webView.src =
          'data:text/html;charset=utf-8,' + encodeURIComponent(contents);
    };

    var xhr = new XMLHttpRequest();
    xhr.open('GET', url);
    xhr.setRequestHeader('Accept', 'text/html');
    xhr.onreadystatechange = function() {
      if (xhr.readyState != XMLHttpRequest.DONE)
        return;
      if (xhr.status != 200) {
        onError();
        return;
      }

      var contentType = xhr.getResponseHeader('Content-Type');
      if (contentType && !contentType.includes('text/html')) {
        onError();
        return;
      }

      setContents(xhr.response);
    };

    try {
      xhr.send();
    } catch (e) {
      onError();
    }
  }
}