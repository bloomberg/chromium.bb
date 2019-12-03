// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Overview Tracing UI.
 */

/**
 * @type {Array<string>}.
 * List of available colors to be used in charts. Each model is associated with
 * the same color in all charts.
 */
var chartColors = [
  '#e6194B',
  '#3cb44b',
  '#4363d8',
  '#f58231',
  '#911eb4',
  '#42d4f4',
  '#f032e6',
  '#469990',
  '#9A6324',
  '#800000',
  '#808000',
  '#000075',
];

/**
 * @type {Array<Object>}.
 * Array of models to display.
 */
var models = [];

/**
 * @type {Array<string>}.
 * Array of taken colors and it is used to prevent several models are displayed
 * in the same color.
 */
var takenColors = [];

/**
 * Maps model to the associated color.
 */
var modelColors = new Map();

function initializeOverviewUi() {
  initializeUi(8 /* zoomLevel */, function() {
    // Update function.
    refreshModels();
  });
}

/**
 * Helper that calculates overall frequency of events.
 *
 * @param {Events} events events to analyze.
 * @param {number} duration duration of analyzed period.
 */
function calculateFPS(events, duration) {
  var eventCount = 0;
  var index = events.getFirstEvent();
  while (index >= 0) {
    ++eventCount;
    index = events.getNextEvent(index, 1 /* direction */);
  }
  // Duration in micro-seconds.
  return eventCount * 1000000 / duration;
}

/**
 * Helper that calculates render quality and commit deviation. This follows the
 * calculation in |ArcAppPerformanceTracingSession|.
 *
 * @param {Object} model model to process.
 */
function calculateAppRenderQualityAndCommitDeviation(model) {
  var deltas = createDeltaEvents(getAppCommitEvents(model));

  var vsyncErrorDeviationAccumulator = 0.0;
  // Frame delta in microseconds.
  var targetFrameTime = 16666;
  for (var i = 0; i < deltas.events.length; i++) {
    var displayFramesPassed = Math.round(deltas.events[i][2] / targetFrameTime);
    var vsyncError =
        deltas.events[i][2] - displayFramesPassed * targetFrameTime;
    vsyncErrorDeviationAccumulator += (vsyncError * vsyncError);
  }
  var commitDeviation =
      Math.sqrt(vsyncErrorDeviationAccumulator / deltas.events.length);

  // Sort by time delta.
  deltas.events.sort(function(a, b) {
    return a[2] - b[2];
  });

  // Get 10% and 90% indices.
  var lowerPosition = Math.round(deltas.events.length / 10);
  var upperPosition = deltas.events.length - 1 - lowerPosition;
  var renderQuality =
      deltas.events[lowerPosition][2] / deltas.events[upperPosition][2];

  return [
    renderQuality * 100.0 /* convert to % */,
    commitDeviation * 0.001 /* mcs to ms */
  ];
}

/**
 * Gets model title as an traced app name. If no information is available it
 * returns default name for app.
 *
 * @param {Object} model model to process.
 */
function getModelTitle(model) {
  return model.information.title ? model.information.title : 'Unknown app';
}

/**
 * Creates view that describes particular model. It shows all relevant
 * information and allows remove the model from the view.
 *
 * @param {Object} model model to process.
 */
