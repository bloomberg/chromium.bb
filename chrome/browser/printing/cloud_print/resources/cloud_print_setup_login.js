// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cloudprint', function() {
  function learnMore() {
    chrome.send('LearnMore', ['']);
    chrome.send('DialogClose', ['']);
  }

  function fixUpTemplateLink() {
    var elm = $('anywhere-explain');
    if (elm)
      elm.innerHTML = elm.textContent;
  }

  return {
    learnMore: learnMore,
    fixUpTemplateLink: fixUpTemplateLink
  };
});


