// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// This file contains helper methods to draw the stats timeline graphs.
// Each graph represents a series of stats report for a PeerConnection,
// e.g. 1234-0-ssrc-abcd123-bytesSent is the graph for the series of bytesSent
// for ssrc-abcd123 of PeerConnection 0 in process 1234.
// The graphs are drawn as CANVAS, grouped per report type per PeerConnection.
// Each group has an expand/collapse button and is collapsed initially.
//

<include src="data_series.js"/>
<include src="timeline_graph_view.js"/>

// Specifies which stats should be drawn on the 'bweCompound' graph and how.
var bweCompoundGraphConfig = {
  googAvailableSendBandwidth: {color: 'red'},
  googTargetEncBitrateCorrected: {color: 'purple'},
  googActualEncBitrate: {color: 'orange'},
  googRetransmitBitrate: {color: 'blue'},
  googTransmitBitrate: {color: 'green'},
};

// Converts the last entry of |srcDataSeries| from the total amount to the
// amount per second.
var totalToPerSecond = function(srcDataSeries) {
  var length = srcDataSeries.dataPoints_.length;
  if (length >= 2) {
    var lastDataPoint = srcDataSeries.dataPoints_[length - 1];
    var secondLastDataPoint = srcDataSeries.dataPoints_[length - 2];
    return (lastDataPoint.value - secondLastDataPoint.value) * 1000 /
           (lastDataPoint.time - secondLastDataPoint.time);
  }

  return 0;
};

// Converts the value of total bytes to bits per second.
var totalBytesToBitsPerSecond = function(srcDataSeries) {
  return totalToPerSecond(srcDataSeries) * 8;
};

// Specifies which stats should be converted before drawn and how.
// |convertedName| is the name of the converted value, |convertFunction|
// is the function used to calculate the new converted value based on the
// original dataSeries.
var dataConversionConfig = {
  packetsSent: {
    convertedName: 'packetsSentPerSecond',
    convertFunction: totalToPerSecond,
  },
  bytesSent: {
    convertedName: 'bitsSentPerSecond',
    convertFunction: totalBytesToBitsPerSecond,
  },
  packetsReceived: {
    convertedName: 'packetsReceivedPerSecond',
    convertFunction: totalToPerSecond,
  },
  bytesReceived: {
    convertedName: 'bitsReceivedPerSecond',
    convertFunction: totalBytesToBitsPerSecond,
  },
  // This is due to a bug of wrong units reported for googTargetEncBitrate.
  // TODO (jiayl): remove this when the unit bug is fixed.
  googTargetEncBitrate: {
    convertedName: 'googTargetEncBitrateCorrected',
    convertFunction: function (srcDataSeries) {
      var length = srcDataSeries.dataPoints_.length;
      var lastDataPoint = srcDataSeries.dataPoints_[length - 1];
      if (lastDataPoint.value < 5000)
        return lastDataPoint.value * 1000;
      return lastDataPoint.value;
    }
  }
};

var graphViews = {};
var dataSeries = {};

// Adds the stats report |singleReport| to the timeline graph for the given
// |peerConnectionElement| and |reportName|.
function drawSingleReport(peerConnectionElement, reportName, singleReport) {
  if (!singleReport || !singleReport.values)
    return;
  for (var i = 0; i < singleReport.values.length - 1; i = i + 2) {
    var rawLabel = singleReport.values[i];
    var rawValue = parseInt(singleReport.values[i + 1]);
    var rawDataSeriesId =
        peerConnectionElement.id + '-' + reportName + '-' + rawLabel;

    var finalDataSeriesId = rawDataSeriesId;
    var finalLabel = rawLabel;
    var finalValue = rawValue;
    // We need to convert the value if dataConversionConfig[rawLabel] exists.
    if (dataConversionConfig[rawLabel]) {
      // Updates the original dataSeries before the conversion.
      addDataSeriesPoint(rawDataSeriesId, singleReport.timestamp,
                         rawLabel, rawValue);

      // Convert to another value to draw on graph, using the original
      // dataSeries as input.
      finalValue = dataConversionConfig[rawLabel].convertFunction(
          dataSeries[rawDataSeriesId]);
      finalLabel = dataConversionConfig[rawLabel].convertedName;
      finalDataSeriesId =
          peerConnectionElement.id + '-' + reportName + '-' + finalLabel;
    }

    // Updates the final dataSeries to draw.
    addDataSeriesPoint(
        finalDataSeriesId, singleReport.timestamp, finalLabel, finalValue);

    // Updates the graph.
    var graphType = bweCompoundGraphConfig[finalLabel] ?
                    'bweCompound' : finalLabel;
    var graphViewId =
        peerConnectionElement.id + '-' + reportName + '-' + graphType;

    if (!graphViews[graphViewId]) {
      graphViews[graphViewId] = createStatsGraphView(peerConnectionElement,
                                                     reportName,
                                                     graphType);
      var date = new Date(singleReport.timestamp);
      graphViews[graphViewId].setDateRange(date, date);
    }
    // Adds the new dataSeries to the graphView. We have to do it here to cover
    // both the simple and compound graph cases.
    if (!graphViews[graphViewId].hasDataSeries(dataSeries[finalDataSeriesId]))
      graphViews[graphViewId].addDataSeries(dataSeries[finalDataSeriesId]);
    graphViews[graphViewId].updateEndDate();
  }
}

