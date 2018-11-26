// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('message', event => {
  if (event.data.command == 'navigate') {
    const url = event.data.url;
    event.source.navigate(url);
  }
});

self.addEventListener('notificationclick', event => {
  clients.openWindow('empty.html');
});