function addModelHeader(model) {
  var header = $('arc-overview-tracing-model-template').cloneNode(true);
  header.hidden = false;

  if (model.information.icon) {
    header.getElementsByClassName('arc-tracing-app-icon')[0].src =
        'data:image/png;base64,' + model.information.icon;
  }
  header.getElementsByClassName('arc-tracing-app-title')[0].textContent =
      getModelTitle(model);
  var date = model.information.timestamp ?
      new Date(model.information.timestamp).toLocaleString() :
      'Unknown date';
  header.getElementsByClassName('arc-tracing-app-date')[0].textContent = date;
  var duration = (model.information.duration * 0.000001).toFixed(2);
  header.getElementsByClassName('arc-tracing-app-duration')[0].textContent =
      duration;
  var platform = model.information.platform ? model.information.platform :
                                              'Unknown platform';
  header.getElementsByClassName('arc-tracing-app-platform')[0].textContent =
      platform;

  header.getElementsByClassName('arc-tracing-app-fps')[0].textContent =
      calculateFPS(getAppCommitEvents(model), model.information.duration)
          .toFixed(2);
  header.getElementsByClassName('arc-tracing-chrome-fps')[0].textContent =
      calculateFPS(getChromeSwapEvents(model), model.information.duration)
          .toFixed(2);
  var renderQualityAndCommitDeviation =
      calculateAppRenderQualityAndCommitDeviation(model);
  header.getElementsByClassName('arc-tracing-app-render-quality')[0]
      .textContent = renderQualityAndCommitDeviation[0].toFixed(1) + '%';
  header.getElementsByClassName('arc-tracing-app-commit-deviation')[0]
      .textContent = renderQualityAndCommitDeviation[1].toFixed(2) + 'ms';

  var cpuPower = getAveragePower(model, 10 /* kCpuPower */);
  var gpuPower = getAveragePower(model, 11 /* kGpuPower */);
  var memoryPower = getAveragePower(model, 12 /* kMemoryPower */);
  header.getElementsByClassName('arc-tracing-app-power-total')[0].textContent =
      (cpuPower + gpuPower + memoryPower).toFixed(2);
  header.getElementsByClassName('arc-tracing-app-power-cpu')[0].textContent =
      cpuPower.toFixed(2);
  header.getElementsByClassName('arc-tracing-app-power-gpu')[0].textContent =
      gpuPower.toFixed(2);
  header.getElementsByClassName('arc-tracing-app-power-memory')[0].textContent =
      memoryPower.toFixed(2);

  // Handler to remove model from the view.
  header.getElementsByClassName('arc-tracing-close-button')[0].onclick =
      function() {
    removeModel(model);
  };

  header.getElementsByClassName('arc-tracing-dot')[0].style.backgroundColor =
      modelColors.get(model);

  $('arc-overview-tracing-models').appendChild(header);
}

/**
 * Helper that analyzes model, extracts surface commit events and creates
 * composited events. These events are distributed per different buffers and
 * output contains these events in one line.
 *
 * @param {object} model source model to analyze.
 */
function getAppCommitEvents(model) {
  var events = [];
  for (var i = 0; i < model.views.length; i++) {
    var view = model.views[i];
    for (var j = 0; j < view.buffers.length; j++) {
      var commitEvents =
          new Events(view.buffers[j], 200 /* kExoSurfaceAttach */);
      var index = commitEvents.getFirstEvent();
      while (index >= 0) {
        events.push(commitEvents.events[index]);
        index = commitEvents.getNextEvent(index, 1 /* direction */);
      }
    }
  }

  // Sort by timestamp.
  events.sort(function(a, b) {
    return a[1] - b[1];
  });

  return new Events(events, 200 /* kExoSurfaceAttach */);
}

/**
 * Helper that analyzes power events of particular type, calculates overall
 * energy consumption and returns average power between first and last event.
 *
 * @param {object} model source model to analyze.
 * @param {number} eventType event type to match particular power counter.
 */
function getAveragePower(model, eventType) {
  var events = new Events(model.system.memory, eventType);
  var lastTimestamp = 0;
  var totalEnergy = 0;
  var index = events.getFirstEvent();
  while (index >= 0) {
    var timestamp = events.events[index][1];
    totalEnergy +=
        events.events[index][2] * (timestamp - lastTimestamp) * 0.001;
    lastTimestamp = timestamp;
    index = events.getNextEvent(index, 1 /* direction */);
  }

  if (!lastTimestamp) {
    return 0;
  }

  return totalEnergy / lastTimestamp;
}

/**
 * Helper that analyzes model, extracts Chrome swap events and creates
 * composited events. These events are distributed per different buffers and
 * output contains these events in one line.
 *
 * @param {object} model source model to analyze.
 */
