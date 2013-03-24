// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome;
if (!chrome)
  chrome = {};

if (!chrome.embeddedSearch) {
  chrome.embeddedSearch = new function() {

    // =========================================================================
    //                            Private functions
    // =========================================================================
    native function GetFont();
    // DEPRECATED. TODO(sreeram): Remove once google.com no longer uses this.
    native function NavigateContentWindow();

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

    var safeObjects = {};

    // Returns the |restrictedText| wrapped in a ShadowDOM.
    function SafeWrap(restrictedText, height, opt_width, opt_fontSize,
        opt_direction) {
      var node = document.createElement('div');
      var nodeShadow = safeObjects.createShadowRoot.apply(node);
      nodeShadow.applyAuthorStyles = true;
      nodeShadow.innerHTML =
          '<div style="' +
              (opt_width ? 'width: ' + opt_width + 'px !important;' : '') +
              'height: ' + height + 'px !important;' +
              'font-family: \'' + GetFont() + '\', \'Arial\' !important;' +
              (opt_fontSize ?
                  'font-size: ' + opt_fontSize + 'px !important;' : '') +
              'overflow: hidden !important;' +
              'text-overflow: ellipsis !important;' +
              'white-space: nowrap !important"' +
              (opt_direction ? ' dir="' + opt_direction + '"' : '') +
              '>' +
            restrictedText +
          '</div>';
      safeObjects.defineProperty(node, 'webkitShadowRoot', { value: null });
      return node;
    }

    chrome.embeddedSearchOnWindowReady = function() {
      // |embeddedSearchOnWindowReady| is used for initializing window context
      // and should be called only once per context.
      safeObjects.createShadowRoot = Element.prototype.webkitCreateShadowRoot;
      safeObjects.defineProperty = Object.defineProperty;
      delete window.chrome.embeddedSearchOnWindowReady;
    };

    // =========================================================================
    //                           Exported functions
    // =========================================================================
    // DEPRECATED. TODO(sreeram): Remove once google.com no longer uses this.
    this.navigateContentWindow = function(destination, disposition) {
      return NavigateContentWindow(destination, disposition);
    };

    this.searchBox = new function() {

      // =======================================================================
      //                                  Constants
      // =======================================================================
      var MAX_CLIENT_SUGGESTIONS_TO_DEDUPE = 6;
      var MAX_ALLOWED_DEDUPE_ATTEMPTS = 5;

      var HTTP_REGEX = /^https?:\/\//;

      var WWW_REGEX = /^www\./;

      // =======================================================================
      //                            Private functions
      // =======================================================================
      native function GetQuery();
      native function GetVerbatim();
      native function GetSelectionStart();
      native function GetSelectionEnd();
      native function GetStartMargin();
      native function GetRightToLeft();
      native function GetAutocompleteResults();
      native function GetDisplayInstantResults();
      native function GetFontSize();
      native function IsKeyCaptureEnabled();
      native function SetQuery();
      native function SetQueryFromAutocompleteResult();
      native function SetSuggestion();
      native function SetSuggestionFromAutocompleteResult();
      native function SetSuggestions();
      native function ShowOverlay();
      native function FocusOmnibox();
      native function StartCapturingKeyStrokes();
      native function StopCapturingKeyStrokes();
      native function NavigateSearchBox();
      native function ShowBars();
      native function HideBars();

      function SafeWrapSuggestion(restrictedText) {
        return SafeWrap(restrictedText, 22);
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
          // TODO(dcblack): Rename these titleElement, urlElement, and
          // combinedElement for optimal correctness.
          if (title) {
            result.titleNode = SafeWrapSuggestion(title);
            combinedHtml += '<span class=chrome_separator> &ndash; </span>' +
                '<span class=chrome_title>' + title + '</span>';
          }
          result.urlNode = SafeWrapSuggestion(url);
          result.combinedNode = SafeWrapSuggestion(combinedHtml);
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
            // TODO(dcblack): This check is insufficient.  We should have a
            // check such that it's only callable once per onnativesuggestions,
            // not once per prefix.  Also, there is a timing problem where if
            // the user types quickly then the client will (correctly) attempt
            // to render stale results, and end up calling dedupe multiple times
            // when getValue shows the same prefix.  A better solution would be
            // to have the client send up rid ranges to dedupe against and have
            // the binary keep around all the old suggestions ever given to this
            // overlay.  I suspect such an approach would clean up this code
            // quite a bit.
            return false;
          }
        } else {
          lastPrefixQueriedForDuplicates = userInput;
          numDedupeAttempts = 1;
        }

        var autocompleteResults = DedupeAutocompleteResults(
            GetAutocompleteResults());
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

      // =======================================================================
      //                           Exported functions
      // =======================================================================
      this.__defineGetter__('value', GetQuery);
      this.__defineGetter__('verbatim', GetVerbatim);
      this.__defineGetter__('selectionStart', GetSelectionStart);
      this.__defineGetter__('selectionEnd', GetSelectionEnd);
      this.__defineGetter__('startMargin', GetStartMargin);
      this.__defineGetter__('rtl', GetRightToLeft);
      this.__defineGetter__('nativeSuggestions', GetAutocompleteResultsWrapper);
      this.__defineGetter__('isKeyCaptureEnabled', IsKeyCaptureEnabled);
      this.__defineGetter__('displayInstantResults', GetDisplayInstantResults);
      this.__defineGetter__('font', GetFont);
      this.__defineGetter__('fontSize', GetFontSize);

      this.setSuggestions = function(text) {
        SetSuggestions(text);
      };
      this.setAutocompleteText = function(text, behavior) {
        SetSuggestion(text, behavior);
      };
      this.setRestrictedAutocompleteText = function(autocompleteResultId) {
        SetSuggestionFromAutocompleteResult(autocompleteResultId);
      };
      this.setValue = function(text, type) {
        SetQuery(text, type);
      };
      // Must access nativeSuggestions before calling setRestrictedValue.
      this.setRestrictedValue = function(autocompleteResultId) {
        SetQueryFromAutocompleteResult(autocompleteResultId);
      };
      this.showOverlay = function(height) {
        ShowOverlay(height);
      };
      this.markDuplicateSuggestions = function(clientSuggestions) {
        return DedupeClientSuggestions(clientSuggestions);
      };
      this.focus = function() {
        FocusOmnibox();
      };
      this.startCapturingKeyStrokes = function() {
        StartCapturingKeyStrokes();
      };
      this.stopCapturingKeyStrokes = function() {
        StopCapturingKeyStrokes();
      };
      this.navigateContentWindow = function(destination, disposition) {
        NavigateSearchBox(destination, disposition);
      }
      this.showBars = function() {
        ShowBars();
      };
      this.hideBars = function() {
        HideBars();
      };
      this.onchange = null;
      this.onsubmit = null;
      this.oncancel = null;
      this.onresize = null;
      this.onkeypress = null;
      this.onkeycapturechange = null;
      this.onmarginchange = null;
      this.onnativesuggestions = null;
      this.onbarshidden = null;

      // DEPRECATED. These methods are from the legacy searchbox API.
      // TODO(jered): Delete these.
      native function GetX();
      native function GetY();
      native function GetWidth();
      native function GetHeight();
      this.__defineGetter__('x', GetX);
      this.__defineGetter__('y', GetY);
      this.__defineGetter__('width', GetWidth);
      this.__defineGetter__('height', GetHeight);
    };

    this.newTabPage = new function() {

      // =======================================================================
      //                            Private functions
      // =======================================================================
      native function GetMostVisitedItems();
      native function GetThemeBackgroundInfo();
      native function DeleteMostVisitedItem();
      native function UndoAllMostVisitedDeletions();
      native function UndoMostVisitedDeletion();
      native function NavigateNewTabPage();

      function SafeWrapMostVisited(restrictedText, width, opt_direction) {
        return SafeWrap(restrictedText, 18, width, 11, opt_direction);
      }

      function GetMostVisitedItemsWrapper() {
        var mostVisitedItems = GetMostVisitedItems();
        for (var i = 0, item; item = mostVisitedItems[i]; ++i) {
          var title = escapeHTML(item.title);
          var domain = escapeHTML(item.domain);
          item.titleElement = SafeWrapMostVisited(title, 140, item.direction);
          item.domainElement = SafeWrapMostVisited(domain, 123);
          delete item.title;
          delete item.domain;
          delete item.direction;
        }
        return mostVisitedItems;
      }

      // =======================================================================
      //                           Exported functions
      // =======================================================================
      this.__defineGetter__('mostVisited', GetMostVisitedItemsWrapper);
      this.__defineGetter__('themeBackgroundInfo', GetThemeBackgroundInfo);

      this.deleteMostVisitedItem = function(restrictedId) {
        DeleteMostVisitedItem(restrictedId);
      };
      this.undoAllMostVisitedDeletions = function() {
        UndoAllMostVisitedDeletions();
      };
      this.undoMostVisitedDeletion = function(restrictedId) {
        UndoMostVisitedDeletion(restrictedId);
      };
      this.navigateContentWindow = function(destination, disposition) {
        NavigateNewTabPage(destination, disposition);
      }

      this.onmostvisitedchange = null;
      this.onthemechange = null;
    };

    // Export legacy searchbox API.
    // TODO: Remove this when Instant Extended is fully launched.
    chrome.searchBox = this.searchBox;
  };
}
