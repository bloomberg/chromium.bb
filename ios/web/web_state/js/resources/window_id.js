// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adheres to closure-compiler conventions in order to enable
// compilation with ADVANCED_OPTIMIZATIONS. See http://goo.gl/FwOgy

// Script to set windowId.


// Namespace for module, used as presence beacon for injection checks.
__gCrWeb['windowIdObject'] = {};

(function() {
  // CRWJSWindowIdManager replaces $(WINDOW_ID) with appropriate string upon
  // injection.
  __gCrWeb['windowId'] = '$(WINDOW_ID)';

  // Wrap queues flushing in setTimeout to avoid reentrant calls.
  // In some circumstances setTimeout does not work on iOS8 if set from
  // injected script. There is an assumption that it's happen when the script
  // has been injected too early. Do not place anything important to delayed
  // function body, since there is no guarantee that it will ever be executed.
  // TODO(eugenebut): Find out why setTimeout does not work (crbug.com/402682).
  window.setTimeout(function() {
    // Send messages queued since message.js injection.
    if (__gCrWeb.message) {
      __gCrWeb.message.invokeQueues();
    }
  }, 0);
}());
