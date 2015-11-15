// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
chrome.runtime.onInstalled.addListener(
  function(a){
    console.log("chrome.runtime.onInstalled details: "+JSON.stringify(a));
    console.log("typeof chrome.dataReductionProxy: "+
                typeof chrome.dataReductionProxy);
    if("undefined"!=typeof chrome.dataReductionProxy)
      if("install"==a.reason)chrome.dataReductionProxy.spdyProxyEnabled.set(
                               {value:!0}),
      "clearDataSavings"in chrome.dataReductionProxy&&
        chrome.dataReductionProxy.clearDataSavings(),
      showEnabledIcon();
      else if("update"==a.reason||"chrome_update"==a.reason)
        showEnabledIconIfProxyOn(),
        chrome.storage.local.get("user_enabled_proxy",
        function(a){"user_enabled_proxy"in a&&(
          chrome.dataReductionProxy.spdyProxyEnabled.set({value:!0}),
          showEnabledIcon(),
          chrome.storage.local.remove("user_enabled_proxy"))})});
      chrome.runtime.onStartup.addListener(function(){
      showEnabledIconIfProxyOn()});chrome.tabs.onCreated.addListener(
      function(a){setTimeout(function(){a.incognito&&
      chrome.browserAction.setIcon({tabId:a.id,path:{
        19:"./images/proxy-disabled19.png",
        38:"./images/proxy-disabled38.png"}})},500)});
function showEnabledIconIfProxyOn(){
  "undefined"!=typeof chrome.dataReductionProxy&&(
    console.log("Calling spdyProxyEnabled.get"),
    chrome.dataReductionProxy.spdyProxyEnabled.get({},
      function(a){
        console.log("chrome.dataReductionProxy.spdyProxyEnabled.get: "
                    +JSON.stringify(a));
        (a="value"in a&&a.value)&&showEnabledIcon()
      })
  )
};
function showEnabledIcon(){
  console.log("Calling chrome.browserAction.setIcon");
  chrome.browserAction.setIcon({path:{19:"./images/proxy-enabled19.webp",
                                      38:"./images/proxy-enabled38.webp"}})
};
