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
  var f = null;
  if (document.getElementById) {
    f = document.getElementById("gaia_loginform");
  } else if (window.gaia_loginform) {
    f = window.gaia_loginform;
  }
  if (f) {
    if (f.Email && (f.Email.value == null || f.Email.value == "")) {
      f.Email.focus();
    } else if (f.Passwd) {
      f.Passwd.focus();
    }
  }
}

function showGaiaLogin(args) {
  document.getElementById('logging_in_throbber').style.display = "none";

  document.getElementById('Email').disabled = false;
  document.getElementById('Passwd').disabled = false;

  var f = document.getElementById("gaia_loginform");
  if (f) {
    if (args.user != undefined) {
      if (f.Email.value != args.user)
        f.Passwd.value = ""; // Reset the password field
      f.Email.value = args.user;
    }

    if (!args.editable_user) {
      f.Email.style.display = 'none';
      var span = document.getElementById('email_readonly');
      span.appendChild(document.createTextNode(f.Email.value));
      span.style.display = 'inline';
      setElementDisplay("createaccountdiv", "none");
    }

    f.AccessCode.disabled = true;
  }

  if (1 == args.error) {
    var access_code = document.getElementById('AccessCode');
    if (access_code.value && access_code.value != "") {
      setElementDisplay("errormsg_0_AccessCode", 'block');
      showAccessCodeRequired();
    } else {
      setElementDisplay("errormsg_1_Password", 'table-row');
    }
    setBlurbError(args.error_message);
  } else if (3 == args.error) {
    setElementDisplay("errormsg_0_Connection", 'table-row');
    setBlurbError(args.error_message);
  } else if (4 == args.error) {
    showCaptcha(args);
  } else if (8 == args.error) {
    showAccessCodeRequired();
  } else if (args.error_message) {
    setBlurbError(args.error_message);
  }

  document.getElementById("signIn").disabled = false;
  document.getElementById("signIn").value = templateData['signin'];
  gaia_setFocus();
}

function showCaptcha(args) {
  g_is_captcha_challenge_active = true;

  // The captcha takes up lots of space, so make room.
  setElementDisplay("top_blurb", "none");
  setElementDisplay("top_blurb_error", "none");
  setElementDisplay("createaccountdiv", "none");
  var gaiaTable = document.getElementById('gaia_table');
  gaiaTable.cellPadding = 0;
  gaiaTable.cellSpacing = 1;
  document.getElementById('createaccountcell').height = 0;

  // It's showtime for the captcha now.
  setElementDisplay("captchadiv", "block");
  document.getElementById('Email').disabled = true;
  document.getElementById('Passwd').disabled = false;
  document.getElementById('CaptchaValue').disabled = false;
  document.getElementById('captcha_wrapper').style.backgroundImage =
      url(args.captchaUrl);
}

function showAccessCodeRequired() {
  setElementDisplay("password_row", "none");
  setElementDisplay("email_row", "none");
  document.getElementById("createaccountcell").style.visibility =
      "hidden";

  setElementDisplay("access_code_label_row", "table-row");
  setElementDisplay("access_code_input_row", "table-row");
  setElementDisplay("access_code_help_row", "table-row");
  document.getElementById('AccessCode').disabled = false;
}

function CloseDialog() {
  chrome.send("DialogClose", [""]);
}

function showGaiaSuccessAndClose() {
  document.getElementById("signIn").value = templateData['success'];
  setTimeout(CloseDialog, 1600);
}

function showGaiaSuccessAndSettingUp() {
  document.getElementById("signIn").value = templateData['settingup'];
}

/**
 * DOMContentLoaded handler, sets up the page.
 */
function load() {
  var acct_text = document.getElementById("gaia_account_text");
  var translated_text = acct_text.textContent;
  var posGoogle = translated_text.indexOf('Google');
  if (posGoogle != -1) {
    var ltr = templateData['textdirection'] == 'ltr';
    var googleIsAtEndOfSentence = posGoogle != 0;
    if (googleIsAtEndOfSentence == ltr) {
      // We're in ltr and in the translation the word 'Google' is AFTER the
      // word 'Account' OR we're in rtl and 'Google' is BEFORE 'Account'.
      var logo_td = document.getElementById('gaia_logo');
      logo_td.parentNode.appendChild(logo_td);
    }
    acct_text.textContent = translated_text.replace('Google','');
  }

  var loginForm = document.getElementById("gaia_loginform");
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

  document.getElementById('Email').disabled = true;
  document.getElementById('Passwd').disabled = true;
  document.getElementById('CaptchaValue').disabled = true;
  document.getElementById('AccessCode').disabled = true;

  document.getElementById('logging_in_throbber').style.display = "block";

  var f = document.getElementById("gaia_loginform");
  var result = JSON.stringify({"user" : f.Email.value,
                               "pass" : f.Passwd.value,
                               "captcha" : f.CaptchaValue.value,
                               "access_code" : f.AccessCode.value});
  document.getElementById("signIn").disabled = true;
  chrome.send("SubmitAuth", [result]);
}

function setElementDisplay(id, display) {
  var d = document.getElementById(id);
  if (d)
    d.style.display = display;
}

function hideBlurb() {
  setElementDisplay('top_blurb', 'none');
}

function setBlurbError(error_message) {
  if (g_is_captcha_challenge_active)
    return;  // No blurb in captcha challenge mode.
  if (error_message) {
    document.getElementById('error_signing_in').style.display = 'none';
    document.getElementById('error_custom').style.display = 'inline';
    document.getElementById('error_custom').textContent = error_message;
  } else {
    document.getElementById('error_signing_in').style.display = 'inline';
    document.getElementById('error_custom').style.display = 'none';
  }
  document.getElementById("top_blurb_error").style.visibility = "visible";
  document.getElementById('Email').disabled = false;
  document.getElementById('Passwd').disabled = false;
}

function resetErrorVisibility() {
  setElementDisplay("errormsg_0_Email", 'none');
  setElementDisplay("errormsg_0_Password", 'none');
  setElementDisplay("errormsg_1_Password", 'none');
  setElementDisplay("errormsg_0_Connection", 'none');
  setElementDisplay("errormsg_0_AccessCode", 'none');
}

function setErrorVisibility() {
  resetErrorVisibility();
  var f = document.getElementById("gaia_loginform");
  if (null == f.Email.value || "" == f.Email.value) {
    setElementDisplay("errormsg_0_Email", 'table-row');
    setBlurbError();
    return false;
  }
  if (null == f.Passwd.value || "" == f.Passwd.value) {
    setElementDisplay("errormsg_0_Password", 'table-row');
    setBlurbError();
    return false;
  }
  if (!f.AccessCode.disabled && (null == f.AccessCode.value ||
      "" == f.AccessCode.value)) {
    setElementDisplay("errormsg_0_Password", 'table-row');
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