function getChromeSwapEvents(model) {
  var events = [];
  for (var i = 0; i < model.chrome.buffers.length; i++) {
    var swapEvents =
        new Events(model.chrome.buffers[i], 504 /* kChromeOSSwapDone */);
    var index = swapEvents.getFirstEvent();
    while (index >= 0) {
      events.push(swapEvents.events[index]);
      index = swapEvents.getNextEvent(index, 1 /* direction */);
    }
  }
  // Sort by timestamp.
  events.sort(function(a, b) {
    return a[1] - b[1];
  });

  return new Events(events, 504 /* kChromeOSSwapDone */);
}

/**
 * Creates events as a smoothed event frequency.
 *
 * @param events source events to analyze.
 * @param {number} duration duration to analyze in microseconds.
 * @param {windowSize} window size to smooth values.
 * @param {step} step to generate next results in microseconds.
 */
function createFPSEvents(events, duration, windowSize, step) {
  var fpsEvents = [];
  var timestamp = 0;
  var index = events.getFirstEvent();
  while (timestamp < duration) {
    var windowFromTimestamp = timestamp - windowSize / 2;
    var windowToTimestamp = timestamp + windowSize / 2;
    // Clamp ranges.
    if (windowToTimestamp > duration) {
      windowFromTimestamp = duration - windowSize;
      windowToTimestamp = duration;
    }
    if (windowFromTimestamp < 0) {
      windowFromTimestamp = 0;
      windowToTimestamp = windowSize;
    }
    while (index >= 0 && events.events[index][1] < windowFromTimestamp) {
      index = events.getNextEvent(index, 1 /* direction */);
    }
    var frames = 0;
    var scanIndex = index;
    while (scanIndex >= 0 && events.events[scanIndex][1] < windowToTimestamp) {
      ++frames;
      scanIndex = events.getNextEvent(scanIndex, 1 /* direction */);
    }
    frames = frames * 1000000 / windowSize;
    fpsEvents.push([0 /* type does not matter */, timestamp, frames]);
    timestamp = timestamp + step;
  }

  return new Events(fpsEvents, 0, 0);
}

/**
 * Creates events as a time difference between events.
 *
 * @param events source events to analyze.
 */
function createDeltaEvents(events) {
  var timeEvents = [];
  var timestamp = 0;
  var lastIndex = events.getFirstEvent();
  while (lastIndex >= 0) {
    var index = events.getNextEvent(lastIndex, 1 /* direction */);
    if (index < 0) {
      break;
    }
    var delta = events.events[index][1] - events.events[lastIndex][1];
    timeEvents.push(
        [0 /* type does not mattter */, events.events[index][1], delta]);
    lastIndex = index;
  }

  return new Events(timeEvents, 0, 0);
}

/**
 * Creates view that shows CPU frequency.
 *
 * @param {HTMLElement} parent container for the newly created view.
 * @param {number} resolution scale of the chart in microseconds per pixel.
 * @param {number} duration length of the chart in microseconds.
 */
function addCPUFrequencyView(parent, resolution, duration) {
  // Range from 0 to 3GHz
  // 50MHz  1 pixel resolution
  var bands = createChart(
      parent, 'CPU Frequency' /* title */, resolution, duration,
      60 /* height */, 5 /* gridLinesCount */);
  var attributesTemplate =
      Object.assign({}, valueAttributes[9 /* kCpuFrequency */]);
  attributesTemplate.minValue = 0;
  attributesTemplate.maxValue = 3000000;  // Khz
  for (i = 0; i < models.length; i++) {
    var attributes = Object.assign({}, attributesTemplate);
    attributes.color = modelColors.get(models[i]);
    bands.addChartSources(
        [new Events(models[i].system.memory, 9 /* kCpuFrequency */)],
        true /* smooth */, attributes);
  }
}

/**
 * Creates view that shows CPU temperature.
 *
 * @param {HTMLElement} parent container for the newly created view.
 * @param {number} resolution scale of the chart in microseconds per pixel.
 * @param {number} duration length of the chart in microseconds.
 */
function addCPUTempView(parent, resolution, duration) {
  // Range from 20 to 100 celsius
  // 2 celsius 1 pixel resolution
  var bands = createChart(
      parent, 'CPU Temperature' /* title */, resolution, duration,
      40 /* height */, 3 /* gridLinesCount */);
  var attributesTemplate =
      Object.assign({}, valueAttributes[8 /* kCpuTemperature */]);
  attributesTemplate.minValue = 20000;
  attributesTemplate.maxValue = 100000;
  for (i = 0; i < models.length; i++) {
    var attributes = Object.assign({}, attributesTemplate);
    attributes.color = modelColors.get(models[i]);
    bands.addChartSources(
        [new Events(models[i].system.memory, 8 /* kCpuTemperature */)],
        true /* smooth */, attributes);
  }
}

