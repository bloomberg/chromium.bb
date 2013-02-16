// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const */ var ItemList = apps_dev_tool.ItemsList;

document.addEventListener('DOMContentLoaded', function() {
  apps_dev_tool.AppsDevTool.initStrings();
  $('search').addEventListener('input', ItemList.onSearchInput);
  ItemList.loadItemsInfo();
});

var appsDevTool = new apps_dev_tool.AppsDevTool();

window.addEventListener('load', function(e) {
  appsDevTool.initialize();
});

chrome.management.onInstalled.addListener(ItemList.loadItemsInfo);
chrome.management.onUninstalled.addListener(ItemList.loadItemsInfo);
