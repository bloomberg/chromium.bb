// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var load_util = {
  report: function(msg) {
    domAutomationController.setAutomationId(0);
    // The automation controller seems to choke on Objects, so turn them into
    // strings.
    domAutomationController.send(JSON.stringify(msg));
  },

  log: function(message) {
    load_util.report({type: "Log", message: message});
  },

  shutdown: function(message, passed) {
    load_util.report({type: "Shutdown", message: message, passed: passed});
  },

  embed: function(manifest_url) {
    var embed = document.createElement("embed");
    embed.src = manifest_url;
    embed.type = "application/x-nacl";
    return embed;
  },

  // Use the DOM to determine the absolute URL.
  absoluteURL: function(url) {
    var a = document.createElement("a");
    a.href = url;
    return a.href;
  },

  crossOriginEmbed: function(manifest_url) {
    manifest_url = load_util.absoluteURL(manifest_url);
    var cross_url = manifest_url.replace("127.0.0.1", "localhost");
    if (cross_url == manifest_url) {
      load_util.shutdown("Could not create a cross-origin URL.", false);
      throw "abort";
    }
    return load_util.embed(cross_url);
  },

  expectLoadFailure: function(embed, message) {
    embed.addEventListener("load", function(event) {
      load_util.log("Load succeeded when it should have failed.");
      load_util.shutdown("1 test failed.", false);
    }, true);

    embed.addEventListener("error", function(event) {
      if (embed.lastError != "NaCl module load failed: " + message) {
        load_util.log("Unexpected load error: " + embed.lastError);
        load_util.shutdown("1 test failed.", false);
      } else {
        load_util.shutdown("1 test passed.", true);
      }
    }, true);
  }
};