/**
 * Creates view that shows GPU frequency.
 *
 * @param {HTMLElement} parent container for the newly created view.
 * @param {number} resolution scale of the chart in microseconds per pixel.
 * @param {number} duration length of the chart in microseconds.
 */
function addGPUFrequencyView(parent, resolution, duration) {
  // Range from 300MHz to 1GHz
  // 14MHz  1 pixel resolution
  var bands = createChart(
      parent, 'GPU Frequency' /* title */, resolution, duration,
      50 /* height */, 4 /* gridLinesCount */);
  var attributesTemplate =
      Object.assign({}, valueAttributes[7 /* kGpuFrequency */]);
  attributesTemplate.minValue = 300;   // Mhz
  attributesTemplate.maxValue = 1000;  // Mhz
  for (i = 0; i < models.length; i++) {
    var attributes = Object.assign({}, attributesTemplate);
    attributes.color = modelColors.get(models[i]);
    bands.addChartSources(
        [new Events(models[i].system.memory, 7 /* kGpuFrequency */)],
        false /* smooth */, attributes);
  }
}

/**
 * Creates view that shows FPS change for app commits or swaps for Chrome
 * updates.
 *
 * @param {HTMLElement} parent container for the newly created view.
 * @param {number} resolution scale of the chart in microseconds per pixel.
 * @param {number} duration length of the chart in microseconds.
 * @param {boolean} appView true for commits for app or false for swaps for
 *                  Chrome.
 */
function addFPSView(parent, resolution, duration, appView) {
  // FPS range from 10 to 70.
  // 1 fps 1 pixel resolution.
  var title = appView ? 'App FPS' : 'Chrome FPS';
  var bands = createChart(
      parent, title, resolution, duration, 60 /* height */,
      5 /* gridLinesCount */);
  var attributesTemplate =
      {maxValue: 70, minValue: 10, name: 'fps', scale: 1.0, width: 1.0};
  for (i = 0; i < models.length; i++) {
    var attributes = Object.assign({}, attributesTemplate);
    var events = appView ? getAppCommitEvents(models[i]) :
                           getChromeSwapEvents(models[i]);
    var fpsEvents = createFPSEvents(
        events, duration, 200000 /* windowSize, 0.2s */,
        16667 /* step per 60 FPS */);
    attributes.color = modelColors.get(models[i]);
    bands.addChartSources([fpsEvents], true /* smooth */, attributes);
  }
}

/**
 * Creates view that shows commit time delta for app or swap time delta for
 * Chrome updates.
 *
 * @param {HTMLElement} parent container for the newly created view.
 * @param {number} resolution scale of the chart in microseconds per pixel.
 * @param {number} duration length of the chart in microseconds.
 * @param {boolean} appView true for commit time for app or false for swap time
 *                  for Chrome.
 */
function addDeltaView(parent, resolution, duration, appView) {
  // time range from 0 to 67ms. 66.67ms is for 15 FPS.
  var title = appView ? 'App commit time' : 'Chrome swap time';
  // 1 ms 1 pixel resolution.  Each grid lines correspond 1/120 FPS time update.
  var bands = createChart(
      parent, title, resolution, duration, 67 /* height */,
      7 /* gridLinesCount */);
  var attributesTemplate = {
    maxValue: 67000,  // microseconds
    minValue: 0,
    name: 'ms',
    scale: 1.0 / 1000.0,
    width: 1.0
  };
  for (i = 0; i < models.length; i++) {
    var attributes = Object.assign({}, attributesTemplate);
    var events = appView ? getAppCommitEvents(models[i]) :
                           getChromeSwapEvents(models[i]);
    var timeEvents = createDeltaEvents(events);
    attributes.color = modelColors.get(models[i]);
    bands.addChartSources([timeEvents], false /* smooth */, attributes);
  }
}

