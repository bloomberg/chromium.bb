// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var target_dir = '/extensions/platform_apps/url_handlers/common/';
  var link = document.getElementById('link');
  var mismatching_link = document.getElementById('mismatching_link');
  var prerender_link = document.getElementById('prerender_link');
  var form  = document.getElementById('form');

  if (link || mismatching_link || prerender_link || form) {
    var clickEvent = document.createEvent('MouseEvents');
    clickEvent.initMouseEvent('click', true, true, window,
                              0, 0, 0, 0, 0, false, false,
                              false, false, 0, null);
    if (link) {
      console.log("Clicking a matching link");
      link.href = target_dir + 'target.html';
      // This click should open the handler app (pre-installed in the browser by
      // the CPP test before launching this) with link.href.
      link.dispatchEvent(clickEvent);
    }

    if (mismatching_link) {
      console.log("Clicking a mismatching link");
      mismatching_link.href = target_dir + 'mismatching_target.html';
      // This click should NOT open the handler app, because the URL does not
      // match the url_handlers of the app. It should open the link in a new
      // tab instead.
      mismatching_link.dispatchEvent(clickEvent);
    }

    if (prerender_link) {
      console.log("Prerendering a link");
      prerender_link.href = target_dir + "target.html";
      var prerender = document.createElement("link");
      prerender.rel = "prerender";
      prerender.href = prerender_link.href;
      prerender.addEventListener("webkitprerenderstop", function() {
        console.log("Prerender aborted. Clicking link");
        prerender_link.dispatchEvent(clickEvent);
      });
      document.body.appendChild(prerender);
    }

    if (form) {
      console.log("Submitting a form");
      form.action = target_dir + 'target.html';
      var submit_button = document.getElementById("submit_button");
      // This click should NOT open the handler app, because form submissions
      // using POST should not be intercepted.
      submit_button.dispatchEvent(clickEvent);
    }
  } else {
    console.log("Calling window.open()");
    window.open(target_dir + 'target.html');
  }
})();
