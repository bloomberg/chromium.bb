// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var botRoot = "http://build.chromium.org/p/chromium";
var waterfallURL = botRoot + "/waterfall/bot_status.json?json=1";
var botList;
var checkinResults;
var bots;
var failures;

function updateBotList(text) {
  var results = (new RegExp('(.*)<\/body>', 'g')).exec(text);
  if (!results || results.index < 0) {
    console.log("Error: couldn't find bot JSON");
    console.log(text);
    return;
  }
  var data;
  try {
    // The build bot returns invalid JSON. Namely it uses single
    // quotes and includes commas in some invalid locations. We have to
    // run some regexps across the text to fix it up.
    var jsonString = results[1].replace(/'/g, '"').replace(/},]/g,'}]');
    data = JSON.parse(jsonString);
  } catch (e) {
    console.dir(e);
    console.log(text);
    return;
  }
  if (!data) {
    throw new Error("JSON parse fail: " + results[1]);
  }
  botList = data[0];
  checkinResults = data[1];

  failures = botList.filter(function(bot) {
    return (bot.color != "success");
  });
  displayFailures();
}

function displayFailures() {
  bots.innerText = "";

  if (failures.length == 0) {
    var anchor = document.createElement("a");
    anchor.addEventListener("click", showConsole);
    anchor.className = "open";
    anchor.innerText = "The tree is completely green.";
    bots.appendChild(anchor);
    bots.appendChild(document.createTextNode(" (no way!)"));
  } else {
    var anchor = document.createElement("a");
    anchor.addEventListener("click", showFailures);
    anchor.innerText = "failures:";
    var div = document.createElement("div");
    div.appendChild(anchor);
    bots.appendChild(div);

    failures.forEach(function(bot, i) {
      var div = document.createElement("div");
      div.className = "bot " + bot.color;
      div.addEventListener("click", function() {
        // Requires closure for each iteration to retain local value of |i|.
        return function() { showBot(i); }
      }());
      div.innerText = bot.title;
      bots.appendChild(div);
    });
  }
}

function showURL(url) {
  window.open(url);
  window.event.stopPropagation();
}

function showBot(botIndex) {
  var bot = failures[botIndex];
  var url = botRoot + "/waterfall/waterfall?builder=" + bot.name;
  showURL(url);
}

function showConsole() {
  var url = botRoot + "/waterfall/console";
  showURL(url);
}

function showTry() {
  var url = botRoot + "/try-server/waterfall";
  showURL(url);
}

function showFyi() {
  var url = botRoot + "/waterfall.fyi/console";
  showURL(url);
}

function showFailures() {
  var url = botRoot +
      "/waterfall/waterfall?show_events=true&failures_only=true";
  showURL(url);
}

function requestURL(url, callback) {
  console.log("requestURL: " + url);
  var xhr = new XMLHttpRequest();
  try {
    xhr.onreadystatechange = function(state) {
      if (xhr.readyState == 4) {
        if (xhr.status == 200) {
          var text = xhr.responseText;
          //console.log(text);
          callback(text);
        } else {
          bots.innerText = "Error.";
        }
      }
    }

    xhr.onerror = function(error) {
      console.log("xhr error: " + JSON.stringify(error));
      console.dir(error);
    }

    xhr.open("GET", url, true);
    xhr.send({});
  } catch(e) {
    console.log("exception: " + e);
  }
}

function toggle_size() {
  if (document.body.className == "big") {
    document.body.className = "small";
  } else {
    document.body.className = "big";
  }
}

document.addEventListener('DOMContentLoaded', function () {
  toggle_size();

  bots = document.getElementById("bots");

  // XHR from onload winds up blocking the load, so we put it in a setTimeout.
  window.setTimeout(requestURL, 0, waterfallURL, updateBotList);

  // Setup event handlers
  document.querySelector('#console').addEventListener('click', showConsole);
  document.querySelector('#try').addEventListener('click', showTry);
  document.querySelector('#fyi').addEventListener('click', showFyi);
});