/**
 * Creates power view for the particular counter.
 *
 * @param {HTMLElement} parent container for the newly created chart.
 * @param {string} title of the chart.
 * @param {number} resolution scale of the chart in microseconds per pixel.
 * @param {number} duration length of the chart in microseconds.
 * @param {number} eventType event type to match particular power counter.
 */
function addPowerView(parent, title, resolution, duration, eventType) {
  // power range from 0 to 10000 milli-watts.
  // 200 milli-watts 1 pixel resolution. Each grid line 2 watts
  bands = createChart(
      parent, title, resolution, duration, 50 /* height */,
      4 /* gridLinesCount */);
  var attributesTemplate = {
    maxValue: 10000,
    minValue: 0,
    name: 'watts',
    scale: 1.0 / 1024,
    width: 1.0
  };
  for (i = 0; i < models.length; i++) {
    var attributes = Object.assign({}, attributesTemplate);
    attributes.color = modelColors.get(models[i]);
    bands.addChartSources(
        [new Events(models[i].system.memory, eventType, eventType)],
        false /* smooth */, attributes);
  }
}

/**
 * Refreshes view, remove all content and creates new one from all available
 * models.
 */
function refreshModels() {
  // Clear previous content.
  $('arc-event-bands').textContent = '';
  $('arc-overview-tracing-models').textContent = '';

  if (models.length == 0) {
    return;
  }

  // Microseconds per pixel. 100% zoom corresponds to 100 mcs per pixel.
  var resolution = zooms[zoomLevel];
  var parent = $('arc-event-bands');

  var duration = 0;
  for (i = 0; i < models.length; i++) {
    duration = Math.max(duration, models[i].information.duration);
  }

  for (i = 0; i < models.length; i++) {
    addModelHeader(models[i]);
  }

  addCPUFrequencyView(parent, resolution, duration);
  addCPUTempView(parent, resolution, duration);
  addGPUFrequencyView(parent, resolution, duration);
  addFPSView(parent, resolution, duration, true /* appView */);
  addDeltaView(parent, resolution, duration, true /* appView */);
  addFPSView(parent, resolution, duration, false /* appView */);
  addDeltaView(parent, resolution, duration, false /* appView */);
  addPowerView(parent, 'CPU Power', resolution, duration, 10 /* eventType */);
  addPowerView(parent, 'GPU Power', resolution, duration, 11 /* eventType */);
  addPowerView(
      parent, 'Memory Power', resolution, duration, 12 /* eventType */);
}

/**
 * Assigns color for the model. Tries to be peristence in different runs. It
 * uses timestamp as a source for hash that points to the ideal color. If that
 * color is already taken for another chart, it scans all possible colors and
 * selects the first available. If nothing helps, pink color as assigned as a
 * fallback.
 *
 * @param model model to assign color to.
 */
function setModelColor(model) {
  // Try to assing color bound to timestamp.
  if (model.information && model.information.timestamp) {
    var color = chartColors[
        Math.trunc(model.information.timestamp * 0.001) % chartColors.length];
    if (!takenColors.includes(color)) {
      modelColors.set(model, color);
      takenColors.push(color);
      return;
    }
  }
  // Just find avaiable.
  for (var i = 0; i < chartColors.length; i++) {
    if (!takenColors.includes(chartColors[i])) {
      modelColors.set(model, chartColors[i]);
      takenColors.push(chartColors[i]);
      return;
    }
  }

  // Nothing helps.
  modelColors.set(model, '#ffc0cb');
}

/**
 * Adds model to the view and refreshes everything.
 *
 * @param {object} model to add.
 */
function addModel(model) {
  models.push(model);

  setModelColor(model);

  refreshModels();
}

/**
 * Removes model from the view and refreshes everything.
 *
 * @param {object} model to add.
 */
function removeModel(model) {
  var index = models.indexOf(model);
  if (index == -1) {
    return;
  }

  models.splice(index, 1);

  index = takenColors.indexOf(modelColors.get(model));
  if (index != -1) {
    takenColors.splice(index, 1);
  }

  modelColors.delete(model);

  refreshModels();
}