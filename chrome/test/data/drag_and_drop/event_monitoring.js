// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Reports an event to DragAndDropBrowserTest and  DOMDragEventWaiter */
window.reportDragAndDropEvent = function(ev) {
  function safe(f) {
    try {
      return f();
    } catch(err) {
      return "exception: " + err.message;
    }
  }

  console.log("got event: " + ev.type);

  if (window.domAutomationController) {
    window.domAutomationController.setAutomationId(0);
    window.domAutomationController.send({
      client_position: safe(function() {
        return "(" + ev.clientX + ", " + ev.clientY + ")";
      }),
      drop_effect: safe(function() { return ev.dataTransfer.dropEffect; }),
      effect_allowed: safe(function() {
        return ev.dataTransfer.effectAllowed;
      }),
      event_type: ev.type,
      file_names: safe(function() {
        return Array
            .from(ev.dataTransfer.files)
            .map(function(file) { return file.name; })
            .sort().join();
      }),
      mime_types: safe(function() {
        return Array.from(ev.dataTransfer.types).sort().join();
      }),
      page_position: safe(function() {
        return "(" + ev.pageX + ", " + ev.pageY + ")";
      }),
      window_name: window.name
    });
  }
}
