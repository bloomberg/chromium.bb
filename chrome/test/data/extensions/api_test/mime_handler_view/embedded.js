// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.addEventListener('message', function(event) {
  // Echo the data back to the source window.
  event.source.postMessage(event.data, '*');
}, false);