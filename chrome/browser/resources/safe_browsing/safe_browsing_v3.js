// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Must match the commands handled by SafeBrowsingBlockingPage::CommandReceived.
var SB_CMD_DO_REPORT = 'doReport';
var SB_CMD_DONT_REPORT = 'dontReport';
var SB_CMD_EXPANDED_SEE_MORE = 'expandedSeeMore';
var SB_CMD_LEARN_MORE_2 = 'learnMore2';
var SB_CMD_PROCEED = 'proceed';
var SB_CMD_REPORT_ERROR = 'reportError';
var SB_CMD_SHOW_DIAGNOSTIC = 'showDiagnostic';
var SB_CMD_SHOW_PRIVACY = 'showPrivacy';
var SB_CMD_TAKE_ME_BACK = 'takeMeBack';

// Other constants defined in safe_browsing_blocking_page.cc.
var SB_BOX_CHECKED = 'boxchecked';
var SB_DISPLAY_CHECK_BOX = 'displaycheckbox';

function applySafeBrowsingStyle() {
  // Dynamically add the second and third paragraphs.
  var secondParagraph = document.createTextNode(
      loadTimeData.getString('secondParagraph'));
  $('second-paragraph').appendChild(secondParagraph);
  if (!loadTimeData.getBoolean('phishing')) {
    var thirdParagraph = document.createTextNode(
        loadTimeData.getString('thirdParagraph'));
    $('third-paragraph').appendChild(thirdParagraph);
  }

  // Add the link to the diagnostic page (malware) or reporting (phishing).
  // TODO(felt): Put the link definition into the grd file once we have new
  // wording.
  var detailsText = document.createTextNode(
      loadTimeData.getString('detailsText'));
  var detailsLink = document.createElement('a');
  detailsLink.setAttribute('href', '#');
  detailsLink.setAttribute('id', 'help-link');
  detailsLink.appendChild(detailsText);
  $('explanation-paragraph').appendChild(detailsLink);

  // Add the link to proceed.
  if (loadTimeData.getBoolean('overridable')) {
    var proceedText = document.createTextNode(
        loadTimeData.getString('proceedText'));
    var proceedLink = document.createElement('a');
    proceedLink.setAttribute('href', '#');
    proceedLink.setAttribute('id', 'proceed-link');
    proceedLink.appendChild(proceedText);
    $('final-paragraph').appendChild(proceedLink);
  }

  // Swap in the appropriate styling.
  $('body').classList.add('safe-browsing');

  // Remove the 'hidden' class from the paragraphs that we use.
  $('second-paragraph').classList.toggle('hidden');
  if (!loadTimeData.getBoolean('phishing'))
    $('third-paragraph').classList.toggle('hidden');
  $('explanation-paragraph').classList.toggle('hidden');
  $('final-paragraph').classList.toggle('hidden');
}
