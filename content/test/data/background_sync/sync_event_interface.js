// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.onsync = function(event) {
    var eventProperties = [
      // Extract name from toString result: "[object <Class>]"
      Object.prototype.toString.call(event).match(/\s([a-zA-Z]+)/)[1],
      (typeof event.waitUntil)
    ];

    console.log(eventProperties.join('|'));
};
