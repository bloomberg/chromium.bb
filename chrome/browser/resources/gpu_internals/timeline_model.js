// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview TimelineModel is a parsed representation of the
 * TraceEvents obtained from base/trace_event in which the begin-end
 * tokens are converted into a hierarchy of processes, threads,
 * subrows, and slices.
 *
 * The building block of the model is a slice. A slice is roughly
 * equivalent to function call executing on a specific thread. As a
 * result, slices may have one or more subslices.
 *
 * A thread contains one or more subrows of slices. Row 0 corresponds to
 * the "root" slices, e.g. the topmost slices. Row 1 contains slices that
 * are nested 1 deep in the stack, and so on. We use these subrows to draw
 * nesting tasks.
 *
 */
cr.define('gpu', function() {
  /**
   * A TimelineSlice represents an interval of time on a given thread
   * associated with a specific trace event. For example,
   *   TRACE_EVENT_BEGIN1("x","myArg", 7) at time=0.1ms
   *   TRACE_EVENT_END()                  at time=0.3ms
   * Results in a single timeline slice from 0.1 with duration 0.2.
   *
   * All time units are stored in milliseconds.
   * @constructor
   */
  function TimelineSlice(title, colorId, start, args) {
    this.title = title;
    this.start = start;
    this.colorId = colorId;
    this.args = args;
    this.didNotFinish = false;
    this.subSlices = [];
  }

  TimelineSlice.prototype = {
    selected: false,

    duration: undefined,

    get end() {
      return this.start + this.duration;
    }
  };

  /**
   * A TimelineThread stores all the trace events collected for a particular
   * thread. We organize the slices on a thread by "subrows," where subrow 0
   * has all the root slices, subrow 1 those nested 1 deep, and so on. There
   * is also a set of non-nested subrows.
   *
   * @constructor
   */
  function TimelineThread(parent, tid) {
    this.parent = parent;
    this.tid = tid;
    this.subRows = [[]];
    this.nonNestedSubRows = [];
  }

  TimelineThread.prototype = {
    /**
     * Name of the thread, if present.
     */
    name: undefined,

    getSubrow: function(i) {
      while (i >= this.subRows.length)
        this.subRows.push([]);
      return this.subRows[i];
    },

    addNonNestedSlice: function(slice) {
      for (var i = 0; i < this.nonNestedSubRows.length; i++) {
        var currSubRow = this.nonNestedSubRows[i];
        var lastSlice = currSubRow[currSubRow.length - 1];
        if (slice.start >= lastSlice.start + lastSlice.duration) {
          currSubRow.push(slice);
          return;
        }
      }
      this.nonNestedSubRows.push([slice]);
    },

    /**
     * Updates the minTimestamp and maxTimestamp fields based on the
     * current slices and nonNestedSubRows attached to the thread.
     */
    updateBounds: function() {
      var values = [];
      var slices;
      if (this.subRows[0].length != 0) {
        slices = this.subRows[0];
        values.push(slices[0].start);
        values.push(slices[slices.length - 1].end);
      }
      for (var i = 0; i < this.nonNestedSubRows.length; ++i) {
        slices = this.nonNestedSubRows[i];
        values.push(slices[0].start);
        values.push(slices[slices.length - 1].end);
      }
      if (values.length) {
        this.minTimestamp = Math.min.apply(Math, values);
        this.maxTimestamp = Math.max.apply(Math, values);
      } else {
        this.minTimestamp = undefined;
        this.maxTimestamp = undefined;
      }
    }

  };

  /**
   * Comparison between threads that orders first by pid,
   * then by names, then by tid.
   */
  TimelineThread.compare = function(x,y) {
    if(x.parent.pid != y.parent.pid) {
      return x.parent.pid - y.parent.pid;
    }

    if (x.name && y.name) {
      var tmp = x.name.localeCompare(y.name);
      if (tmp == 0)
        return x.tid - y.tid;
      return tmp;
    } else if (x.name) {
      return -1;
    } else if (y.name){
      return 1;
    } else {
      return x.tid - y.tid;
    }
  };


  /**
   * The TimelineProcess represents a single process in the
   * trace. Right now, we keep this around purely for bookkeeping
   * reasons.
   * @constructor
   */
  function TimelineProcess(pid) {
    this.pid = pid;
    this.threads = {};
  };

  TimelineProcess.prototype = {
    getThread: function(tid) {
      if (!this.threads[tid])
        this.threads[tid] = new TimelineThread(this, tid);
      return this.threads[tid];
    }
  };

  /**
   * Builds a model from an array of TraceEvent objects.
   * @param {Array} events An array of TraceEvents created by
   *     TraceEvent.ToJSON().
   * @constructor
   */
  function TimelineModel(events) {
    this.processes = {};
    this.importErrors = [];

    if (events)
      this.importEvents(events);
  }

  TimelineModel.prototype = {
    __proto__: cr.EventTarget.prototype,

    getProcess: function(pid) {
      if (!this.processes[pid])
        this.processes[pid] = new TimelineProcess(pid);
      return this.processes[pid];
    },

    /**
     * The import takes an array of json-ified TraceEvents and adds them into
     * the TimelineModel as processes, threads, and slices.
     */
    importEvents: function(events) {
      // A ptid is a pid and tid joined together x:y fashion, eg 1024:130
      // The ptid is a unique key for a thread in the trace.
      this.importErrors = [];

      // Threadstate
      const numColorIds = 30;
      function ThreadState(tid) {
        this.openSlices = [];
        this.openNonNestedSlices = {};
      }
      var threadStateByPTID = {};

      var nameToColorMap = {};
      function getColor(name) {
        if (!(name in nameToColorMap)) {
          // Compute a simplistic hashcode of the string so we get consistent
          // coloring across traces.
          var hash = 0;
          for (var i = 0; i < name.length; ++i)
            hash = (hash + 37 * hash + 11 * name.charCodeAt(i)) % 0xFFFFFFFF;
          nameToColorMap[name] = hash % numColorIds;
        }
        return nameToColorMap[name];
      }

      var self = this;

      /**
       * Helper to process a 'begin' event (e.g. initiate a slice).
       * @param {ThreadState} state Thread state (holds slices).
       * @param {Object} event The current trace event.
       */
      function processBegin(state, event) {
        var colorId = getColor(event.name);
        var slice =
            { index: eI,
              slice: new TimelineSlice(event.name, colorId, event.ts,
                                       event.args) };
        if (event.args['ui-nest'] === '0') {
          var sliceID = event.name;
          for (var x in event.args)
            sliceID += ';' + event.args[x];
          if (state.openNonNestedSlices[sliceID])
            this.importErrors.push('Event ' + sliceID + ' already open.');
          state.openNonNestedSlices[sliceID] = slice;
        } else {
          state.openSlices.push(slice);
        }
      }

      /**
       * Helper to process an 'end' event (e.g. close a slice).
       * @param {ThreadState} state Thread state (holds slices).
       * @param {Object} event The current trace event.
       */
      function processEnd(state, event) {
        if (event.args['ui-nest'] === '0') {
          var sliceID = event.name;
          for (var x in event.args)
            sliceID += ';' + event.args[x];
          var slice = state.openNonNestedSlices[sliceID];
          if (!slice)
            return;
          slice.slice.duration = event.ts - slice.slice.start;

          // Store the slice in a non-nested subrow.
          var thread = self.getProcess(event.pid).getThread(event.tid);
          thread.addNonNestedSlice(slice.slice);
          delete state.openNonNestedSlices[name];
        } else {
          if (state.openSlices.length == 0) {
            // Ignore E events that are unmatched.
            return;
          }
          var slice = state.openSlices.pop().slice;
          slice.duration = event.ts - slice.start;

          // Store the slice on the correct subrow.
          var thread = self.getProcess(event.pid).getThread(event.tid);
          var subRowIndex = state.openSlices.length;
          thread.getSubrow(subRowIndex).push(slice);

          // Add the slice to the subSlices array of its parent.
          if (state.openSlices.length) {
            var parentSlice = state.openSlices[state.openSlices.length - 1];
            parentSlice.slice.subSlices.push(slice);
          }
        }
      }

      // Walk through events
      for (var eI = 0; eI < events.length; eI++) {
        var event = events[eI];
        var ptid = event.pid + ':' + event.tid;

        if (!(ptid in threadStateByPTID))
          threadStateByPTID[ptid] = new ThreadState();
        var state = threadStateByPTID[ptid];

        if (event.ph == 'B') {
          processBegin(state, event);
        } else if (event.ph == 'E') {
          processEnd(state, event);
        } else if (event.ph == 'I') {
          // Treat an Instant event as a duration 0 slice.
          // TimelineSliceTrack's redraw() knows how to handle this.
          processBegin(state, event);
          processEnd(state, event);
        } else if (event.ph == 'M') {
          if (event.name == 'thread_name') {
            var thread = this.getProcess(event.pid).getThread(event.tid);
            thread.name = event.args.name;
          } else {
            this.importErrors.push('Unrecognized metadata name: ' + event.name);
          }
        } else {
          this.importErrors.push('Unrecognized event phase: ' + event.ph +
                          '(' + event.name + ')');
        }
      }
      this.pruneEmptyThreads();
      this.updateBounds();

      // Add end events for any events that are still on the stack. These
      // are events that were still open when trace was ended, and can often
      // indicate deadlock behavior.
      for (var ptid in threadStateByPTID) {
        var state = threadStateByPTID[ptid];
        while (state.openSlices.length > 0) {
          var slice = state.openSlices.pop();
          slice.slice.duration = this.maxTimestamp - slice.slice.start;
          slice.slice.didNotFinish = true;
          var event = events[slice.index];

          // Store the slice on the correct subrow.
          var thread = this.getProcess(event.pid).getThread(event.tid);
          var subRowIndex = state.openSlices.length;
          thread.getSubrow(subRowIndex).push(slice.slice);

          // Add the slice to the subSlices array of its parent.
          if (state.openSlices.length) {
            var parentSlice = state.openSlices[state.openSlices.length - 1];
            parentSlice.slice.subSlices.push(slice.slice);
          }
        }
      }

      this.shiftWorldToMicroseconds();

      var boost = (this.maxTimestamp - this.minTimestamp) * 0.15;
      this.minTimestamp = this.minTimestamp - boost;
      this.maxTimestamp = this.maxTimestamp + boost;
    },

    /**
     * Removes threads from the model that have no subrows.
     */
    pruneEmptyThreads: function() {
      for (var pid in this.processes) {
        var process = this.processes[pid];
        var prunedThreads = [];
        for (var tid in process.threads) {
          var thread = process.threads[tid];
          if (thread.subRows[0].length || thread.nonNestedSubRows.legnth)
            prunedThreads.push(thread);
        }
        process.threads = prunedThreads;
      }
    },

    updateBounds: function() {
      var wmin = Infinity;
      var wmax = -wmin;
      var threads = this.getAllThreads();
      for (var tI = 0; tI < threads.length; tI++) {
        var thread = threads[tI];
        thread.updateBounds();
        if (thread.minTimestamp != undefined &&
            thread.maxTimestamp != undefined) {
          wmin = Math.min(wmin, thread.minTimestamp);
          wmax = Math.max(wmax, thread.maxTimestamp);
        }
      }
      this.minTimestamp = wmin;
      this.maxTimestamp = wmax;
    },

    shiftWorldToMicroseconds: function() {
      var timeBase = this.minTimestamp;
      var threads = this.getAllThreads();
      for (var tI = 0; tI < threads.length; tI++) {
        var thread = threads[tI];
        var shiftSubRow = function(subRow) {
          for (var tS = 0; tS < subRow.length; tS++) {
            var slice = subRow[tS];
            slice.start = (slice.start - timeBase) / 1000;
            slice.duration /= 1000;
          }
        };
        for (var tSR = 0; tSR < thread.subRows.length; tSR++) {
          shiftSubRow(thread.subRows[tSR]);
        }
        for (var tSR = 0; tSR < thread.nonNestedSubRows.length; tSR++) {
          shiftSubRow(thread.nonNestedSubRows[tSR]);
        }
      }

      this.updateBounds();
    },

    getAllThreads: function() {
      var threads = [];
      for (var pid in this.processes) {
        var process = this.processes[pid];
        for (var tid in process.threads) {
          threads.push(process.threads[tid]);
        }
      }
      return threads;
    }

  };

  return {
    TimelineSlice: TimelineSlice,
    TimelineThread: TimelineThread,
    TimelineProcess: TimelineProcess,
    TimelineModel: TimelineModel
  };
});
