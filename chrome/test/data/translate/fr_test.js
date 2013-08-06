// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (typeof world != 'undefined') {
  div_log('CSP doesn\'t work');
  document.title = 'FAIL';
  return;
}
world='main';
