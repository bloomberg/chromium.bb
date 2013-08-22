// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var contentWatcherNative = requireNative("contentWatcherNative");

// Watches the page for all changes and calls FrameMutated (a C++ callback) in
// response.
var mutation_observer = new MutationObserver(contentWatcherNative.FrameMutated);

// This runs once per frame, when the module is 'require'd.
mutation_observer.observe(document, {
  childList: true,
  attributes: true,
  characterData: true,
  subtree: true});
