/*
  Copyright (c) 2012 The Chromium Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

/**
 * @fileoverview Handles drawing a general Chrome Endure graph.
 */

document.title = Config.title + ' - ' + Config.buildslave;

var unitsX = 'unitsX';
var unitsY = 'unitsY';
var unitsYOther = null;
var graphList = [];
var revisionNumbers = [];
var graphDataOtherRows = null;

var eventRows = null;
var eventTypes = [];
var eventInfo = null;

var params = ParseParams();

/**
 * Encapsulates a *-summary.dat file.
 * @constructor
 *
 * @param {string} data Raw data from a *-summary.dat file.
 */
function Rows(data) {
  this.rows = data.split('\n');
  this.length = this.rows.length;
}

/**
 * Returns the row at the given index.
 *
 * @param {number} i The index of a row of data from the *-summary.dat file.
 * @return {Object} An object representing a row of data from the input file.
 */
Rows.prototype.get = function(i) {
  if (!this.rows[i].length) return null;
  var row = jsonToJs(this.rows[i]);
  row.revision = isNaN(row['rev']) ? row['rev'] : parseInt(row['rev']);
  return row;
};

/**
 * Gets the current URL, but without the 'lookout' parameter.
 *
 * @return {string} The current URL, but without the 'lookout' parameter.
 */
function get_url() {
  new_url = window.location.href;
  new_url = new_url.replace(/\&lookout=1/, '');
  return new_url;
}

/**
 * Reports an error message on the webpage.
 *
 * @param {string} error An error message to display on the page.
 */
function reportError(error) {
  document.getElementById('output').innerHTML = '<p>' + error + '</p>';
}

/**
 * Converts a JSON string into a Javascript object.
 *
 * @param {string} data A string in JSON format.
 * @return {Object} A Javascript object computed from the JSON string.
 */
function jsonToJs(data) {
  return eval('(' + data + ')')
}

/**
 * Causes the page to navigate to another graph.
 *
 * @param {string} graph The name of the graph to which to navigate.
 */
function goTo(graph) {
  params.graph = graph;
  window.location.href = MakeURL(params);
}

/**
 * Returns a function that will navigate the page to another graph.
 *
 * @param {string} graph The name of the graph to which to navigate.
 * @return {Function} A function that will navigate the page to another graph.
 */
function goToClosure(graph) {
  return function(){goTo(graph)};
}

/**
 * Changes the event being overlayed on the graph.
 *
 * @param {string} eventName The name of the event to overlay on the graph.
 */
function changeEventCompare(eventName) {
  delete params.revisionOther;
  delete params.graphOther;
  if (eventName == 'None') {
    delete params.event;
    window.location.href = MakeURL(params);
  } else {
    params.event = eventName;
    window.location.href = MakeURL(params);
  }
}

/**
 * Changes the other measurement being overlayed on top of an original line on
 * the graph.
 *
 * @param {string} graphName The name of the other graph to overlay on top of
 *     the existing graph.
 */
function changeMeasurementCompare(graphName) {
  delete params.revisionOther;
  delete params.event;
  if (graphName == 'None') {
    delete params.graphOther;
    window.location.href = MakeURL(params);
  } else {
    params.graphOther = graphName;
    window.location.href = MakeURL(params);
  }
}

/**
 * Changes the number of the other revision to compare against on the graph.
 *
 * @param {string} revision The revision number of the other line to plot on
 *     the graph.
 */
function changeRevisionCompare(revision) {
  delete params.graphOther;
  delete params.event;
  if (revision == 'None') {
    delete params.revisionOther;
    window.location.href = MakeURL(params);
  } else {
    params.revisionOther = revision;
    window.location.href = MakeURL(params);
  }
}

/**
 * Changes the displayed revision number of the graph line.
 *
 * @param {string} revision The revision number of the graph to display.
 */
function changeRevision(revision) {
  delete params.revisionOther;
  delete params.graphOther;
  delete params.event;
  params.revision = revision;
  window.location.href = MakeURL(params);
}

