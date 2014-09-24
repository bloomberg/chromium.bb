// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Should match SSLBlockingPageCommands in ssl_blocking_page.cc.
var CMD_DONT_PROCEED = 0;
var CMD_PROCEED = 1;
var CMD_MORE = 2;
var CMD_RELOAD = 3;
var CMD_HELP = 4;
var CMD_CLOCK = 5;

function setupSSLDebuggingInfo() {
  if (!loadTimeData.getBoolean('ssl'))
    return;

  // The titles are not internationalized because this is debugging information
  // for bug reports, help center posts, etc.
  appendDebuggingField('Subject', loadTimeData.getString('subject'));
  appendDebuggingField('Issuer', loadTimeData.getString('issuer'));
  appendDebuggingField('Expires on', loadTimeData.getString('expirationDate'));
  appendDebuggingField('Current date', loadTimeData.getString('currentDate'));
  appendDebuggingField('PEM encoded chain', loadTimeData.getString('pem'));

  $('error-code').addEventListener('click', toggleDebuggingInfo);
}
