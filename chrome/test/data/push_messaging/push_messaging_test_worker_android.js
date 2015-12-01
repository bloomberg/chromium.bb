// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var messagePort = null;

onmessage = event => {
  messagePort = event.data;
  messagePort.postMessage('ready');
};

onpush = event =>
  event.waitUntil(registration.showNotification('push notification'));
