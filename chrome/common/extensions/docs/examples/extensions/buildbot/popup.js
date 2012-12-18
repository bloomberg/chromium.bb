// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var lkgrURL = 'http://chromium-status.appspot.com/lkgr';

function requestURL(url, callback, opt_responseType) {
  var xhr = new XMLHttpRequest();
  xhr.responseType = opt_responseType || "document";

  xhr.onreadystatechange = function(state) {
    if (xhr.readyState == 4) {
      if (xhr.status == 200) {
        callback(xhr.responseType == "document" ?
                 xhr.responseXML : xhr.responseText);
      } else {
        chrome.browserAction.setBadgeText({text:"?"});
        chrome.browserAction.setBadgeBackgroundColor({color:[0,0,255,255]});
      }
    }
  };

  xhr.onerror = function(error) {
    console.log("xhr error:", error);
  };

  xhr.open("GET", url, true);
  xhr.send();
}

function updateLKGR(lkgr) {
  var link = document.getElementById('link_lkgr');
  link.innerHTML = 'LKGR (' + lkgr + ')';
}

function addBotStatusRow(table, bot) {
  var baseURL = "http://chromium-build.appspot.com/p/chromium" +
    (bot.id != "" ? "." + bot.id : "");
  var consoleURL = baseURL + "/console";
  var statusURL = baseURL + "/horizontal_one_box_per_builder";

  var row = table.insertRow(-1);
  var label = row.insertCell(-1);
  label.className = "botlabel";
  label.innerHTML = "<a href=\"" + consoleURL + "\" target=\"_blank\" " +
    "id=\"link_" + bot.id + "\">" + bot.label + "</a>";

  var status = row.insertCell(-1);
  status.className = "botstatus";
  status.innerHTML = "<iframe class=\"statusiframe\" scrolling=\"no\" src=\"" +
    statusURL + "\"></iframe>";
}

function fillBotStatusTable() {
  var closer_bots = [
    {id: "", label: "Chromium"},
    {id: "win", label: "Win"},
    {id: "mac", label: "Mac"},
    {id: "linux", label: "Linux"},
    {id: "chromiumos", label: "ChromiumOS"},
    {id: "chrome", label: "Official"},
    {id: "memory", label: "Memory"}
  ];

  var other_bots = [
    {id: "lkgr", label: "LKGR"},
    {id: "perf", label: "Perf"},
    {id: "memory.fyi", label: "Memory FYI"},
    {id: "gpu", label: "GPU"},
    {id: "gpu.fyi", label: "GPU FYI"}
  ];

  var table = document.getElementById("bot-status-table");
  for (var i = 0; i < closer_bots.length; i++) {
    addBotStatusRow(table, closer_bots[i]);
  }

  table.insertRow(-1).insertCell().className = "spacer";

  for (i = 0; i < other_bots.length; i++) {
    addBotStatusRow(table, other_bots[i]);
  }
}

function main() {
  requestURL(lkgrURL, updateLKGR, "text");
  fillBotStatusTable();
}

main();
