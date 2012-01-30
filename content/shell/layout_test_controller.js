// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var layoutTestController = layoutTestController || {};

(function() {
  native function NotifyDone();
  native function SetDumpAsText();
  native function SetDumpChildFramesAsText();
  native function SetWaitUntilDone();

  layoutTestController = new function() {
    this.notifyDone = NotifyDone;
    this.dumpAsText = SetDumpAsText;
    this.dumpChildFramesAsText = SetDumpChildFramesAsText;
    this.waitUntilDone = SetWaitUntilDone;
  }();
})();
