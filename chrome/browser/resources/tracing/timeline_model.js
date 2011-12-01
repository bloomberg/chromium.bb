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
cr.define('tracing', function() {
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
  function TimelineSlice(title, colorId, start, args, opt_duration) {
    this.title = title;
    this.start = start;
    this.colorId = colorId;
    this.args = args;
    this.didNotFinish = false;
    this.subSlices = [];
    if (opt_duration !== undefined)
      this.duration = opt_duration;
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
    },

    /**
     * @return {String} A user-friendly name for this thread.
     */
    get userFriendlyName() {
      var tname = this.name || this.tid;
      return this.parent.pid + ': ' + tname;
    },

    /**
     * @return {String} User friendly details about this thread.
     */
    get userFriendlyDetials() {
      return 'pid: ' + this.parent.pid +
          ', tid: ' + this.tid +
          (this.name ? ', name: ' + this.name : '');
    }

  };

  /**
   * Comparison between threads that orders first by pid,
   * then by names, then by tid.
   */
  TimelineThread.compare = function(x, y) {
    if (x.parent.pid != y.parent.pid) {
      return TimelineProcess.compare(x.parent, y.parent.pid);
    }

    if (x.name && y.name) {
      var tmp = x.name.localeCompare(y.name);
      if (tmp == 0)
        return x.tid - y.tid;
      return tmp;
    } else if (x.name) {
      return -1;
    } else if (y.name) {
      return 1;
    } else {
      return x.tid - y.tid;
    }
  };

  /**
   * Stores all the samples for a given counter.
   * @constructor
   */
  function TimelineCounter(parent, id, name) {
    this.parent = parent;
    this.id = id;
    this.name = name;
    this.seriesNames = [];
    this.seriesColors = [];
    this.timestamps = [];
    this.samples = [];
  }

  TimelineCounter.prototype = {
    __proto__: Object.prototype,

    get numSeries() {
      return this.seriesNames.length;
    },

    get numSamples() {
      return this.timestamps.length;
    },

    /**
     * Updates the bounds for this counter based on the samples it contains.
     */
    updateBounds: function() {
      if (this.seriesNames.length != this.seriesColors.length)
        throw 'seriesNames.length must match seriesColors.length';
      if (this.numSeries * this.numSamples != this.samples.length)
        throw 'samples.length must be a multiple of numSamples.';

      this.totals = [];
      if (this.samples.length == 0) {
        this.minTimestamp = undefined;
        this.maxTimestamp = undefined;
        this.maxTotal = 0;
        return;
      }
      this.minTimestamp = this.timestamps[0];
      this.maxTimestamp = this.timestamps[this.timestamps.length - 1];

      var numSeries = this.numSeries;
      var maxTotal = -Infinity;
      for (var i = 0; i < this.timestamps.length; i++) {
        var total = 0;
        for (var j = 0; j < numSeries; j++) {
          total += this.samples[i * numSeries + j];
          this.totals.push(total);
        }
        if (total > maxTotal)
          maxTotal = total;
      }
      this.maxTotal = maxTotal;
    }

  };

  /**
   * Comparison between counters that orders by pid, then name.
   */
  TimelineCounter.compare = function(x, y) {
    if (x.parent.pid != y.parent.pid) {
      return TimelineProcess.compare(x.parent, y.parent.pid);
    }
    var tmp = x.name.localeCompare(y.name);
    if (tmp == 0)
      return x.tid - y.tid;
    return tmp;
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
    this.counters = {};
  };

  TimelineProcess.prototype = {
    get numThreads() {
      var n = 0;
      for (var p in this.threads) {
        n++;
      }
      return n;
    },

    /**
     * @return {TimlineThread} The thread identified by tid on this process,
     * creating it if it doesn't exist.
     */
    getOrCreateThread: function(tid) {
      if (!this.threads[tid])
        this.threads[tid] = new TimelineThread(this, tid);
      return this.threads[tid];
    },

    /**
     * @return {TimlineCounter} The counter on this process named 'name',
     * creating it if it doesn't exist.
     */
    getOrCreateCounter: function(cat, name) {
      var id = cat + '.' + name;
      if (!this.counters[id])
        this.counters[id] = new TimelineCounter(this, id, name);
      return this.counters[id];
    }
  };

  /**
   * Comparison between processes that orders by pid.
   */
  TimelineProcess.compare = function(x, y) {
    return x.pid - y.pid;
  };

  /**
   * Computes a simplistic hashcode of the provide name. Used to chose colors
   * for slices.
   * @param {string} name The string to hash.
   */
  function getStringHash(name) {
    var hash = 0;
    for (var i = 0; i < name.length; ++i)
      hash = (hash + 37 * hash + 11 * name.charCodeAt(i)) % 0xFFFFFFFF;
    return hash;
  }

  /**
   * The number of color IDs that getStringColorId can choose from.
   */
  const numColorIds = 30;

  /**
   * @return {Number} A color ID that is stably associated to the provided via
   * the getStringHash method.
   */
  function getStringColorId(string) {
    var hash = getStringHash(string);
    return hash % numColorIds;
  }

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

    get numProcesses() {
      var n = 0;
      for (var p in this.processes)
        n++;
      return n;
    },

    getOrCreateProcess: function(pid) {
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

      // Threadstate.
      function ThreadState(tid) {
        this.openSlices = [];
        this.openNonNestedSlices = {};
      }
      var threadStateByPTID = {};

      var nameToColorMap = {};
      function getColor(name) {
        if (!(name in nameToColorMap)) {
          nameToColorMap[name] = getStringColorId(name);
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

        if (event.uts)
          slice.slice.startInUserTime = event.uts;

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
          if (event.uts)
            slice.durationInUserTime = event.uts - slice.slice.startInUserTime;

          // Store the slice in a non-nested subrow.
          var thread =
              self.getOrCreateProcess(event.pid).getOrCreateThread(event.tid);
          thread.addNonNestedSlice(slice.slice);
          delete state.openNonNestedSlices[name];
        } else {
          if (state.openSlices.length == 0) {
            // Ignore E events that are unmatched.
            return;
          }
          var slice = state.openSlices.pop().slice;
          slice.duration = event.ts - slice.start;
          if (event.uts)
            slice.durationInUserTime = event.uts - slice.startInUserTime;

          // Store the slice on the correct subrow.
          var thread = self.getOrCreateProcess(event.pid)
              .getOrCreateThread(event.tid);
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
        } else if (event.ph == 'C') {
          var ctr_name;
          if (event.id !== undefined)
            ctr_name = event.name + '[' + event.id + ']';
          else
            ctr_name = event.name;

          var ctr = this.getOrCreateProcess(event.pid)
              .getOrCreateCounter(event.cat, ctr_name);
          // Initialize the counter's series fields if needed.
          if (ctr.numSeries == 0) {
            for (var seriesName in event.args) {
              ctr.seriesNames.push(seriesName);
              ctr.seriesColors.push(
                  getStringColorId(ctr.name + '.' + seriesName));
            }
            if (ctr.numSeries == 0) {
              this.importErrors.push('Expected counter ' + event.name +
                  ' to have at least one argument to use as a value.');
              // Drop the counter.
              delete ctr.parent.counters[ctr.name];
              continue;
            }
          }

          // Add the sample values.
          ctr.timestamps.push(event.ts);
          for (var i = 0; i < ctr.numSeries; i++) {
            var seriesName = ctr.seriesNames[i];
            if (event.args[seriesName] === undefined) {
              ctr.samples.push(0);
              continue;
            }
            ctr.samples.push(event.args[seriesName]);
          }

        } else if (event.ph == 'M') {
          if (event.name == 'thread_name') {
            var thread = this.getOrCreateProcess(event.pid)
                             .getOrCreateThread(event.tid);
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

      // Adjust the model's max value temporarily to include the max value of
      // any of the open slices, since they wouldn't have been included in the
      // bounds calculation. We need the true global max value because the
      // duration for any open slices is set so that they end at this global
      // maximum.
      for (var ptid in threadStateByPTID) {
        var state = threadStateByPTID[ptid];
        for (var i = 0; i < state.openSlices.length; i++) {
          var slice = state.openSlices[i];
          this.minTimestamp = Math.min(this.minTimestamp, slice.slice.start);
          this.maxTimestamp = Math.max(this.maxTimestamp, slice.slice.start);
          for (var s = 0; s < slice.slice.subSlices.length; s++) {
            var subSlice = slice.slice.subSlices[s];
            this.minTimestamp = Math.min(this.minTimestamp, subSlice.start);
            this.maxTimestamp = Math.max(this.maxTimestamp, subSlice.start);
            if (subSlice.duration)
              this.maxTimestamp = Math.max(this.maxTimestamp, subSlice.end);
          }
        }
      }

      // Automatically close any slices are still open. These occur in a number
      // of reasonable situations, e.g. deadlock. This pass ensures the open
      // slices make it into the final model.
      for (var ptid in threadStateByPTID) {
        var state = threadStateByPTID[ptid];
        while (state.openSlices.length > 0) {
          var slice = state.openSlices.pop();
          slice.slice.duration = this.maxTimestamp - slice.slice.start;
          slice.slice.didNotFinish = true;
          var event = events[slice.index];

          // Store the slice on the correct subrow.
          var thread = this.getOrCreateProcess(event.pid)
                           .getOrCreateThread(event.tid);
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
     * Removes threads from the model that are fully empty.
     */
    pruneEmptyThreads: function() {
      for (var pid in this.processes) {
        var process = this.processes[pid];
        var prunedThreads = {};
        for (var tid in process.threads) {
          var thread = process.threads[tid];

          // Begin-events without matching end events leave a thread in a state
          // where the toplevel subrows are empty but child subrows have
          // entries. The autocloser will fix this up later. But, for the
          // purposes of pruning, such threads need to be treated as having
          // content.
          var hasNonEmptySubrow = false;
          for (var s = 0; s < thread.subRows.length; s++)
            hasNonEmptySubrow |= thread.subRows[s].length > 0;

          if (hasNonEmptySubrow || thread.nonNestedSubRows.legnth)
            prunedThreads[tid] = thread;
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
      var counters = this.getAllCounters();
      for (var tI = 0; tI < counters.length; tI++) {
        var counter = counters[tI];
        counter.updateBounds();
        if (counter.minTimestamp != undefined &&
            counter.maxTimestamp != undefined) {
          wmin = Math.min(wmin, counter.minTimestamp);
          wmax = Math.max(wmax, counter.maxTimestamp);
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
            if (slice.startInUserTime)
              slice.startInUserTime /= 1000;
            if (slice.durationInUserTime)
              slice.durationInUserTime /= 1000;
          }
        };
        for (var tSR = 0; tSR < thread.subRows.length; tSR++) {
          shiftSubRow(thread.subRows[tSR]);
        }
        for (var tSR = 0; tSR < thread.nonNestedSubRows.length; tSR++) {
          shiftSubRow(thread.nonNestedSubRows[tSR]);
        }
      }
      var counters = this.getAllCounters();
      for (var tI = 0; tI < counters.length; tI++) {
        var counter = counters[tI];
        for (var sI = 0; sI < counter.timestamps.length; sI++)
          counter.timestamps[sI] = (counter.timestamps[sI] - timeBase) / 1000;
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
    },

    /**
     * @return {Array} An array of all the counters in the model.
     */
    getAllCounters: function() {
      var counters = [];
      for (var pid in this.processes) {
        var process = this.processes[pid];
        for (var tid in process.counters) {
          counters.push(process.counters[tid]);
        }
      }
      return counters;
    }

  };

  return {
    getStringHash: getStringHash,
    getStringColorId: getStringColorId,
    TimelineSlice: TimelineSlice,
    TimelineThread: TimelineThread,
    TimelineCounter: TimelineCounter,
    TimelineProcess: TimelineProcess,
    TimelineModel: TimelineModel
  };
});
