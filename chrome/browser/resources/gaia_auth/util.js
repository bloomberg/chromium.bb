// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
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
