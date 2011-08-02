// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: cr/ui.js
// require: util.js

cr.ui.decorate('#sync-results-splitter', cr.ui.Splitter);

chrome.sync.decorateSearchControls(
  $('sync-search-query'),
  $('sync-search-status'),
  $('sync-results-list'),
  $('sync-result-details'));