// Makes sure the TimelineDataSeries with id |dataSeriesId| is created,
// and adds the new data point to it.
function addDataSeriesPoint(dataSeriesId, time, label, value) {
  if (!dataSeries[dataSeriesId]) {
    dataSeries[dataSeriesId] = new TimelineDataSeries();
    if (bweCompoundGraphConfig[label]) {
      dataSeries[dataSeriesId].setColor(
          bweCompoundGraphConfig[label].color);
    }
  }
  dataSeries[dataSeriesId].addPoint(time, value);
}

// Ensures a div container to hold all stats graphs for a report type
// is created as a child of |peerConnectionElement|.
function ensureStatsGraphTopContainer(peerConnectionElement, reportName) {
  var containerId = peerConnectionElement.id + '-' +
                    reportName + '-graph-container';
  var container = $(containerId);
  if (!container) {
    container = document.createElement('div');
    container.id = containerId;
    container.className = 'stats-graph-container-collapsed';

    peerConnectionElement.appendChild(container);
    container.innerHTML =
        '<button>Expand/Collapse Graphs for ' + reportName + '</button>';

    // Expands or collapses the graphs on click.
    container.childNodes[0].addEventListener('click', function(event) {
      var element = event.target.parentNode;
      if (element.className == 'stats-graph-container-collapsed')
        element.className = 'stats-graph-container';
      else
        element.className = 'stats-graph-container-collapsed';
    });
  }
  return container;
}

// Creates the container elements holding a timeline graph
// and the TimelineGraphView object.
function createStatsGraphView(peerConnectionElement, reportName, statsName) {
  var topContainer = ensureStatsGraphTopContainer(peerConnectionElement,
                                                  reportName);

  var graphViewId =
      peerConnectionElement.id + '-' + reportName + '-' + statsName;
  var divId = graphViewId + '-div';
  var canvasId = graphViewId + '-canvas';
  var container = document.createElement("div");
  container.className = 'stats-graph-sub-container';

  topContainer.appendChild(container);
  container.innerHTML = '<div>' + statsName + '</div>' +
      '<div id=' + divId + '><canvas id=' + canvasId + '></canvas></div>';
  if (statsName == 'bweCompound') {
      container.insertBefore(createBweCompoundLegend(peerConnectionElement,
                                                     reportName),
                             $(divId));
  }
  return new TimelineGraphView(divId, canvasId);
}

// Creates the legend section for the bweCompound graph.
// Returns the legend element.
function createBweCompoundLegend(peerConnectionElement, reportName) {
  var legend = document.createElement('div');
  for (var prop in bweCompoundGraphConfig) {
    var div = document.createElement('div');
    legend.appendChild(div);
    div.innerHTML = '<input type=checkbox checked></input>' + prop;
    div.style.color = bweCompoundGraphConfig[prop].color;
    div.dataSeriesId = peerConnectionElement.id + '-' + reportName + '-' + prop;
    div.graphViewId =
        peerConnectionElement.id + '-' + reportName + '-bweCompound';
    div.firstChild.addEventListener('click', function(event) {
        var target = dataSeries[event.target.parentNode.dataSeriesId];
        target.show(event.target.checked);
        graphViews[event.target.parentNode.graphViewId].repaint();
    });
  }
  return legend;
}
