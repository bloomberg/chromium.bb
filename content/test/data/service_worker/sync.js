// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var code = 404;

this.onfetch = function(event) {
    response = new Response({
        statusCode: code,
        statusText: 'OK',
        method: 'GET',
        headers: {
            'Content-Language': 'fi',
            'Content-Type': 'text/html; charset=UTF-8'
        }
    });

    event.respondWith(new Promise(function(r) {
        setTimeout(function() { r(response); }, 5);
    }));
};

this.onsync = function(event) {
    code = 200;
};