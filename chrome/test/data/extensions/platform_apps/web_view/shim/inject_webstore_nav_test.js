// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

embedder.processMessage = function(data) {
  switch (data[0]) {
    case 'navigate':
      window.location.href = data[1];
      return true;
    case 'isalive':
      var msg = ['alive'];
      embedder.channel.postMessage(JSON.stringify(msg), '*');
      return true;
    default:
      return false;
  }
};
