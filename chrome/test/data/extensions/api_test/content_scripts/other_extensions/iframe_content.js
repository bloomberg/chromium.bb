// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.addEventListener('DOMContentLoaded', function() {
  var id = window.setInterval(function() {
    clearInterval(id);
    var parent_extension_page = unescape(location.hash.replace('#', ''));

    console.log('PAGE: Sending content to parent extension page - ' +
                parent_extension_page);
    window.parent.postMessage(document.getElementById('content').innerText,
                              parent_extension_page);
  }, 10);
});
