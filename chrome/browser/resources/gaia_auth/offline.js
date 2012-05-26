// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline login implementation.
 */

function load() {
  var params = getUrlSearchParams(location.search);

  // Setup localized strings.
  $('sign-in-title').textContent = decodeURIComponent(params['stringSignIn']);
  $('email-label').textContent = decodeURIComponent(params['stringEmail']);
  $('password-label').textContent =
      decodeURIComponent(params['stringPassword']);
  $('submit-button').value = decodeURIComponent(params['stringSignIn']);
  $('errormsg-alert').textContent = decodeURIComponent(params['stringError']);

  // Setup actions.
  var form = $('offline-login-form');
  form.addEventListener('submit', function(e) {
    var msg = {
      'method': 'offlineLogin',
      'email': form.email.value,
      'password': form.password.value
    };
    window.parent.postMessage(msg, 'chrome://oobe/');
    e.preventDefault();
  });

  var email = params['email'];
  if (email) {
    // Email is present, which means that unsuccessful login attempt has been
    // made. Try to mimic Gaia's behaviour.
    form.email.value = email;
    form.password.classList.add('form-error');
    form.password.focus();
  } else {
    form.email.focus();
  }
  window.parent.postMessage({'method': 'loginUILoaded'}, 'chrome://oobe/');
}

document.addEventListener('DOMContentLoaded', load);
