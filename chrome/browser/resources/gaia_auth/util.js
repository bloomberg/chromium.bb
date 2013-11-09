// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function $(id) {
  return document.getElementById(id);
}

function getUrlSearchParams(search) {
  var params = {};

  if (search) {
    // Strips leading '?'
    search = search.substring(1);
    var pairs = search.split('&');
    for (var i = 0; i < pairs.length; ++i) {
      var pair = pairs[i].split('=');
      if (pair.length == 2) {
        params[pair[0]] = decodeURIComponent(pair[1]);
      } else {
        params[pair] = true;
      }
    }
  }

  return params;
}

/**
 * Creates a new URL which is the old URL with a GET param of key=value.
 * Copied from ui/webui/resources/js/util.js.
 * @param {string} url The base URL. There is not sanity checking on the URL so
 *     it must be passed in a proper format.
 * @param {string} key The key of the param.
 * @param {string} value The value of the param.
 * @return {string} The new URL.
 */
function appendParam(url, key, value) {
  var param = encodeURIComponent(key) + '=' + encodeURIComponent(value);

  if (url.indexOf('?') == -1)
    return url + '?' + param;
  return url + '&' + param;
}
