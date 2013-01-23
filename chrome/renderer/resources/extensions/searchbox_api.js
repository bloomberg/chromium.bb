// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome;
if (!chrome)
  chrome = {};
if (!chrome.searchBox) {
  chrome.searchBox = new function() {
    var safeObjects = {};
    chrome.searchBoxOnWindowReady = function() {
      // |searchBoxOnWindowReady| is used for initializing window context and
      // should be called only once per context.
      safeObjects.createShadowRoot = Element.prototype.webkitCreateShadowRoot;
      safeObjects.defineProperty = Object.defineProperty;
      delete window.chrome.searchBoxOnWindowReady;
    };

    // =========================================================================
    //                                  Constants
    // =========================================================================
    var MAX_CLIENT_SUGGESTIONS_TO_DEDUPE = 6;
    var MAX_ALLOWED_DEDUPE_ATTEMPTS = 5;

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
    native function GetStartMargin();
    native function GetEndMargin();
    native function GetRightToLeft();
    native function GetAutocompleteResults();
    native function GetContext();
    native function GetDisplayInstantResults();
    native function GetFont();
    native function GetFontSize();
    native function GetThemeBackgroundInfo();
    native function GetThemeAreaHeight();
    native function IsKeyCaptureEnabled();
    native function NavigateContentWindow();
    native function SetSuggestions();
    native function SetQuerySuggestion();
    native function SetQuerySuggestionFromAutocompleteResult();
    native function SetQuery();
    native function SetQueryFromAutocompleteResult();
    native function Show();
    native function StartCapturingKeyStrokes();
    native function StopCapturingKeyStrokes();

    function escapeHTML(text) {
      return text.replace(/[<>&"']/g, function(match) {
        switch (match) {
          case '<': return '&lt;';
          case '>': return '&gt;';
          case '&': return '&amp;';
          case '"': return '&quot;';
          case "'": return '&apos;';
        }
      });
    }

    // Returns the |restrictedText| wrapped in a ShadowDOM.
    function SafeWrap(restrictedText) {
      var node = document.createElement('div');
      var nodeShadow = safeObjects.createShadowRoot.apply(node);
      nodeShadow.applyAuthorStyles = true;
      nodeShadow.innerHTML =
          '<div style="' +
              'width: ' + (window.innerWidth - 155) + 'px !important;' +
              'height: 22px !important;' +
              'font-family: \'' + GetFont() + '\', \'Arial\' !important;' +
              'overflow: hidden !important;' +
              'text-overflow: ellipsis !important;' +
              'white-space: nowrap !important">' +
            restrictedText +
          '</div>';
      safeObjects.defineProperty(node, 'webkitShadowRoot', { value: null });
      return node;
    }

    // Wraps the AutocompleteResult query and URL into ShadowDOM nodes so that
    // the JS cannot access them and deletes the raw values.
    function GetAutocompleteResultsWrapper() {
      var autocompleteResults = DedupeAutocompleteResults(
          GetAutocompleteResults());
      var userInput = GetQuery();
      for (var i = 0, result; result = autocompleteResults[i]; ++i) {
        var title = escapeHTML(result.contents);
        var url = escapeHTML(CleanUrl(result.destination_url, userInput));
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

    // TODO(dcblack): Do this in C++ instead of JS.
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

    // TODO(dcblack): Do this in C++ instead of JS.
    function CanonicalizeUrl(url) {
      return url.replace(HTTP_REGEX, '').replace(WWW_REGEX, '');
    }

    // Removes duplicates from AutocompleteResults.
    // TODO(dcblack): Do this in C++ instead of JS.
    function DedupeAutocompleteResults(autocompleteResults) {
      var urlToResultMap = {};
      for (var i = 0, result; result = autocompleteResults[i]; ++i) {
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
    var numDedupeAttempts = 0;

    function DedupeClientSuggestions(clientSuggestions) {
      var userInput = GetQuery();
      if (userInput == lastPrefixQueriedForDuplicates) {
        numDedupeAttempts += 1;
        if (numDedupeAttempts > MAX_ALLOWED_DEDUPE_ATTEMPTS) {
          // Request blocked for privacy reasons.
          // TODO(dcblack): This check is insufficient.  We should have a check
          // such that it's only callable once per onnativesuggestions, not
          // once per prefix.  Also, there is a timing problem where if the user
          // types quickly then the client will (correctly) attempt to render
          // stale results, and end up calling dedupe multiple times when
          // getValue shows the same prefix.  A better solution would be to have
          // the client send up rid ranges to dedupe against and have the
          // binary keep around all the old suggestions ever given to this
          // overlay.  I suspect such an approach would clean up this code quite
          // a bit.
          return false;
        }
      } else {
        lastPrefixQueriedForDuplicates = userInput;
        numDedupeAttempts = 1;
      }

      var autocompleteResults = GetAutocompleteResults();
      var nativeUrls = {};
      for (var i = 0, result; result = autocompleteResults[i]; ++i) {
        var nativeUrl = CanonicalizeUrl(result.destination_url);
        nativeUrls[nativeUrl] = result.rid;
      }
      for (var i = 0; clientSuggestions[i] &&
           i < MAX_CLIENT_SUGGESTIONS_TO_DEDUPE; ++i) {
        var result = clientSuggestions[i];
        if (result.url) {
          var clientUrl = CanonicalizeUrl(result.url);
          if (clientUrl in nativeUrls) {
            result.duplicateOf = nativeUrls[clientUrl];
          }
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
    this.__defineGetter__('startMargin', GetStartMargin);
    this.__defineGetter__('endMargin', GetEndMargin);
    this.__defineGetter__('rtl', GetRightToLeft);
    this.__defineGetter__('nativeSuggestions', GetAutocompleteResultsWrapper);
    this.__defineGetter__('isKeyCaptureEnabled', IsKeyCaptureEnabled);
    this.__defineGetter__('context', GetContext);
    this.__defineGetter__('displayInstantResults', GetDisplayInstantResults);
    this.__defineGetter__('themeBackgroundInfo', GetThemeBackgroundInfo);
    this.__defineGetter__('themeAreaHeight', GetThemeAreaHeight);
    this.__defineGetter__('font', GetFont);
    this.__defineGetter__('fontSize', GetFontSize);
    this.setSuggestions = function(text) {
      SetSuggestions(text);
    };
    this.setAutocompleteText = function(text, behavior) {
      SetQuerySuggestion(text, behavior);
    };
    this.setRestrictedAutocompleteText = function(resultId) {
      SetQuerySuggestionFromAutocompleteResult(resultId);
    };
    this.setValue = function(text, type) {
      SetQuery(text, type);
    };
    this.setRestrictedValue = function(resultId) {
      SetQueryFromAutocompleteResult(resultId);
    };
    this.show = function(reason, height) {
      Show(reason, height);
    };
    this.markDuplicateSuggestions = function(clientSuggestions) {
      return DedupeClientSuggestions(clientSuggestions);
    };
    this.navigateContentWindow = function(destination) {
      return NavigateContentWindow(destination);
    };
    this.startCapturingKeyStrokes = function() {
      StartCapturingKeyStrokes();
    };
    this.stopCapturingKeyStrokes = function() {
      StopCapturingKeyStrokes();
    };
    this.onchange = null;
    this.onsubmit = null;
    this.oncancel = null;
    this.onresize = null;
    this.onautocompleteresults = null;
    this.onkeypress = null;
    this.onkeycapturechange = null;
    this.oncontextchange = null;
    this.onmarginchange = null;
  };
}