/**
 * Initializes the UI for changing the revision number of the displayed graph.
 */
function initRevisionOptions() {
  var html = '<table cellpadding=5><tr><td>';
  html += '<b>Chrome revision:</b>&nbsp;';
  html += '<select onchange=\"changeRevision(this.value)\">';
  for (var i = 0; i < revisionNumbers.length; ++i) {
    html += '<option id=\"r' + revisionNumbers[i] + '\"';
    if (revisionNumbers[i] == params.revision)
      html += 'selected=\"true\"';
    html += '>' + revisionNumbers[i] + '</option>';
  }
  html += '</select></td></tr></table>';

  document.getElementById('revisions').innerHTML = html;
}

/**
 * Initializes the UI for changing what is compared against the current line
 * on the displayed graph.
 */
function initComparisonOptions() {
  var html = '<table cellpadding=5>';
  html += '<tr><td><b>Compare with (select one):</b></td></tr>';

  html += '<tr><td>&nbsp;&nbsp;&nbsp;Another run:&nbsp;';
  html += '<select onchange=\"changeRevisionCompare(this.value)\">';
  html += '<option selected=\"true\">None</option>';
  for (var i = 0; i < revisionNumbers.length; ++i) {
    html += '<option id=\"r' + revisionNumbers[i] + '\"';
    if (revisionNumbers[i] == params.revisionOther)
      html += 'selected=\"true\"';
    html += '>' + revisionNumbers[i] + '</option>';
  }
  html += '</select></td></tr>'

  html += '<tr><td>&nbsp;&nbsp;&nbsp;Another measurement of same run:&nbsp;';
  html += '<select onchange=\"changeMeasurementCompare(this.value)\">';
  html += '<option selected=\"true\">None</option>';
  for (var i = 0; i < graphList.length; ++i) {
    var graph = graphList[i];
    html += '<option id=\"r' + graph.name + '\"';
    if (graph.name == params.graphOther)
      html += 'selected=\"true\"';
    html += '>' + graph.name + '</option>';
  }
  html += '</select></td></tr>';

  html += '<tr><td>&nbsp;&nbsp;&nbsp;Event overlay:&nbsp;';
  if (eventTypes.length >= 1) {
    html += '<select onchange=\"changeEventCompare(this.value)\">';
    html += '<option selected=\"true\">None</option>';
    for (var i = 0; i < eventTypes.length; ++i) {
      var eventType = eventTypes[i];
      html += '<option id=\"' + eventType + '\"';
      if (eventType == params.event)
        html += 'selected=\"true\"';
      html += '>' + eventType + '</option>';
    }
    html += '</select>';
  } else {
    html += '&nbsp;<i><font size=-1>No events for this revision</font></i>';
  }
  html += '</td></tr></table>';

  document.getElementById('comparisons').innerHTML = html;
}

/**
 * Initializes the UI for the tabs at the top of a graph to change the displayed
 * line.
 */
function initPlotSwitcher(tabs) {
  var switcher = document.getElementById('switcher');
  for (var i = 0; i < tabs.length; ++i) {
    var is_selected = tabs[i] == params.graph;
    var tab = document.createElement(is_selected ? 'span' : 'a');
    tab.appendChild(document.createTextNode(tabs[i] + ' '));
    if (!is_selected)
      tab.addEventListener('click', goToClosure(tabs[i]), false);
    switcher.appendChild(tab);
  }
}

/**
 * Adds data to existing arrays indicating what data should be plotted.
 *
 * @param {number} revisionNum The revision number of the data to plot.
 * @param {Rows} dataRows The |Rows| object containing the plot data.
 * @param {Array} plotData A list of data lines to plot, to which new data will
 *     be appended.
 * @param {Array} dataDescriptions A list of string descriptions corresponding
 *     to data lines in |plotData|, to which new data will be appended.
 * @return {Object} A row object specified by {@code revisionNum} on success,
 *     otherwise returns null.
 */
