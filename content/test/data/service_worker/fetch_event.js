// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.onfetch = function(event) {
    var headers = new HeaderMap;
    headers.set('Content-Language', 'fi');
    headers.set('Content-Type', 'text/html; charset=UTF-8');
    var response = new Response({
        status: 301,
        statusText: 'Moved Permanently',
        headers: headers
    });

    event.respondWith(new Promise(function(r) {
        setTimeout(function() { r(response); }, 5);
    }));
};
