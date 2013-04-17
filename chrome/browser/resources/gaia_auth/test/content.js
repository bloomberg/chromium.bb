// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('Test script injected!');

function getQueryParam(key, defaultVal) {
  if (!defaultVal) defaultVal = '';
  key = key.replace(/[\[]/, '\\\[').replace(/[\]]/, '\\\]');
  var regex = new RegExp('[\\?&]' + key + '=([^&#]*)');
  var qs = regex.exec(window.location.href);
  if (qs == null)
    return defaultVal;
  else
    return qs[1];
}

console.log(document.URL);
if (document.URL.match(/https\:\/\/www\.google\.com\/accounts\/ServiceLogin/) ||
    document.URL.match(/https\:\/\/accounts\.google\.com\/ServiceLogin/) ||
    document.URL.match(
        /https\:\/\/gaiastaging\.corp\.google\.com\/ServiceLogin/) ||
    document.URL.match(/https\:\/\/insecure\.com\/accounts\/ServiceLogin/) ||
  document.URL.match(/http\:\/\/localhost(:[0-9]*)?\/ServiceLogin/)) {
  var testEmail = unescape(getQueryParam('test_email'));
  var testPassword = unescape(getQueryParam('test_pwd'));
  var testContinue = unescape(getQueryParam('continue'));
  console.log('Got test account info: ' + testEmail + '/' + testPassword);
  document.getElementById('Email').value = testEmail;
  document.getElementById('Passwd').value = testPassword;
  document.getElementById('continue').value = testContinue;
  console.log('Form field changed!');
  if (testEmail != '') {
    document.getElementById('signIn').click();
    console.log('Form submitted!');
  }
}