function addToPlotData(revisionNum, dataRows, plotData, dataDescriptions) {
  // Get data for the revision number(s) to plot.
  var found = false;
  for (var i = 0; i < dataRows.length; ++i) {
    var row = dataRows.get(i);
    if (row && row.revision == revisionNum) {
      found = true;
      break;
    }
  }
  if (!found) {
    return null;
  }

  if (row.stack) {
    if (!row.stack_order) {
      reportError('No stack order was specified.');
      return null;
    }
    var traceList = row.stack_order;
  } else {
    // Identify the (single) trace name associated with this revision.
    var traceName = null;
    for (var t in row.traces) {
      if (traceName) {
        reportError('Only one trace per revision is supported for ' +
                    'non-stacked graphs.');
        return null;
      }
      traceName = t;
    }
    var traceList = [traceName];
  }

  var lines = [];
  for (var i = 0, traceName; traceName = traceList[i]; ++i) {
    var trace = row.traces[traceName];
    if (!trace) {
      reportError('No specified trace was found.');
      return null;
    }

    var points = [];
    for (var j = 0, point; point = trace[j]; ++j) {
      points.push([parseFloat(point[0]), parseFloat(point[1])]);
    }
    lines.push(points);
    dataDescriptions.push(traceName + ' [r' + row.revision + ']');
  }

  if (row.stack) {
    lines = graphUtils.stackFrontToBack(graphUtils.interpolate(lines));
  }

  for (var i = 0, line; line = lines[i]; ++i) {
    plotData.push(line);
  }

  return row;
}

/**
 * Callback for when a *-summary.dat data file has been read.
 *
 * @param {string} data The string data from the inputted text file.
 * @param {string} error A string error message, in case an error occurred
 *     during the file read.
 */
function receivedSummary(data, error) {
  if (error) {
    reportError(error);
    return;
  }

  var errorMessages = '';
  var rows = new Rows(data);

  // Build and order a list of revision numbers.
  revisionNumbers = [];
  for (var i = 0; i < rows.length; ++i) {
    var row = rows.get(i);
    if (!row)
      continue;
    revisionNumbers.push(row.revision);
  }
  revisionNumbers.sort(
      function(a, b) { return parseInt(a, 10) - parseInt(b, 10) });

  // Get the revision number to plot.
  if (!('revision' in params) || params.revision == '') {
    if (revisionNumbers.length >= 2 && 'lookout' in params) {
      // Since the last graph (test run) might still be in progress, get the
      // second-to-last graph to display on the summary page.  That one
      // is assumed to have finished running to completion.
      params.revision = revisionNumbers[revisionNumbers.length-2];
    } else {
      if (revisionNumbers.length >= 1) {
        params.revision = revisionNumbers[revisionNumbers.length-1];
      } else {
        reportError('No revision information to plot.');
        return;
      }
    }
  }

  var plotData = [];  // plotData is a list of graph lines; each graph line is
                      // a list of points; each point is a list of 2 values,
                      // representing the (x, y) pair.
  var dataDescriptions = [];

  var row = addToPlotData(params.revision, rows, plotData, dataDescriptions);
  if (!row) {
    errorMessages += 'No data for the specified revision.<br>';
  }
  // From index {@code plotData.length} onwards, any graph lines in
  // {@code plotData} are considered to be part of a second set of graphs.
  var graphsOtherStartIndex = plotData.length;

  var rowOther = null;
  if ('revisionOther' in params) {
    rowOther = addToPlotData(params.revisionOther, rows, plotData,
                             dataDescriptions);
    if (!rowOther)
      errorMessages += 'No data for the revision to compare against.<br>';
  }

  if ('graphOther' in params) {
    rowOther = addToPlotData(params.revision, graphDataOtherRows, plotData,
                             dataDescriptions);
    if (rowOther) {
      for (var i = 0; i < graphList.length; ++i) {
        if (graphList[i].name == params.graphOther) {
          unitsYOther = graphList[i].units;
          break;
        }
      }
    } else {
      errorMessages += 'No data for the measurement to compare against.<br>';
    }
  }

  // Identify the events for the current revision.
  if (eventRows) {
    for (var index = 0; index < eventRows.length; ++index) {
      var info = eventRows.get(index);
      if (params.revision == info['rev']) {
        eventInfo = info;
        break;
      }
    }
    if (eventInfo != null) {
      for (var key in eventInfo['events']) {
        eventTypes.push(key);
      }
    }
  }

  // Get data for the events to display, if one was requested in the params.
  var eventNameToPlot = null;
  var eventInfoToPlot = null;
  if ('event' in params && eventInfo != null) {
    for (var key in eventInfo['events']) {
      if (key == params['event']) {
        eventInfoToPlot = eventInfo['events'][key];
        eventNameToPlot = key;
      }
    }
  }

  // Draw everything.
  if (errorMessages == '') {
    var plotter = new Plotter(
        plotData,
        dataDescriptions,
        eventNameToPlot, eventInfoToPlot,
        unitsX, unitsY, unitsYOther, graphsOtherStartIndex,
        document.getElementById('output'),
        'lookout' in params,
        !!row.stack,
        rowOther && !!rowOther.stack);

    plotter.plot();
  } else {
    errorMessages = '<br><br><br><table border=2 cellpadding=5><tr><td>' +
                    errorMessages + '</td></tr></table><br><br>';
    document.getElementById('output').innerHTML = errorMessages;
  }

  if (!('lookout' in params)) {
    initRevisionOptions();
    initComparisonOptions();
  }
}

