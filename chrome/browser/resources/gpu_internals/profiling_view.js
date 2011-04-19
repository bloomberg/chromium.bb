// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview ProfilingView visualizes GPU_TRACE events using the
 * gpu.Timeline component.
 */
cr.define('gpu', function() {
  /**
   * ProfilingView
   * @constructor
   * @extends {gpu.Tab}
   */
  ProfilingView = cr.ui.define(gpu.Tab);

  ProfilingView.prototype = {
    __proto__: gpu.Tab.prototype,

    traceEvents_: [],

    decorate: function() {
      // make the <list>/add/save/record element
      this.controlDiv_ = document.createElement('div');
      this.controlDiv_.className = 'control';
      this.appendChild(this.controlDiv_);

      this.recordBn_ = document.createElement('button');
      this.recordBn_.textContent = 'Record';
      this.recordBn_.addEventListener('click', this.onRecord_.bind(this));

      this.saveBn_ = document.createElement('button');
      this.saveBn_.textContent = 'Save';
      this.saveBn_.addEventListener('click', this.onSave_.bind(this));

      this.loadBn_ = document.createElement('button');
      this.loadBn_.textContent = 'Load';
      this.loadBn_.addEventListener('click', this.onLoad_.bind(this));

      this.container_ = document.createElement('div');
      this.container_.className = 'container';

      this.timelineView_ = new TimelineView();

      this.controlDiv_.appendChild(this.recordBn_);
      if (!browserBridge.debugMode) {
        this.controlDiv_.appendChild(this.loadBn_);
        this.controlDiv_.appendChild(this.saveBn_);
      }

      this.container_.appendChild(this.timelineView_);
      this.appendChild(this.container_);

      tracingController.addEventListener('traceEnded',
          this.onRecordDone_.bind(this));
      tracingController.addEventListener('loadTraceFileComplete',
          this.onLoadTraceFileComplete_.bind(this));
      tracingController.addEventListener('saveTraceFileComplete',
          this.onSaveTraceFileComplete_.bind(this));
      tracingController.addEventListener('loadTraceFileCanceled',
          this.onLoadTraceFileCanceled_.bind(this));
      tracingController.addEventListener('saveTraceFileCanceled',
          this.onSaveTraceFileCanceled_.bind(this));
      this.refresh_();
    },

    refresh_: function() {
      var hasEvents = this.traceEvents_ && this.traceEvents_.length;

      this.saveBn_.disabled = !hasEvents;

      this.timelineView_.traceEvents = this.traceEvents_;
    },

    ///////////////////////////////////////////////////////////////////////////

    onRecord_: function() {
      tracingController.beginTracing();
    },

    onRecordDone_: function() {
      this.traceEvents_ = tracingController.traceEvents;
      this.refresh_();
    },

    ///////////////////////////////////////////////////////////////////////////

    onSave_: function() {
      this.overlayEl_ = new gpu.Overlay();
      this.overlayEl_.className = 'profiling-overlay';

      var labelEl = document.createElement('div');
      labelEl.className = 'label';
      labelEl.textContent = 'Saving...';
      this.overlayEl_.appendChild(labelEl);
      this.overlayEl_.visible = true;

      tracingController.beginSaveTraceFile(this.traceEvents_);
    },

    onSaveTraceFileComplete_: function(e) {
      this.overlayEl_.visible = false;
      this.overlayEl_ = undefined;
    },

    onSaveTraceFileCanceled_: function(e) {
      this.overlayEl_.visible = false;
      this.overlayEl_ = undefined;
    },

    ///////////////////////////////////////////////////////////////////////////

    onLoad_: function() {
      this.overlayEl_ = new gpu.Overlay();
      this.overlayEl_.className = 'profiling-overlay';

      var labelEl = document.createElement('div');
      labelEl.className = 'label';
      labelEl.textContent = 'Loading...';
      this.overlayEl_.appendChild(labelEl);
      this.overlayEl_.visible = true;

      tracingController.beginLoadTraceFile();
    },

    onLoadTraceFileComplete_: function(e) {
      this.overlayEl_.visible = false;
      this.overlayEl_ = undefined;

      this.traceEvents_ = e.events;
      this.refresh_();
    },

    onLoadTraceFileCanceled_: function(e) {
      this.overlayEl_.visible = false;
      this.overlayEl_ = undefined;
    }
  };

  return {
    ProfilingView: ProfilingView
  };
});
