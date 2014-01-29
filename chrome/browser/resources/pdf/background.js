// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.streamsPrivate.onExecuteMimeTypeHandler.addListener(
  function(mime_type, original_url, content_url, tab_id) {
    // TODO(raymes): Currently this doesn't work with embedded PDFs (it
    // causes the entire frame to navigate). Also work out how we can
    // mask the URL with the URL of the PDF.
    chrome.tabs.update(tab_id, { url: 'index.html?' + content_url });
  }
);
