// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview State and UI for trace data collection.
 */
cr.define('gpu', function() {
  function TracingController() {
    this.overlay_ = $('gpu-tracing-overlay-template').cloneNode(true);
    this.overlay_.removeAttribute('id');
    cr.ui.decorate(this.overlay_, gpu.Overlay);

    this.traceEvents_ = [];

    var startButton = $('gpu-tracing-start-button-template').cloneNode(true);
    startButton.removeAttribute('id');
    document.body.appendChild(startButton);
    startButton.onclick = this.beginTracing.bind(this);

    var stopButton = this.overlay_.querySelector('.gpu-tracing-stop-button');
    stopButton.onclick = this.endTracing.bind(this);
  }

  TracingController.prototype = {
    __proto__: cr.EventTarget.prototype,

    tracingEnabled_: false,

    /**
     * Called by info_view to empty the trace buffer
     */
    beginTracing: function() {
      if (this.tracingEnabled_)
        throw Error('Tracing already begun.');

      this.overlay_.visible = true;

      this.tracingEnabled_ = true;
      console.log('Beginning to trace...');

      this.traceEvents_ = [];
      if (!browserBridge.debugMode)
        chrome.send('beginTracing');

      this.tracingEnabled_ = true;

      var e = new cr.Event('traceBegun');
      e.events = this.traceEvents_;
      this.dispatchEvent(e);

      e = new cr.Event('traceEventsChanged');
      e.numEvents = this.traceEvents_.length;
      this.dispatchEvent(e);

      var self = this;
      window.addEventListener('keypress', function f(e) {
        if (e.keyIdentifier == 'Enter') {
          window.removeEventListener('keypress', f);
          self.endTracing();
        }
      });
    },

    /**
     * Checks whether tracing is enabled
     */
    get isTracingEnabled() {
      return this.tracingEnabled_;
    },

    /**
     * Gets the currently traced events. If tracing is active, then
     * this can change on the fly.
     */
    get traceEvents() {
      return this.traceEvents_;
    },

    /**
     * Callbed by gpu c++ code when new GPU trace data arrives.
     */
    onTraceDataCollected: function(events) {
      this.traceEvents_.push.apply(this.traceEvents_, events);
    },

    /**
     * Called by info_view to finish tracing and update all views.
     */
    endTracing: function() {
      if (!this.tracingEnabled_) throw new Error('Tracing not begun.');

      console.log('Finishing trace');
      if (!browserBridge.debugMode) {
        chrome.send('beginToEndTracing');
      } else {
        var events = getTimelineTestData1();
        this.onTraceDataCollected(events);
        window.setTimeout(this.onEndTracingComplete.bind(this), 250);
      }
    },

    /**
     * Called by the browser when all processes ack tracing
     * having completed.
     */
    onEndTracingComplete: function() {
      this.overlay_.visible = false;
      this.tracingEnabled_ = false;
      console.log('onEndTracingComplete p1 with ' +
                  this.traceEvents_.length + ' events.');
      var e = new cr.Event('traceEnded');
      e.events = this.traceEvents_;
      this.dispatchEvent(e);
    },

    selfTest: function() {
      this.beginTracing();
      window.setTimeout(this.endTracing.bind(This), 500);
    }
  };
  return {
    TracingController: TracingController
  };
});

