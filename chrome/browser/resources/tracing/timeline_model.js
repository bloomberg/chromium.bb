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
   * A TimelineSlice represents an interval of time on a given resource plus
   * parameters associated with that interval.
   *
   * A slice is typically associated with a specific trace event pair on a
   * specific thread.
   * For example,
   *   TRACE_EVENT_BEGIN1("x","myArg", 7) at time=0.1ms
   *   TRACE_EVENT_END()                  at time=0.3ms
   * This results in a single timeline slice from 0.1 with duration 0.2 on a
   * specific thread.
   *
   * A slice can also be an interval of time on a Cpu on a TimelineCpu.
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
   * The TimelineCpu represents a Cpu from the kernel's point of view.
   * @constructor
   */
  function TimelineCpu(number) {
    this.cpuNumber = number;
    this.slices = [];
    this.counters = {};
  };

  TimelineCpu.prototype = {
    /**
     * @return {TimlineCounter} The counter on this process named 'name',
     * creating it if it doesn't exist.
     */
    getOrCreateCounter: function(cat, name) {
      var id;
      if (cat.length)
        id = cat + '.' + name;
      else
        id = name;
      if (!this.counters[id])
        this.counters[id] = new TimelineCounter(this, id, name);
      return this.counters[id];
    },

    /**
     * Updates the minTimestamp and maxTimestamp fields based on the
     * current slices attached to the cpu.
     */
    updateBounds: function() {
      var values = [];
      if (this.slices.length) {
        this.minTimestamp = this.slices[0].start;
        this.maxTimestamp = this.slices[this.slices.length - 1].end;
      } else {
        this.minTimestamp = undefined;
        this.maxTimestamp = undefined;
      }
    }
  };

  /**
   * Comparison between processes that orders by cpuNumber.
   */
  TimelineCpu.compare = function(x, y) {
    return x.cpuNumber - y.cpuNumber;
  };

  // The color pallette is split in half, with the upper
  // half of the pallette being the "highlighted" verison
  // of the base color. So, color 7's highlighted form is
  // 7 + (pallette.length / 2).
  //
  // These bright versions of colors are automatically generated
  // from the base colors.
  //
  // Within the color pallette, there are "regular" colors,
  // which can be used for random color selection, and
  // reserved colors, which are used when specific colors
  // need to be used, e.g. where red is desired.
  const palletteBase = [
    {r: 138, g: 113, b: 152},
    {r: 175, g: 112, b: 133},
    {r: 127, g: 135, b: 225},
    {r: 93, g: 81, b: 137},
    {r: 116, g: 143, b: 119},
    {r: 178, g: 214, b: 122},
    {r: 87, g: 109, b: 147},
    {r: 119, g: 155, b: 95},
    {r: 114, g: 180, b: 160},
    {r: 132, g: 85, b: 103},
    {r: 157, g: 210, b: 150},
    {r: 148, g: 94, b: 86},
    {r: 164, g: 108, b: 138},
    {r: 139, g: 191, b: 150},
    {r: 110, g: 99, b: 145},
    {r: 80, g: 129, b: 109},
    {r: 125, g: 140, b: 149},
    {r: 93, g: 124, b: 132},
    {r: 140, g: 85, b: 140},
    {r: 104, g: 163, b: 162},
    {r: 132, g: 141, b: 178},
    {r: 131, g: 105, b: 147},
    {r: 135, g: 183, b: 98},
    {r: 152, g: 134, b: 177},
    {r: 141, g: 188, b: 141},
    {r: 133, g: 160, b: 210},
    {r: 126, g: 186, b: 148},
    {r: 112, g: 198, b: 205},
    {r: 180, g: 122, b: 195},
    {r: 203, g: 144, b: 152},
    // Reserved Entires
    {r: 182, g: 125, b: 143},
    {r: 126, g: 200, b: 148},
    {r: 133, g: 160, b: 210},
    {r: 240, g: 240, b: 240}];

  // Make sure this number tracks the number of reserved entries in the
  // pallette.
  const numReservedColorIds = 4;

  function brighten(c) {
    var k;
    if (c.r >= 240 && c.g >= 240 && c.b >= 240)
      k = -0.20;
    else
      k = 0.45;

    return {r: Math.min(255, c.r + Math.floor(c.r * k)),
      g: Math.min(255, c.g + Math.floor(c.g * k)),
      b: Math.min(255, c.b + Math.floor(c.b * k))};
  }
  function colorToString(c) {
    return 'rgb(' + c.r + ',' + c.g + ',' + c.b + ')';
  }

  /**
   * The number of color IDs that getStringColorId can choose from.
   */
  const numRegularColorIds = palletteBase.length - numReservedColorIds;
  const highlightIdBoost = palletteBase.length;

  const pallette = palletteBase.concat(palletteBase.map(brighten)).
      map(colorToString);
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
   * Gets the color pallette.
   */
  function getPallette() {
    return pallette;
  }

  /**
   * @return {Number} The value to add to a color ID to get its highlighted
   * colro ID. E.g. 7 + getPalletteHighlightIdBoost() yields a brightened from
   * of 7's base color.
   */
  function getPalletteHighlightIdBoost() {
    return highlightIdBoost;
  }

  /**
   * @param {String} name The color name.
   * @return {Number} The color ID for the given color name.
   */
  function getColorIdByName(name) {
    if (name == 'iowait')
      return numRegularColorIds;
    if (name == 'running')
      return numRegularColorIds + 1;
    if (name == 'runnable')
      return numRegularColorIds + 2;
    if (name == 'sleeping')
      return numRegularColorIds + 3;
    throw 'Unrecognized color ' + name;
  }

  // Previously computed string color IDs. They are based on a stable hash, so
  // it is safe to save them throughout the program time.
  var stringColorIdCache = {};

  /**
   * @return {Number} A color ID that is stably associated to the provided via
   * the getStringHash method. The color ID will be chosen from the regular
   * ID space only, e.g. no reserved ID will be used.
   */
  function getStringColorId(string) {
    if (stringColorIdCache[string] === undefined) {
      var hash = getStringHash(string);
      stringColorIdCache[string] = hash % numRegularColorIds;
    }
    return stringColorIdCache[string];
  }

  /**
   * Builds a model from an array of TraceEvent objects.
   * @param {Object=} opt_data The event data to import into the new model.
   *     See TimelineModel.importEvents for details and more advanced ways to
   *     import data.
   * @param {bool=} opt_zeroAndBoost Whether to align to zero and boost the
   *     by 15%. Defaults to true.
   * @constructor
   */
  function TimelineModel(opt_eventData, opt_zeroAndBoost) {
    this.cpus = {};
    this.processes = {};
    this.importErrors = [];

    if (opt_eventData)
      this.importEvents(opt_eventData, opt_zeroAndBoost);
  }

  var importerConstructors = [];

  /**
   * Registers an importer. All registered importers are considered
   * when processing an import request.
   *
   * @param {Function} importerConstructor The importer's constructor function.
   */
  TimelineModel.registerImporter = function(importerConstructor) {
    importerConstructors.push(importerConstructor);
  }

  TimelineModel.prototype = {
    __proto__: cr.EventTarget.prototype,

    get numProcesses() {
      var n = 0;
      for (var p in this.processes)
        n++;
      return n;
    },

    /**
     * @return {TimelineProcess} Gets a specific TimelineCpu or creates one if
     * it does not exist.
     */
    getOrCreateCpu: function(cpuNumber) {
      if (!this.cpus[cpuNumber])
        this.cpus[cpuNumber] = new TimelineCpu(cpuNumber);
      return this.cpus[cpuNumber];
    },

    /**
     * @return {TimelineProcess} Gets a TimlineProcess for a specified pid or
     * creates one if it does not exist.
     */
    getOrCreateProcess: function(pid) {
      if (!this.processes[pid])
        this.processes[pid] = new TimelineProcess(pid);
      return this.processes[pid];
    },

    /**
     * The import takes an array of json-ified TraceEvents and adds them into
     * the TimelineModel as processes, threads, and slices.
     */

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
      var hasData = false;

      var threads = this.getAllThreads();
      for (var tI = 0; tI < threads.length; tI++) {
        var thread = threads[tI];
        thread.updateBounds();
        if (thread.minTimestamp != undefined &&
            thread.maxTimestamp != undefined) {
          wmin = Math.min(wmin, thread.minTimestamp);
          wmax = Math.max(wmax, thread.maxTimestamp);
          hasData = true;
        }
      }
      var counters = this.getAllCounters();
      for (var tI = 0; tI < counters.length; tI++) {
        var counter = counters[tI];
        counter.updateBounds();
        if (counter.minTimestamp != undefined &&
            counter.maxTimestamp != undefined) {
          hasData = true;
          wmin = Math.min(wmin, counter.minTimestamp);
          wmax = Math.max(wmax, counter.maxTimestamp);
        }
      }

      for (var cpuNumber in this.cpus) {
        var cpu = this.cpus[cpuNumber];
        cpu.updateBounds();
        if (cpu.minTimestamp != undefined &&
            cpu.maxTimestamp != undefined) {
          hasData = true;
          wmin = Math.min(wmin, cpu.minTimestamp);
          wmax = Math.max(wmax, cpu.maxTimestamp);
        }
      }

      if (hasData) {
        this.minTimestamp = wmin;
        this.maxTimestamp = wmax;
      } else {
        this.maxTimestamp = undefined;
        this.minTimestamp = undefined;
      }
    },

    shiftWorldToZero: function() {
      if (this.minTimestamp === undefined)
        return;
      var timeBase = this.minTimestamp;
      var threads = this.getAllThreads();
      for (var tI = 0; tI < threads.length; tI++) {
        var thread = threads[tI];
        var shiftSubRow = function(subRow) {
          for (var tS = 0; tS < subRow.length; tS++) {
            var slice = subRow[tS];
            slice.start = (slice.start - timeBase);
          }
        };

        if (thread.cpuSlices)
          shiftSubRow(thread.cpuSlices);

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
          counter.timestamps[sI] = (counter.timestamps[sI] - timeBase);
      }
      var cpus = this.getAllCpus();
      for (var tI = 0; tI < cpus.length; tI++) {
        var cpu = cpus[tI];
        for (var sI = 0; sI < cpu.slices.length; sI++)
          cpu.slices[sI].start = (cpu.slices[sI].start - timeBase);
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
     * @return {Array} An array of all cpus in the model.
     */
    getAllCpus: function() {
      var cpus = [];
      for (var cpu in this.cpus)
        cpus.push(this.cpus[cpu]);
      return cpus;
    },

    /**
     * @return {Array} An array of all processes in the model.
     */
    getAllProcesses: function() {
      var processes = [];
      for (var pid in this.processes)
        processes.push(this.processes[pid]);
      return processes;
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
      for (var cpuNumber in this.cpus) {
        var cpu = this.cpus[cpuNumber];
        for (var counterName in cpu.counters)
          counters.push(cpu.counters[counterName]);
      }
      return counters;
    },

    /**
     * Imports the provided events into the model. The eventData type
     * is undefined and will be passed to all the timeline importers registered
     * via TimelineModel.registerImporter. The first importer that returns true
     * for canImport(events) will be used to import the events.
     *
     * @param {Object} events Events to import.
     * @param {boolean} isChildImport True the eventData being imported is an
     *     additional trace after the primary eventData.
     */
    importOneTrace_: function(eventData, isAdditionalImport) {
      var importerConstructor;
      for (var i = 0; i < importerConstructors.length; ++i) {
        if (importerConstructors[i].canImport(eventData)) {
          importerConstructor = importerConstructors[i];
          break;
        }
      }
      if (!importerConstructor)
        throw 'Could not find an importer for the provided eventData.';

      var importer = new importerConstructor(
          this, eventData, isAdditionalImport);
      importer.importEvents();
      this.pruneEmptyThreads();
    },

    /**
     * Imports the provided traces into the model. The eventData type
     * is undefined and will be passed to all the timeline importers registered
     * via TimelineModel.registerImporter. The first importer that returns true
     * for canImport(events) will be used to import the events.
     *
     * The primary trace is provided via the eventData variable. If multiple
     * traces are to be imported, specify the first one as events, and the
     * remainder in the opt_additionalEventData array.
     *
     * @param {Object} eventData Events to import.
     * @param {bool=} opt_zeroAndBoost Whether to align to zero and boost the
     *     by 15%. Defaults to true.
     * @param {Array=} opt_additionalEventData An array of eventData objects
     *     (e.g. array of arrays) to
     * import after importing the primary events.
     */
    importEvents: function(eventData,
                           opt_zeroAndBoost, opt_additionalEventData) {
      if (opt_zeroAndBoost === undefined)
        opt_zeroAndBoost = true;

      this.importOneTrace_(eventData, false);
      if (opt_additionalEventData) {
        for (var i = 0; i < opt_additionalEventData.length; ++i) {
          this.importOneTrace_(opt_additionalEventData[i], true);
        }
      }

      this.updateBounds();

      if (opt_zeroAndBoost)
        this.shiftWorldToZero();

      if (opt_zeroAndBoost &&
          this.minTimestamp !== undefined &&
          this.maxTimestamp !== undefined) {
        var boost = (this.maxTimestamp - this.minTimestamp) * 0.15;
        this.minTimestamp = this.minTimestamp - boost;
        this.maxTimestamp = this.maxTimestamp + boost;
      }
    }
  };

  return {
    getPallette: getPallette,
    getPalletteHighlightIdBoost: getPalletteHighlightIdBoost,
    getColorIdByName: getColorIdByName,
    getStringHash: getStringHash,
    getStringColorId: getStringColorId,

    TimelineSlice: TimelineSlice,
    TimelineThread: TimelineThread,
    TimelineCounter: TimelineCounter,
    TimelineProcess: TimelineProcess,
    TimelineCpu: TimelineCpu,
    TimelineModel: TimelineModel
  };
});
