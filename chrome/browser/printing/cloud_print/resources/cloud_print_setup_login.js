// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

  function showGaiaLogin(args) {
    frames['gaialogin'].showGaiaLogin(args);
    new_height = $('cloudprint-signup').offsetHeight;
    login_height = frames['gaialogin'].document.body.scrollHeight;
    if (login_height > new_height) {
      new_height = login_height;
    }
    $('cloudprint-contents').style.height = new_height + 4 + 'px';
  }

  function showGaiaSuccessAndSettingUp() {
    frames['gaialogin'].showGaiaSuccessAndSettingUp()
  }

  return {
    learnMore: learnMore,
    fixUpTemplateLink: fixUpTemplateLink,
    showGaiaLogin: showGaiaLogin,
    showGaiaSuccessAndSettingUp: showGaiaSuccessAndSettingUp
  };
});
