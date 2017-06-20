// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Processes select tag that contains list of available terms for different
 * languages and zones. In case of initial load, tries to find terms that match
 * exactly current language and country code and automatically redirects the
 * view in case such terms are found. Leaves terms in select tag that only match
 * current language or country code or default English variant or currently
 * selected. Note that document.countryCode must be set before calling this
 * function.
 */
function processLangZoneTerms() {
  var doc = document;
  var selectLangZoneTerms =
      doc.getElementById('play-footer').getElementsByTagName('select')[0];

  var initialLoad =
      window.location.href == 'https://play.google.com/about/play-terms.html';
  var langSegments = navigator.language.split('-');
  if (initialLoad) {
    var applyTermsForLangAndZone = function(termsLang) {
      var matchByLangZone = '/intl/' + termsLang + '_' + document.countryCode +
          '/about/play-terms.html';
      for (var i = selectLangZoneTerms.options.length - 1; i >= 0; --i) {
        var option = selectLangZoneTerms.options[i];
        if (option.value == matchByLangZone) {
          window.location.href = option.value;
          return true;
        }
      }
      return false;
    };

    // Try two versions of the language, full and short (if it exists, for
    // example en-GB -> en). Note, terms may contain entries for both types, for
    // example: en_ie, es-419_ar, es_as, pt-PT_pt.
    if (applyTermsForLangAndZone(navigator.language)) {
      return;
    }
    if (langSegments.length == 2 && applyTermsForLangAndZone(langSegments[0])) {
      return;
    }
  }

  var matchByLang = '/intl/' + navigator.language + '_';
  var matchByLangShort = null;
  if (langSegments.length == 2) {
    matchByLangShort = '/intl/' + langSegments[0] + '_';
  }

  var matchByZone = '_' + document.countryCode + '/about/play-terms.html';
  var matchByDefault = '/intl/en/about/play-terms.html';

  // We are allowed to display terms by default only in language that matches
  // current UI language. In other cases we have to switch to default version.
  var langMatch = false;
  var defaultExist = false;

  for (var i = selectLangZoneTerms.options.length - 1; i >= 0; --i) {
    var option = selectLangZoneTerms.options[i];
    if (selectLangZoneTerms.selectedIndex == i) {
      langMatch = option.value.startsWith(matchByLang) ||
          (matchByLangShort && option.value.startsWith(matchByLangShort));
      continue;
    }
    if (option.value == matchByDefault) {
      defaultExist = true;
      continue;
    }
    if (!option.value.startsWith(matchByLang) &&
        !option.value.endsWith(matchByZone) &&
        !(matchByLangShort && option.value.startsWith(matchByLangShort)) &&
        option.text != 'English') {
      selectLangZoneTerms.removeChild(option);
    }
  }
  if (initialLoad && !langMatch && defaultExist) {
    window.location.href = matchByDefault;
  } else {
    // Show content once we reached target url.
    document.body.hidden = false;
  }
}

/**
 * Formats current document in order to display it correctly.
 */
function formatDocument() {
  if (document.viewMode) {
    document.body.classList.add(document.viewMode);
  }
  // playstore.css is injected into the document and it is applied first.
  // Need to remove existing links that contain references to external
  // stylesheets which override playstore.css.
  var links = document.head.getElementsByTagName('link');
  for (var i = links.length - 1; i >= 0; --i) {
    document.head.removeChild(links[i]);
  }

  // Create base element that forces internal links to be opened in new window.
  var base = document.createElement('base');
  base.target = '_blank';
  document.head.appendChild(base);

  // Remove header element that contains logo image we don't want to show in
  // our view.
  var doc = document;
  document.body.removeChild(doc.getElementById('play-header'));

  // Hide content at this point. We might want to redirect our view to terms
  // that exactly match current language and country code.
  document.body.hidden = true;
}

/**
 * Searches in footer for a privacy policy link.
 * @return {string} Link to Google Privacy Policy detected from the current
 *                  document or link to the default policy if it is not found.
 */
function getPrivacyPolicyLink() {
  var doc = document;
  var links = doc.getElementById('play-footer').getElementsByTagName('a');
  for (var i = 0; i < links.length; ++i) {
    var targetURL = links[i].href;
    if (targetURL.endsWith('/policies/privacy/')) {
      return targetURL;
    }
  }
  return 'https://www.google.com/policies/privacy/';
}

formatDocument();
processLangZoneTerms();
