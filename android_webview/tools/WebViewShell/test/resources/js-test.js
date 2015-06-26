// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

debug = function debug(msg)
{
  console.log(msg);
};

description = function description(msg, quiet)
{
  console.log(msg);
};

finishJSTest = function finishJSTest() {
  console.log("TEST FINISHED");
};

function isWorker()
{
  // It's conceivable that someone would stub out 'document' in a worker so
  // also check for childNodes, an arbitrary DOM-related object that is
  // meaningless in a WorkerContext.
  return (typeof document === 'undefined' ||
          typeof document.childNodes === 'undefined') && !!self.importScripts;
}

function handleTestFinished() {
  if (!window.jsTestIsAsync)
    finishJSTest();
}

// Returns a sorted array of property names of object.  This function returns
// not only own properties but also properties on prototype chains.
function getAllPropertyNames(object) {
    var properties = [];
    for (var property in object) {
        properties.push(property);
    }
    return properties.sort();
}

if (!isWorker()) {
  window.addEventListener('DOMContentLoaded', handleTestFinished, false);
}
