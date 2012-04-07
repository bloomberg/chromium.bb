// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview ProfilingView visualizes TRACE_EVENT events using the
 * tracing.Timeline component.
 */
cr.define('tracing', function() {
  /**
   * ProfilingView
   * @constructor
   * @extends {ui.TabPanel}
   */
  ProfilingView = cr.ui.define(cr.ui.TabPanel);

  ProfilingView.prototype = {
    __proto__: cr.ui.TabPanel.prototype,

    traceEvents_: [],
    systemTraceEvents_: [],

    decorate: function() {
      cr.ui.TabPanel.prototype.decorate.apply(this);

      // make the <list>/add/save/record element
      this.controlDiv_ = document.createElement('div');
      this.controlDiv_.className = 'control';
      this.appendChild(this.controlDiv_);

      var tracingEl = document.createElement('span');
      tracingEl.textContent = 'Tracing: ';

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

      this.controlDiv_.appendChild(tracingEl);
      this.controlDiv_.appendChild(this.recordBn_);
      this.controlDiv_.appendChild(this.loadBn_);
      this.controlDiv_.appendChild(this.saveBn_);

      if (cr.isChromeOS) {
        this.systemTracingBn_ = document.createElement('input');
        this.systemTracingBn_.type = 'checkbox';
        this.systemTracingBn_.checked = true;

        var systemTracingLabelEl = document.createElement('div');
        systemTracingLabelEl.className = 'label';
        systemTracingLabelEl.textContent = 'System events';
        systemTracingLabelEl.appendChild(this.systemTracingBn_);

        this.controlDiv_.appendChild(systemTracingLabelEl);
      }

      this.container_.appendChild(this.timelineView_);
      this.appendChild(this.container_);

      document.addEventListener('keypress', this.onKeypress_.bind(this));

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
      var traceEvents = tracingController.traceEvents;
      var hasEvents = traceEvents && traceEvents.length;

      this.saveBn_.disabled = !hasEvents;

      if (!hasEvents) return;

      var m = new tracing.TimelineModel();
      m.importEvents(traceEvents, true, [tracingController.systemTraceEvents]);
      this.timelineView_.model = m;
    },

    onKeypress_: function(event) {
      if (event.keyCode == 114 && !tracingController.isTracingEnabled) {
        this.onRecord_();
      }
    },

    ///////////////////////////////////////////////////////////////////////////

    onRecord_: function() {
      tracingController.beginTracing(this.systemTracingBn_.checked);
    },

    onRecordDone_: function() {
      this.refresh_();
    },

    ///////////////////////////////////////////////////////////////////////////

    onSave_: function() {
      this.overlayEl_ = new tracing.Overlay();
      this.overlayEl_.className = 'profiling-overlay';

      var labelEl = document.createElement('div');
      labelEl.className = 'label';
      labelEl.textContent = 'Saving...';
      this.overlayEl_.appendChild(labelEl);
      this.overlayEl_.visible = true;

      tracingController.beginSaveTraceFile();
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
      this.overlayEl_ = new tracing.Overlay();
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
