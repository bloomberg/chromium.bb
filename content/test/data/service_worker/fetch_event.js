// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.onfetch = function(event) {
    response = new Response({
        statusCode: 200,
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
