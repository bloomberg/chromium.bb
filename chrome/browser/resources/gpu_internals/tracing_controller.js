// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview State and UI for trace data collection.
 */
cr.define('gpu', function() {

  function TracingController() {
    this.overlay_ = document.createElement('div');
    this.overlay_.className = 'gpu-tracing-overlay';

    cr.ui.decorate(this.overlay_, gpu.Overlay);

    this.statusDiv_ = document.createElement('div');
    this.overlay_.appendChild(this.statusDiv_);

    this.bufferPercentDiv_ = document.createElement('div');
    this.overlay_.appendChild(this.bufferPercentDiv_);

    this.stopButton_ = document.createElement('button');
    this.stopButton_.onclick = this.endTracing.bind(this);
    this.stopButton_.innerText = 'Stop tracing';
    this.overlay_.appendChild(this.stopButton_);

    this.traceEvents_ = [];

    if (browserBridge.debugMode) {
      var tracingControllerTests = document.createElement('script');
      tracingControllerTests.src =
          './gpu_internals/tracing_controller_tests.js';
      document.body.appendChild(tracingControllerTests);
    }

    this.onKeydownBoundToThis_ = this.onKeydown_.bind(this);
    this.onKeypressBoundToThis_ = this.onKeypress_.bind(this);
  }

  TracingController.prototype = {
    __proto__: cr.EventTarget.prototype,

    tracingEnabled_: false,
    tracingEnding_: false,

    onRequestBufferPercentFullComplete: function(percent_full) {
      if (!this.overlay_.visible)
        return;

      window.setTimeout(this.beginRequestBufferPercentFull_.bind(this), 250);

      this.bufferPercentDiv_.textContent = 'Buffer usage: ' +
          Math.round(100 * percent_full) + '%';
    },

    /**
     * Begin requesting the buffer fullness
     */
    beginRequestBufferPercentFull_: function() {
      chrome.send('beginRequestBufferPercentFull');
    },

    /**
     * Called by info_view to empty the trace buffer
     */
    beginTracing: function() {
      if (this.tracingEnabled_)
        throw Error('Tracing already begun.');

      this.stopButton_.hidden = false;
      this.statusDiv_.textContent = 'Tracing active.';
      this.overlay_.visible = true;

      this.tracingEnabled_ = true;
      console.log('Beginning to trace...');
      this.statusDiv_.textContent = 'Tracing active.';

      this.traceEvents_ = [];
      if (!browserBridge.debugMode) {
        chrome.send('beginTracing');
        this.beginRequestBufferPercentFull_();
      }

      this.tracingEnabled_ = true;

      var e = new cr.Event('traceBegun');
      e.events = this.traceEvents_;
      this.dispatchEvent(e);

      e = new cr.Event('traceEventsChanged');
      e.numEvents = this.traceEvents_.length;
      this.dispatchEvent(e);

      window.addEventListener('keypress', this.onKeypressBoundToThis_);
      window.addEventListener('keydown', this.onKeydownBoundToThis_);

      // In debug mode, stop tracing automatically
      if (browserBridge.debugMode)
        window.setTimeout(this.endTracing.bind(this), 100);
    },

    onKeydown_: function(e) {
      if (e.keyCode == 27) {
        this.endTracing();
      }
    },

    onKeypress_: function(e) {
      if (e.keyIdentifier == 'Enter') {
        this.endTracing();
      }
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
      this.statusDiv_.textContent = 'Processing trace...';
      this.traceEvents_.push.apply(this.traceEvents_, events);
    },

    /**
     * Called by info_view to finish tracing and update all views.
     */
    endTracing: function() {
      if (!this.tracingEnabled_) throw new Error('Tracing not begun.');
      if (this.tracingEnding_) return;
      this.tracingEnding_ = true;

      this.statusDiv_.textContent = 'Ending trace...';
      console.log('Finishing trace');
      this.statusDiv_.textContent = 'Downloading trace data...';
      this.stopButton_.hidden = true;
      if (!browserBridge.debugMode) {
        // delay sending endTracingAsync until we get a chance to
        // update the screen...
        window.setTimeout(function() {
          chrome.send('endTracingAsync');
        }, 100);
      } else {
        var events = tracingControllerTestEvents;
        this.onTraceDataCollected(events);
        window.setTimeout(this.onEndTracingComplete.bind(this), 250);
      }
    },


    /**
     * Called by the browser when all processes complete tracing.
     */
    onEndTracingComplete: function() {
      window.removeEventListener('keydown', this.onKeydownBoundToThis_);
      window.removeEventListener('keypress', this.onKeypressBoundToThis_);
      this.overlay_.visible = false;
      this.tracingEnabled_ = false;
      this.tracingEnding_ = false;
      console.log('onEndTracingComplete p1 with ' +
                  this.traceEvents_.length + ' events.');
      var e = new cr.Event('traceEnded');
      e.events = this.traceEvents_;
      this.dispatchEvent(e);
    },

    /**
     * Tells browser to put up a load dialog and load the trace file
     */
    beginLoadTraceFile: function() {
      chrome.send('loadTraceFile');
    },

    /**
     * Called by the browser when a trace file is loaded.
     */
    onLoadTraceFileComplete: function(data) {
      if (data.traceEvents) {
        this.traceEvents_ = data.traceEvents;
      } else { // path for loading traces saved without metadata
        if (!data.length)
          console.log('Expected an array when loading the trace file');
        else
          this.traceEvents_ = data;
      }
      var e = new cr.Event('loadTraceFileComplete');
      e.events = this.traceEvents_;
      this.dispatchEvent(e);
    },

    /**
     * Called by the browser when loading a trace file was canceled.
     */
    onLoadTraceFileCanceled: function() {
      cr.dispatchSimpleEvent(this, 'loadTraceFileCanceled');
    },

    /**
     * Tells browser to put up a save dialog and save the trace file
     */
    beginSaveTraceFile: function(traceEvents) {
      var data = {
        traceEvents: traceEvents,
        clientInfo: browserBridge.clientInfo,
        gpuInfo: browserBridge.gpuInfo
      };
      chrome.send('saveTraceFile', [JSON.stringify(data)]);
    },

    /**
     * Called by the browser when a trace file is saveed.
     */
    onSaveTraceFileComplete: function() {
      cr.dispatchSimpleEvent(this, 'saveTraceFileComplete');
    },

    /**
     * Called by the browser when saving a trace file was canceled.
     */
    onSaveTraceFileCanceled: function() {
      cr.dispatchSimpleEvent(this, 'saveTraceFileCanceled');
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