/**
 * Callback for when a second *-summary.dat data file has been read, in the
 * event that a second graph line is being overlayed on top of an existing
 * graph line.
 *
 * @param {string} data The string data from the inputted text file.
 * @param {string} error A string error message, in case an error occurred
 *     during the file read.
 */
function receivedSummaryGraphOther(data, error) {
  if (error) {
    reportError(error);
    return;
  }

  graphDataOtherRows = new Rows(data);
  Fetch(escape(params.graph) + '-summary.dat', receivedSummary);
}

/**
 * Callback for when an event info file has been read.
 *
 * @param {string} data The string data from the inputted text file.
 * @param {string} error A string error message, in case an error occurred
 *     during the file read.
 */
function receivedEvents(data, error) {
  if (!error)
    eventRows = new Rows(data);
  fetchSummary();
}

/**
 * Callback for when a graphs.dat data file has been read.
 *
 * @param {string} data The string data from the inputted text file.
 * @param {string} error A string error message, in case an error occurred
 *     during the file read.
 */
function receivedGraphList(data, error) {
  if (error) {
    reportError(error);
    return;
  }
  graphList = jsonToJs(data);

  if (!('graph' in params) || params.graph == '')
    if (graphList.length > 0)
      params.graph = graphList[0].name

  // Add a selection tab for each graph, and find the units for the selected
  // one while we're at it.
  tabs = [];
  for (var index = 0; index < graphList.length; ++index) {
    var graph = graphList[index];
    tabs.push(graph.name);
    if (graph.name == params.graph) {
      unitsX = graph.units_x;
      unitsY = graph.units;
    }
  }
  initPlotSwitcher(tabs);

  fetchEvents();
}

/**
 * Starts fetching a *-summary.dat file.
 */
function fetchSummary() {
  if ('graphOther' in params) {
    // We need to overlay a second graph over the first one, so we need to
    // fetch that summary data too.  Do it first.
    Fetch(escape(params.graphOther) + '-summary.dat',
          receivedSummaryGraphOther);
  } else {
    Fetch(escape(params.graph) + '-summary.dat',
          receivedSummary);
  }
}

/**
 * Starts fetching an event info file.
 */
function fetchEvents() {
  Fetch('_EVENT_-summary.dat', receivedEvents);
}

/**
 * Starts fetching a graphs.dat file.
 */
function fetchGraphList() {
  Fetch('graphs.dat', receivedGraphList);
}

window.addEventListener('load', fetchGraphList, false);
