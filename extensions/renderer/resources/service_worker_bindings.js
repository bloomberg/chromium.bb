// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(backgroundUrl) {
  'use strict';

  self.chrome = self.chrome || {};

  self.chrome.getBackgroundClient = function() { return new Promise(
   function(resolve, reject) {
     self.clients.matchAll({
      includeUncontrolled: true,
      type: 'window'
     }).then(function(clients) {
      for (let client of clients) {
         if (client.url == backgroundUrl) {
           resolve(client);
           return;
         }
       }
       reject("BackgroundClient ('" + backgroundUrl + "') does not exist.")
     })
    });
  }
});
