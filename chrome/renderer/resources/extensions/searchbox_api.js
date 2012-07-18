// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome;
if (!chrome)
  chrome = {};
if (!chrome.searchBox) {
  chrome.searchBox = new function() {
    native function GetValue();
    native function GetVerbatim();
    native function GetSelectionStart();
    native function GetSelectionEnd();
    native function GetX();
    native function GetY();
    native function GetWidth();
    native function GetHeight();
    native function SetSuggestions();
    this.__defineGetter__('value', GetValue);
    this.__defineGetter__('verbatim', GetVerbatim);
    this.__defineGetter__('selectionStart', GetSelectionStart);
    this.__defineGetter__('selectionEnd', GetSelectionEnd);
    this.__defineGetter__('x', GetX);
    this.__defineGetter__('y', GetY);
    this.__defineGetter__('width', GetWidth);
    this.__defineGetter__('height', GetHeight);
    this.setSuggestions = function(text) {
      SetSuggestions(text);
    };
    this.onchange = null;
    this.onsubmit = null;
    this.oncancel = null;
    this.onresize = null;
  };
}
