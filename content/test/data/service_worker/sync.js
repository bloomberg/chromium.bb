// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var code = 404;

this.onfetch = function(event) {
    response = new Response(new Blob(), {
        status: code,
        statusText: 'OK'
    });

    event.respondWith(new Promise(function(r) {
        setTimeout(function() { r(response); }, 5);
    }));
};

this.onsync = function(event) {
    code = 200;
};
