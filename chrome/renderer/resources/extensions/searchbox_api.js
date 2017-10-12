// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome;
if (!chrome)
  chrome = {};

if (!chrome.embeddedSearch) {
  chrome.embeddedSearch = new function() {
    this.searchBox = new function() {

      // =======================================================================
      //                            Private functions
      // =======================================================================
      native function GetRightToLeft();
      native function IsFocused();
      native function IsKeyCaptureEnabled();
      native function Paste();
      native function StartCapturingKeyStrokes();
      native function StopCapturingKeyStrokes();

      // =======================================================================
      //                           Exported functions
      // =======================================================================
      Object.defineProperty(this, 'rtl', { get: GetRightToLeft });
      this.__defineGetter__('isFocused', IsFocused);
      this.__defineGetter__('isKeyCaptureEnabled', IsKeyCaptureEnabled);

      this.paste = function(value) {
        Paste(value);
      };

      this.startCapturingKeyStrokes = function() {
        StartCapturingKeyStrokes();
      };

      this.stopCapturingKeyStrokes = function() {
        StopCapturingKeyStrokes();
      };

      this.onfocuschange = null;
      this.onkeycapturechange = null;
    };

    this.newTabPage = new function() {

      // =======================================================================
      //                            Private functions
      // =======================================================================
      native function CheckIsUserSignedInToChromeAs();
      native function CheckIsUserSyncingHistory();
      native function DeleteMostVisitedItem();
      native function GetMostVisitedItemData();
      native function GetMostVisitedItems();
      native function GetThemeBackgroundInfo();
      native function IsInputInProgress();
      native function LogEvent();
      native function LogMostVisitedImpression();
      native function LogMostVisitedNavigation();
      native function UndoAllMostVisitedDeletions();
      native function UndoMostVisitedDeletion();

      function GetMostVisitedItemsWrapper() {
        var mostVisitedItems = GetMostVisitedItems();
        for (var i = 0, item; item = mostVisitedItems[i]; ++i) {
          item.faviconUrl = GenerateFaviconURL(item.renderViewId, item.rid);

          // These properties are private data and should not be returned to
          // the page. They are only accessible via getMostVisitedItemData().
          delete item.url;
          delete item.title;
          delete item.domain;
          delete item.direction;
          delete item.renderViewId;
        }
        return mostVisitedItems;
      }

      function GenerateFaviconURL(renderViewId, rid) {
        return "chrome-search://favicon/size/16@" +
            window.devicePixelRatio + "x/" +
            renderViewId + "/" + rid;
      }

      // =======================================================================
      //                           Exported functions
      // =======================================================================
      this.__defineGetter__('isInputInProgress', IsInputInProgress);
      this.__defineGetter__('mostVisited', GetMostVisitedItemsWrapper);
      this.__defineGetter__('themeBackgroundInfo', GetThemeBackgroundInfo);

      this.checkIsUserSignedIntoChromeAs = function(identity) {
        CheckIsUserSignedInToChromeAs(identity);
      };

      this.checkIsUserSyncingHistory = function() {
        CheckIsUserSyncingHistory();
      };

      this.deleteMostVisitedItem = function(restrictedId) {
        DeleteMostVisitedItem(restrictedId);
      };

      // This method is restricted to chrome-search://most-visited pages by
      // checking the invoking context's origin in searchbox_extension.cc.
      this.getMostVisitedItemData = function(restrictedId) {
        return GetMostVisitedItemData(restrictedId);
      };

      // This method is restricted to chrome-search://most-visited and
      // chrome-search://local-ntp pages by checking the invoking context's
      // origin in searchbox_extension.cc.
      this.logEvent = function(histogram_name) {
        LogEvent(histogram_name);
      };

      // This method is restricted to chrome-search://most-visited pages by
      // checking the invoking context's origin in searchbox_extension.cc.
      this.logMostVisitedImpression = function(position, tileTitleSource,
                                               tileSource, tileType,
                                               dataGenerationTime) {
        LogMostVisitedImpression(position, tileTitleSource, tileSource,
                                 tileType, dataGenerationTime);
      };

      // This method is restricted to chrome-search://most-visited pages by
      // checking the invoking context's origin in searchbox_extension.cc.
      this.logMostVisitedNavigation = function(position, tileTitleSource,
                                               tileSource, tileType,
                                               dataGenerationTime) {
        LogMostVisitedNavigation(position, tileTitleSource, tileSource,
                                 tileType, dataGenerationTime);
      };

      this.undoAllMostVisitedDeletions = function() {
        UndoAllMostVisitedDeletions();
      };

      this.undoMostVisitedDeletion = function(restrictedId) {
        UndoMostVisitedDeletion(restrictedId);
      };

      this.onsignedincheckdone = null;
      this.onhistorysynccheckdone = null;
      this.oninputcancel = null;
      this.oninputstart = null;
      this.onmostvisitedchange = null;
      this.onthemechange = null;
    };

    // TODO(jered): Remove when google no longer expects this object.
    chrome.searchBox = this.searchBox;
  };
}
