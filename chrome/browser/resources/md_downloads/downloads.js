// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.addEventListener('load', function() {
  downloads.Manager.onLoad();

  if (!cr.isChromeOS) {
    // TODO(dbeam): this is copied out of roboto.css. I excluded roboto.css from
    // vulcanize.py and tried to re-use the "src:" rule from the file via
    // querySelector(href$=roboto.css).sheet.rules[2].style.src, but alas .rules
    // can't be accessed because the origins don't match (chrome://resources vs
    // chrome://downloads). So it's duplicated.
    new FontFace('Roboto', "local('Roboto Bold'), local('Roboto-Bold'), " +
        "url(chrome://resources/roboto/roboto-bold.woff2) format('woff2')",
        {weight: 'bold'}).load();
  }
});
