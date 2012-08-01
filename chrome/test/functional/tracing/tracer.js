// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.pyautoRecordTrace = function(systemTracing) {
  'use strict';
  if( window.timelineModelShimId == undefined )
    window.timelineModelShimId = 0;
  if( window.timelineModelShims == undefined )
    window.timelineModelShims = {};
  var handler = function() {
    tracingController.removeEventListener('traceEnded', handler);
    var model = new TimelineModelShim(
      Array.prototype.slice.call(arguments, 1)
    );
    var events = [tracingController.traceEvents_];
    if (tracingController.supportsSystemTracing)
      events.push(tracingController.systemTraceEvents_);
    model.importTraces(events);
    var shimId = window.timelineModelShimId;
    window.timelineModelShims[shimId] = model;
    window.domAutomationController.send(shimId);
    window.timelineModelShimId++;
  };
  tracingController.addEventListener('traceEnded', handler);
  var willSystemTrace =
    tracingController.supportsSystemTracing ? systemTracing : false;
  tracingController.beginTracing(willSystemTrace);
  return willSystemTrace;
};

// This causes the PyAuto ExecuteJavascript call which executed this file to
// return.
window.domAutomationController.send('');
