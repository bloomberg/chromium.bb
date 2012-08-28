/*
  Copyright (c) 2012 The Chromium Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

/**
 * @fileoverview Common methods for performance-plotting Javascript.
 */

/**
 * Fetches a URL asynchronously and invokes a callback when complete.
 *
 * @param {string} url URL to fetch.
 * @param {Function(string, string)} callback The function to invoke when the
 *     results of the URL fetch are complete.  The function should accept two
 *     strings representing the URL data, and any errors, respectively.
 */
function Fetch(url, callback) {
  var r = new XMLHttpRequest();
  r.open('GET', url, true);
  r.setRequestHeader('pragma', 'no-cache');
  r.setRequestHeader('cache-control', 'no-cache');

  r.onreadystatechange = function() {
    if (r.readyState == 4) {
      var text = r.responseText;
      var error;
      if (r.status != 200)
        error = url + ': ' + r.status + ': ' + r.statusText;
      else if (!text)
        error = url + ': null response';
      callback(text, error);
    }
  }

  r.send(null);
}

/**
 * Parses the parameters of the current page's URL.
 *
 * @return {Object} An object with properties given by the parameters specified
 *     in the URL's query string.
 */
function ParseParams() {
  var result = new Object();

  var query = window.location.search.substring(1)
  if (query.charAt(query.length - 1) == '/')
    query = query.substring(0, query.length - 1)  // Strip trailing slash.
  var s = query.split('&');

  for (i = 0; i < s.length; ++i) {
    var v = s[i].split('=');
    var key = v[0];
    var value = unescape(v[1]);
    result[key] = value;
  }

  if ('history' in result) {
    result['history'] = parseInt(result['history']);
    result['history'] = Math.max(result['history'], 2);
  }
  if ('rev' in result) {
    result['rev'] = parseInt(result['rev']);
    result['rev'] = Math.max(result['rev'], -1);
  }

  return result;
}

/**
 * Creates the URL constructed from the current pathname and the given params.
 *
 * @param {Object} An object containing parameters for a URL query string.
 * @return {string} The URL constructed from the given params.
 */
function MakeURL(params) {
  var url = window.location.pathname;
  var sep = '?';
  for (p in params) {
    if (!p)
      continue;
    url += sep + p + '=' + params[p];
    sep = '&';
  }
  return url;
}
