// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function create(manifest_url) {
  var embed = load_util.embed(manifest_url);

  embed.addEventListener("load", function(evt) {
    load_util.shutdown("1 test passed.", true);
  }, true);

  embed.addEventListener("error", function(evt) {
    load_util.log("Load error: " + embed.lastError);
    load_util.shutdown("1 test failed.", false);
  }, true);

  document.body.appendChild(embed);
}

function documentLoaded() {
  create('extension_validation_cache.nmf');
}

document.addEventListener('DOMContentLoaded', documentLoaded);
