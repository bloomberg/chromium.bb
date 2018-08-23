// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var parentWindow = null;

window.onload = () => {
  document.body.addEventListener('wheel', (e) => {
    parentWindow.postMessage('Seen wheel event', '*');
  });
};

window.addEventListener('message', (e) => {
  parentWindow = e.source;
  parentWindow.postMessage('WebViewTest.LAUNCHED', '*');
});
