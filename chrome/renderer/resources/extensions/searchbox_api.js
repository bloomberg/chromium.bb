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
      native function GetFont();
      native function GetFontSize();
      native function GetMostVisitedItemData();
      native function GetQuery();
      native function GetRightToLeft();
      native function GetStartMargin();
      native function IsFocused();
      native function IsKeyCaptureEnabled();
      native function Paste();
      native function SetVoiceSearchSupported();
      native function StartCapturingKeyStrokes();
      native function StopCapturingKeyStrokes();

      // =======================================================================
      //                           Exported functions
      // =======================================================================
      this.__defineGetter__('font', GetFont);
      this.__defineGetter__('fontSize', GetFontSize);
      this.__defineGetter__('isFocused', IsFocused);
      this.__defineGetter__('isKeyCaptureEnabled', IsKeyCaptureEnabled);
      this.__defineGetter__('rtl', GetRightToLeft);
      this.__defineGetter__('startMargin', GetStartMargin);
      this.__defineGetter__('value', GetQuery);

      // This method is restricted to chrome-search://most-visited pages by
      // checking the invoking context's origin in searchbox_extension.cc.
      this.getMostVisitedItemData = function(restrictedId) {
        return GetMostVisitedItemData(restrictedId);
      };

      this.paste = function(value) {
        Paste(value);
      };

      this.setVoiceSearchSupported = function(supported) {
        SetVoiceSearchSupported(supported);
      };

      this.startCapturingKeyStrokes = function() {
        StartCapturingKeyStrokes();
      };

      this.stopCapturingKeyStrokes = function() {
        StopCapturingKeyStrokes();
      };

      this.onfocuschange = null;
      this.onkeycapturechange = null;
      this.onmarginchange = null;
      this.onsubmit = null;
      this.ontogglevoicesearch = null;

      // TODO(jered): Remove these when google no longer requires them.
      this.displayInstantResults = false;
      this.setRestrictedValue = function() {};
    };

    this.newTabPage = new function() {

      // =======================================================================
      //                            Private functions
      // =======================================================================
      native function DeleteMostVisitedItem();
      native function GetAppLauncherEnabled();
      native function GetMostVisitedItems();
      native function GetThemeBackgroundInfo();
      native function IsInputInProgress();
      native function LogEvent();
      native function NavigateContentWindow();
      native function UndoAllMostVisitedDeletions();
      native function UndoMostVisitedDeletion();

      function GetMostVisitedItemsWrapper() {
        var mostVisitedItems = GetMostVisitedItems();
        for (var i = 0, item; item = mostVisitedItems[i]; ++i) {
          item.faviconUrl = GenerateFaviconURL(item.renderViewId, item.rid);
          // These properties are private data and should not be returned to
          // the page. They are only accessible via getMostVisitedItemData().
          item.url = null;
          item.title = null;
          item.domain = null;
          item.direction = null;
          item.renderViewId = null;
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
      this.__defineGetter__('appLauncherEnabled', GetAppLauncherEnabled);
      this.__defineGetter__('isInputInProgress', IsInputInProgress);
      this.__defineGetter__('mostVisited', GetMostVisitedItemsWrapper);
      this.__defineGetter__('themeBackgroundInfo', GetThemeBackgroundInfo);

      this.deleteMostVisitedItem = function(restrictedId) {
        DeleteMostVisitedItem(restrictedId);
      };

      // This method is restricted to chrome-search://most-visited pages by
      // checking the invoking context's origin in searchbox_extension.cc.
      this.logEvent = function(histogram_name) {
        LogEvent(histogram_name);
      };

      this.navigateContentWindow = function(destination, disposition) {
        NavigateContentWindow(destination, disposition);
      };

      this.undoAllMostVisitedDeletions = function() {
        UndoAllMostVisitedDeletions();
      };

      this.undoMostVisitedDeletion = function(restrictedId) {
        UndoMostVisitedDeletion(restrictedId);
      };

      this.oninputcancel = null;
      this.oninputstart = null;
      this.onmostvisitedchange = null;
      this.onthemechange = null;
    };

    // TODO(jered): Remove when google no longer expects this object.
    chrome.searchBox = this.searchBox;
  };
}
