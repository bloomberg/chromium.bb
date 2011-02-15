// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Variable to track if a captcha challenge was issued. If this gets set to
// true, it stays that way until we are told about successful login from
// the browser.  This means subsequent errors (like invalid password) are
// rendered in the captcha state, which is basically identical except we
// don't show the top error blurb "Error Signing in" or the "Create
// account" link.
var g_is_captcha_challenge_active = false;

// Taken from new_new_tab.js.
// TODO(tim): Can this be unified?
function url(s) {
  // http://www.w3.org/TR/css3-values/#uris
  // Parentheses, commas, whitespace characters, single quotes (') and
  // double quotes (") appearing in a URI must be escaped with a backslash
  var s2 = s.replace(/(\(|\)|\,|\s|\'|\"|\\)/g, '\\$1');
  // WebKit has a bug when it comes to URLs that end with \
  // https://bugs.webkit.org/show_bug.cgi?id=28885
  if (/\\\\$/.test(s2)) {
    // Add a space to work around the WebKit bug.
    s2 += ' ';
  }
  return 'url("' + s2 + '")';
}

function gaia_setFocus() {
  var form = document.getElementById("gaia-login-form");
  if (form.email && (form.email.value == null || form.email.value == "")) {
    form.email.focus();
  } else if (form.passwd) {
    form.passwd.focus();
  }
}

function showGaiaLogin(args) {
  document.getElementById('logging-in-throbber').style.display = "none";

  document.getElementById('email').disabled = false;
  document.getElementById('passwd').disabled = false;

  var f = document.getElementById("gaia-login-form");
  if (f) {
    if (args.user != undefined) {
      if (f.email.value != args.user)
        f.passwd.value = ""; // Reset the password field
      f.email.value = args.user;
    }

    if (!args.editable_user) {
      f.email.style.display = 'none';
      var span = document.getElementById('email-readonly');
      span.appendChild(document.createTextNode(f.email.value));
      span.style.display = 'inline';
      setElementDisplay("create-account-div", "none");
    }

    f.accessCode.disabled = true;
  }

  if (1 == args.error) {
    var access_code = document.getElementById('access-code');
    if (access_code.value && access_code.value != "") {
      setElementDisplay("errormsg-0-access-code", 'block');
      showAccessCodeRequired();
    } else {
      setElementDisplay("errormsg-1-password", 'table-row');
    }
    setBlurbError(args.error_message);
  } else if (3 == args.error) {
    setElementDisplay("errormsg-0-connection", 'table-row');
    setBlurbError(args.error_message);
  } else if (4 == args.error) {
    showCaptcha(args);
  } else if (8 == args.error) {
    showAccessCodeRequired();
  } else if (args.error_message) {
    setBlurbError(args.error_message);
  }

  document.getElementById("sign-in").disabled = false;
  document.getElementById("sign-in").value = templateData['signin'];
  gaia_setFocus();
}

function showCaptcha(args) {
  g_is_captcha_challenge_active = true;

  // The captcha takes up lots of space, so make room.
  setElementDisplay("top-blurb", "none");
  setElementDisplay("top-blurb-error", "none");
  setElementDisplay("create-account-div", "none");
  document.getElementById('create-account-cell').height = 0;

  // It's showtime for the captcha now.
  setElementDisplay("captcha-div", "block");
  document.getElementById('email').disabled = true;
  document.getElementById('passwd').disabled = false;
  document.getElementById('captcha-value').disabled = false;
  document.getElementById('captcha-wrapper').style.backgroundImage =
      url(args.captchaUrl);
}

function showAccessCodeRequired() {
  setElementDisplay("password-row", "none");
  setElementDisplay("email-row", "none");
  document.getElementById("create-account-cell").style.visibility =
      "hidden";

  setElementDisplay("access-code-label-row", "table-row");
  setElementDisplay("access-code-input-row", "table-row");
  setElementDisplay("access-code-help-row", "table-row");
  document.getElementById('access-code').disabled = false;
}

function CloseDialog() {
  chrome.send("DialogClose", [""]);
}

function showGaiaSuccessAndClose() {
  document.getElementById("sign-in").value = templateData['success'];
  setTimeout(CloseDialog, 1600);
}

function showGaiaSuccessAndSettingUp() {
  document.getElementById("sign-in").value = templateData['settingup'];
}

/**
 * DOMContentLoaded handler, sets up the page.
 */
function load() {
  var acct_text = document.getElementById("gaia-account-text");
  var translated_text = acct_text.textContent;
  var posGoogle = translated_text.indexOf('Google');
  if (posGoogle != -1) {
    var ltr = templateData['textdirection'] == 'ltr';
    var googleIsAtEndOfSentence = posGoogle != 0;
    if (googleIsAtEndOfSentence == ltr) {
      // We're in ltr and in the translation the word 'Google' is AFTER the
      // word 'Account' OR we're in rtl and 'Google' is BEFORE 'Account'.
      var logo_td = document.getElementById('gaia-logo');
      logo_td.parentNode.appendChild(logo_td);
    }
    acct_text.textContent = translated_text.replace('Google','');
  }

  var loginForm = document.getElementById("gaia-login-form");
  loginForm.onsubmit = function() {
    sendCredentialsAndClose();
    return false;
  };

  var gaiaCancel = document.getElementById("gaia-cancel");
  gaiaCancel.onclick = function() {
    CloseDialog();
  };

  var args = JSON.parse(chrome.dialogArguments);
  showGaiaLogin(args);
}

function sendCredentialsAndClose() {
  if (!setErrorVisibility())
    return false;

  document.getElementById('email').disabled = true;
  document.getElementById('passwd').disabled = true;
  document.getElementById('captcha-value').disabled = true;
  document.getElementById('access-code').disabled = true;

  document.getElementById('logging-in-throbber').style.display = "block";

  var f = document.getElementById("gaia-login-form");
  var result = JSON.stringify({"user" : f.email.value,
                               "pass" : f.passwd.value,
                               "captcha" : f.captchaValue.value,
                               "access_code" : f.accessCode.value});
  document.getElementById("sign-in").disabled = true;
  chrome.send("SubmitAuth", [result]);
}

function setElementDisplay(id, display) {
  var d = document.getElementById(id);
  if (d)
    d.style.display = display;
}

function hideBlurb() {
  setElementDisplay('top-blurb', 'none');
}

function setBlurbError(error_message) {
  if (g_is_captcha_challenge_active)
    return;  // No blurb in captcha challenge mode.
  if (error_message) {
    document.getElementById('error-signing-in').style.display = 'none';
    document.getElementById('error-custom').style.display = 'inline';
    document.getElementById('error-custom').textContent = error_message;
  } else {
    document.getElementById('error-signing-in').style.display = 'inline';
    document.getElementById('error-custom').style.display = 'none';
  }
  document.getElementById("top-blurb-error").style.visibility = "visible";
  document.getElementById('email').disabled = false;
  document.getElementById('passwd').disabled = false;
}

function resetErrorVisibility() {
  setElementDisplay("errormsg-0-email", 'none');
  setElementDisplay("errormsg-0-password", 'none');
  setElementDisplay("errormsg-1-password", 'none');
  setElementDisplay("errormsg-0-connection", 'none');
  setElementDisplay("errormsg-0-access-code", 'none');
}

function setErrorVisibility() {
  resetErrorVisibility();
  var f = document.getElementById("gaia-login-form");
  if (null == f.email.value || "" == f.email.value) {
    setElementDisplay("errormsg-0-email", 'table-row');
    setBlurbError();
    return false;
  }
  if (null == f.passwd.value || "" == f.passwd.value) {
    setElementDisplay("errormsg-0-password", 'table-row');
    setBlurbError();
    return false;
  }
  if (!f.accessCode.disabled && (null == f.accessCode.value ||
      "" == f.accessCode.value)) {
    setElementDisplay("errormsg-0-password", 'table-row');
    return false;
  }
  return true;
}

function onPreCreateAccount() {
  return true;
}

function onPreLogin() {
  if (window["onlogin"] != null) {
    return onlogin();
  } else {
    return true;
  }
}

document.addEventListener('DOMContentLoaded', load);
