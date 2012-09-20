// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome;
if (!chrome)
  chrome = {};
if (!chrome.searchBox) {
  chrome.searchBox = new function() {
    // =========================================================================
    //                                  Constants
    // =========================================================================

    var MAX_CLIENT_SUGGESTIONS_TO_DEDUPE = 6;

    var HTTP_REGEX = /^https?:\/\//;

    var WWW_REGEX = /^www\./;

    // =========================================================================
    //                            Private functions
    // =========================================================================
    native function GetQuery();
    native function GetVerbatim();
    native function GetSelectionStart();
    native function GetSelectionEnd();
    native function GetX();
    native function GetY();
    native function GetWidth();
    native function GetHeight();
    native function GetAutocompleteResults();
    native function NavigateContentWindow();
    native function SetSuggestions();
    native function SetQuerySuggestion();
    native function SetQuerySuggestionFromAutocompleteResult();
    native function SetQuery();
    native function SetQueryFromAutocompleteResult();
    native function SetPreviewHeight();

    // Returns the |restrictedText| wrapped in a ShadowDOM.
    function SafeWrap(restrictedText) {
      var node = document.createElement('div');
      var nodeShadow = new WebKitShadowRoot(node);
      nodeShadow.applyAuthorStyles = true;
      nodeShadow.innerHTML =
          '<div style="width:700px!important;' +
          '            height:22px!important;' +
          '            font-family:\'Chrome Droid Sans\',\'Arial\'!important;' +
          '            overflow:hidden!important;' +
          '            text-overflow:ellipsis!important">' +
          '  <nobr>' + restrictedText + '</nobr>' +
          '</div>';
      return node;
    }

    // Wraps the AutocompleteResult query and URL into ShadowDOM nodes so that
    // the JS cannot access them and deletes the raw values.
    function GetAutocompleteResultsWrapper() {
      var autocompleteResults = DedupeAutcompleteResults(
          GetAutocompleteResults());
      var userInput = GetQuery();
      for (var i = 0; i < autocompleteResults.length; ++i) {
        var result = autocompleteResults[i];
        var title = result.contents;
        var url = CleanUrl(result.destination_url, userInput);
        var combinedHtml = '<span class=chrome_url>' + url + '</span>';
        if (title) {
          result.titleNode = SafeWrap(title);
          combinedHtml += '<span class=chrome_separator> &ndash; </span>' +
              '<span class=chrome_title>' + title + '</span>';
        }
        result.urlNode = SafeWrap(url);
        result.combinedNode = SafeWrap(combinedHtml);
        delete result.contents;
        delete result.destination_url;
      }
      return autocompleteResults;
    }

    function CleanUrl(url, userInput) {
      if (url.indexOf(userInput) == 0) {
        return url;
      }
      url = url.replace(HTTP_REGEX, '');
      if (url.indexOf(userInput) == 0) {
        return url;
      }
      return url.replace(WWW_REGEX, '');
    }

    function CanonicalizeUrl(url) {
      return url.replace(HTTP_REGEX, '').replace(WWW_REGEX, '');
    }

    // Removes duplicates from AutocompleteResults.
    // TODO(dcblack): Do this in C++ instead of JS.
    function DedupeAutcompleteResults(autocompleteResults) {
      var urlToResultMap = {};
      for (var i = 0; i < autocompleteResults.length; ++i) {
        var result = autocompleteResults[i];
        var url = CanonicalizeUrl(result.destination_url);
        if (url in urlToResultMap) {
          var oldRelevance = urlToResultMap[url].rankingData.relevance;
          var newRelevance = result.rankingData.relevance;
          if (newRelevance > oldRelevance) {
            urlToResultMap[url] = result;
          }
        } else {
          urlToResultMap[url] = result;
        }
      }
      var dedupedResults = [];
      for (url in urlToResultMap) {
        dedupedResults.push(urlToResultMap[url]);
      }
      return dedupedResults;
    }

    var lastPrefixQueriedForDuplicates = '';

    function DedupeClientSuggestions(clientSuggestions) {
      var userInput = GetQuery();
      if (userInput == lastPrefixQueriedForDuplicates) {
        // Request blocked for privacy reasons.
        // TODO(dcblack): This check is insufficient.  We should have a check
        // such that it's only callable once per onnativesuggestions, not
        // once per prefix.
        return false;
      }
      lastPrefixQueriedForDuplicates = userInput;
      var autocompleteResults = GetAutocompleteResults();
      var nativeUrls = {};
      for (var i = 0; i < autocompleteResults.length; ++i) {
        var nativeUrl = CanonicalizeUrl(autocompleteResults[i].destination_url);
        nativeUrls[nativeUrl] = autocompleteResults[i].rid;
      }
      for (var i = 0; i < clientSuggestions.length &&
           i < MAX_CLIENT_SUGGESTIONS_TO_DEDUPE; ++i) {
        var clientUrl = CanonicalizeUrl(clientSuggestions[i].url);
        if (clientUrl in nativeUrls) {
          clientSuggestions[i].duplicateOf = nativeUrls[clientUrl];
        }
      }
      return true;
    }

    // =========================================================================
    //                           Exported functions
    // =========================================================================
    this.__defineGetter__('value', GetQuery);
    this.__defineGetter__('verbatim', GetVerbatim);
    this.__defineGetter__('selectionStart', GetSelectionStart);
    this.__defineGetter__('selectionEnd', GetSelectionEnd);
    this.__defineGetter__('x', GetX);
    this.__defineGetter__('y', GetY);
    this.__defineGetter__('width', GetWidth);
    this.__defineGetter__('height', GetHeight);
    this.__defineGetter__('nativeSuggestions', GetAutocompleteResultsWrapper);
    this.setSuggestions = function(text) {
      SetSuggestions(text);
    };
    this.setAutocompleteText = function(text, behavior) {
      SetQuerySuggestion(text, behavior);
    };
    this.setRestrictedAutocompleteText = function(resultId, behavior) {
      SetQuerySuggestionFromAutocompleteResult(resultId, behavior);
    };
    this.setValue = function(text, type) {
      SetQuery(text, type);
    };
    this.setRestrictedValue = function(resultId, behavior) {
      SetQueryFromAutocompleteResult(resultId, behavior);
    };
    this.setNonNativeDropdownHeight = function(height) {
      SetPreviewHeight(height);
    };
    this.markDuplicateSuggestions = function(clientSuggestions) {
      return DedupeClientSuggestions(clientSuggestions);
    };
    this.navigateContentWindow = function(destination) {
      return NavigateContentWindow(destination);
    };
    this.onchange = null;
    this.onsubmit = null;
    this.oncancel = null;
    this.onresize = null;
    this.onautocompleteresults = null;
    this.onkeypress = null;
  };
}
