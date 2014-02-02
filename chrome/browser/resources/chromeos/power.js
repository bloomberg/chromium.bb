// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Plot a line graph of data versus time on a HTML canvas element.
 *
 * @param {HTMLCanvasElement} canvas The canvas on which the line graph is
 *     drawn.
 * @param {Array.<number>} tData The time (in seconds) in the past when the
 *     corresponding data in plots was sampled.
 * @param {Array.<{data: Array.<number>, color: string}>} plots An
 *     array of plots to plot on the canvas. The field 'data' of a plot is an
 *     array of samples to be plotted as a line graph with color speficied by
 *     the field 'color'. The elements in the 'data' array are ordered
 *     corresponding to their sampling time in the argument 'tData'. Also, the
 *     number of elements in the 'data' array should be the same as in the time
 *     array 'tData' above.
 * @param {number} yMin Minimum bound of y-axis
 * @param {number} yMax Maximum bound of y-axis.
 * @param {integer} yPrecision An integer value representing the number of
 *     digits of precision the y-axis data should be printed with.
 */
function plotLineGraph(canvas, tData, plots, yMin, yMax, yPrecision) {
  var textFont = '12px Arial';
  var textHeight = 12;
  var padding = 5;  // Pixels
  var errorOffsetPixels = 15;
  var gridColor = '#ccc';
  var ctx = canvas.getContext('2d');
  var size = tData.length;

  function drawText(text, x, y) {
    ctx.font = textFont;
    ctx.fillStyle = '#000';
    ctx.fillText(text, x, y);
  }

  function printErrorText(text) {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    drawText(text, errorOffsetPixels, errorOffsetPixels);
  }

  if (size < 2) {
    printErrorText(loadTimeData.getString('notEnoughDataAvailableYet'));
    return;
  }

  for (var count = 0; count < plots.length; count++) {
    if (plots[count].data.length != size) {
      throw new Error('Mismatch in time and plot data.');
    }
  }

  function valueToString(value) {
    return Number(value).toPrecision(yPrecision);
  }

  function getTextWidth(text) {
    ctx.font = textFont;
    // For now, all text is drawn to the left of vertical lines, or centered.
    // Add a 2 pixel padding so that there is some spacing between the text
    // and the vertical line.
    return Math.round(ctx.measureText(text).width) + 2;
  }

  function drawHighlightText(text, x, y, color) {
    ctx.strokeStyle = '#000';
    ctx.strokeRect(x, y - textHeight, getTextWidth(text), textHeight);
    ctx.fillStyle = color;
    ctx.fillRect(x, y - textHeight, getTextWidth(text), textHeight);
    ctx.fillStyle = '#fff';
    ctx.fillText(text, x, y);
  }

  function drawLine(x1, y1, x2, y2, color) {
    ctx.save();
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.strokeStyle = color;
    ctx.stroke();
    ctx.restore();
  }

  // The strokeRect method of the 2d context of a canvas draws a bounding
  // rectangle with an offset origin and greater dimensions. Hence, use this
  // function to draw a rect at the desired location with desired dimensions.
  function drawRect(x, y, width, height, color) {
    drawLine(x, y, x + width - 1, y, color);
    drawLine(x, y, x, y + height - 1, color);
    drawLine(x, y + height - 1, x + width - 1, y + height - 1, color);
    drawLine(x + width - 1, y, x + width - 1, y + height - 1, color);
  }

  var yMinStr = valueToString(yMin);
  var yMaxStr = valueToString(yMax);
  var yHalfStr = valueToString((yMax + yMin) / 2);
  var yMinWidth = getTextWidth(yMinStr);
  var yMaxWidth = getTextWidth(yMaxStr);
  var yHalfWidth = getTextWidth(yHalfStr);

  var xMinStr = tData[0];
  var xMaxStr = tData[size - 1];
  var xMinWidth = getTextWidth(xMinStr);
  var xMaxWidth = getTextWidth(xMaxStr);

  var xOrigin = padding + Math.max(yMinWidth,
                                   yMaxWidth,
                                   Math.round(xMinWidth / 2));
  var yOrigin = padding + textHeight;
  var width = canvas.width - xOrigin - Math.floor(xMaxWidth / 2) - padding;
  if (width < size) {
    canvas.width += size - width;
    width = size;
  }
  var height = canvas.height - yOrigin - textHeight - padding;

  function drawPlots() {
    // Start fresh.
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Draw the bounding rectangle.
    drawRect(xOrigin, yOrigin, width, height, gridColor);

    // Draw the x and y bound values.
    drawText(yMaxStr, xOrigin - yMaxWidth, yOrigin + textHeight);
    drawText(yMinStr, xOrigin - yMinWidth, yOrigin + height);
    drawText(xMinStr, xOrigin - xMinWidth / 2, yOrigin + height + textHeight);
    drawText(xMaxStr,
             xOrigin + width - xMaxWidth / 2,
             yOrigin + height + textHeight);

    // Draw y-level (horizontal) lines.
    drawLine(xOrigin + 1, yOrigin + height / 4,
             xOrigin + width - 2, yOrigin + height / 4,
             gridColor);
    drawLine(xOrigin + 1, yOrigin + height / 2,
             xOrigin + width - 2, yOrigin + height / 2, gridColor);
    drawLine(xOrigin + 1, yOrigin + 3 * height / 4,
             xOrigin + width - 2, yOrigin + 3 * height / 4,
             gridColor);

    // Draw half-level value.
    drawText(yHalfStr,
             xOrigin - yHalfWidth,
             yOrigin + height / 2 + textHeight / 2);

    // Draw the plots.
    var yValRange = yMax - yMin;
    for (var count = 0; count < plots.length; count++) {
      var plot = plots[count];
      var yData = plot.data;
      ctx.strokeStyle = plot.color;
      ctx.beginPath();
      var beginPath = true;
      for (var i = 0; i < size; i++) {
        var val = yData[i];
        if (typeof val === 'string') {
          // Stroke the plot drawn so far and begin a fresh plot.
          ctx.stroke();
          ctx.beginPath();
          beginPath = true;
          continue;
        }
        var xPos = xOrigin + Math.floor(i / (size - 1) * (width - 1));
        var yPos = yOrigin + height - 1 -
                   Math.round((val - yMin) / yValRange * (height - 1));
        if (beginPath) {
          ctx.moveTo(xPos, yPos);
          // A simple move to does not print anything. Hence, draw a little
          // square here to mark a beginning.
          ctx.fillStyle = '#000';
          ctx.fillRect(xPos - 1, yPos - 1, 2, 2);
          beginPath = false;
        } else {
          ctx.lineTo(xPos, yPos);
          if (i === size - 1 || typeof yData[i + 1] === 'string') {
            // Draw a little square to mark an end to go with the start
            // markers from above.
            ctx.fillStyle = '#000';
            ctx.fillRect(xPos - 1, yPos - 1, 2, 2);
          }
        }
      }
      ctx.stroke();
    }

    // Paint the missing time intervals with |gridColor|.
    // Pick one of the plots to look for missing time intervals.
    var inMissingInterval = false;
    var intervalStart;
    for (var i = 0; i < size; i++) {
      if (typeof plots[0].data[i] === 'string') {
        if (!inMissingInterval) {
          inMissingInterval = true;
          // The missing interval should actually start from the previous
          // sample.
          intervalStart = Math.max(i - 1, 0);
        }
      } else if (inMissingInterval) {
        inMissingInterval = false;
        var xLeft = xOrigin +
            Math.floor(intervalStart / (size - 1) * (width - 1));
        var xRight = xOrigin + Math.floor(i / (size - 1) * (width - 1));
        ctx.fillStyle = gridColor;
        // The x offsets below are present so that the blank space starts
        // and ends between two valid samples.
        ctx.fillRect(xLeft + 1, yOrigin, xRight - xLeft - 2, height - 1);
      }
    }
  }

  function drawTimeGuide(tDataIndex) {
    var x = xOrigin + tDataIndex / (size - 1) * (width - 1);
    drawLine(x, yOrigin, x, yOrigin + height - 1, '#000');
    drawText(tData[tDataIndex],
             x - getTextWidth(tData[tDataIndex]) / 2,
             yOrigin - 2);

    for (var count = 0; count < plots.length; count++) {
      var yData = plots[count].data;

      // Draw small black square on the plot where the time guide intersects
      // it.
      var val = yData[tDataIndex];
      var yPos, valStr;
      if (typeof val === 'string') {
        yPos = yOrigin + Math.round(height / 2);
        valStr = val;
      } else {
        yPos = yOrigin + height - 1 -
            Math.round((val - yMin) / (yMax - yMin) * (height - 1));
        valStr = valueToString(val);
      }
      ctx.fillStyle = '#000';
      ctx.fillRect(x - 2, yPos - 2, 4, 4);

      // Draw the val to right of the intersection.
      var yLoc;
      if (yPos - textHeight / 2 < yOrigin) {
        yLoc = yOrigin + textHeight;
      } else if (yPos + textHeight / 2 >= yPos + height) {
        yLoc = yOrigin + height - 1;
      } else {
        yLoc = yPos + textHeight / 2;
      }
      drawHighlightText(valStr, x + 5, yLoc, plots[count].color);
    }
  }

  function onMouseOverOrMove(event) {
    drawPlots();

    var boundingRect = canvas.getBoundingClientRect();
    var x = event.clientX - boundingRect.left;
    var y = event.clientY - boundingRect.top;
    if (x < xOrigin || x >= xOrigin + width ||
        y < yOrigin || y >= yOrigin + height) {
      return;
    }

    if (width == size) {
      drawTimeGuide(x - xOrigin);
    } else {
      drawTimeGuide(Math.round((x - xOrigin) / (width - 1) * (size - 1)));
    }
  }

  function onMouseOut(event) {
    drawPlots();
  }

  drawPlots();
  canvas.addEventListener('mouseover', onMouseOverOrMove);
  canvas.addEventListener('mousemove', onMouseOverOrMove);
  canvas.addEventListener('mouseout', onMouseOut);
}

var sleepSampleInterval = 30 * 1000; // in milliseconds.
var sleepText = loadTimeData.getString('systemSuspended');

/**
 * Add samples in |sampleArray| to individual plots in |plots|. If the system
 * resumed from a sleep/suspend, then "suspended" sleep samples are added to
 * the plot for the sleep duration.
 *
 * @param {Array.<{data: Array.<number>, color: string}>} plots An
 *     array of plots to plot on the canvas. The field 'data' of a plot is an
 *     array of samples to be plotted as a line graph with color speficied by
 *     the field 'color'. The elements in the 'data' array are ordered
 *     corresponding to their sampling time in the argument 'tData'. Also, the
 *     number of elements in the 'data' array should be the same as in the time
 *     array 'tData' below.
 * @param {Array.<number>} tData The time (in seconds) in the past when the
 *     corresponding data in plots was sampled.
 * @param {Array.<number>} sampleArray The array of samples wherein each
 *     element corresponds to the individual plot in |plots|.
 * @param {number} sampleTime Time in milliseconds since the epoch when the
 *     samples in |sampleArray| were captured.
 * @param {number} previousSampleTime Time in milliseconds since the epoch
 *     when the sample prior to the current sample was captured.
 * @param {Array.<{time: number, sleepDuration: number}>} systemResumedArray An
 *     array objects corresponding to system resume events. The 'time' field is
 *     for the time in milliseconds since the epoch when the system resumed. The
 *     'sleepDuration' field is for the time in milliseconds the system spent
 *     in sleep/suspend state.
 */
function addTimeDataSample(plots, tData, sampleArray,
                           sampleTime, previousSampleTime,
                           systemResumedArray) {
  for (var i = 0; i < plots.length; i++) {
    if (plots[i].data.length != tData.length) {
      throw new Error('Mismatch in time and plot data.');
    }
  }

  var time;
  if (tData.length == 0) {
    time = new Date(sampleTime);
    tData[0] = time.toLocaleTimeString();
    for (var i = 0; i < plots.length; i++) {
      plots[i].data[0] = sampleArray[i];
    }
    return;
  }

  for (var i = 0; i < systemResumedArray.length; i++) {
    var resumeTime = systemResumedArray[i].time;
    var sleepDuration = systemResumedArray[i].sleepDuration;
    var sleepStartTime = resumeTime - sleepDuration;
    if (resumeTime < sampleTime && sleepStartTime > previousSampleTime) {
      // Add sleep samples for every |sleepSampleInterval|.
      var sleepSampleTime = sleepStartTime;
      while (sleepSampleTime < resumeTime) {
        time = new Date(sleepSampleTime);
        tData.push(time.toLocaleTimeString());
        for (var j = 0; j < plots.length; j++) {
          plots[j].data.push(sleepText);
        }
        sleepSampleTime += sleepSampleInterval;
      }
    }
  }

  time = new Date(sampleTime);
  tData.push(time.toLocaleTimeString());
  for (var i = 0; i < plots.length; i++) {
    plots[i].data.push(sampleArray[i]);
  }
}

/**
 * Display the battery charge vs time on a line graph.
 *
 * @param {Array.<{time: number,
 *                 batteryPercent: number,
 *                 batteryDischargeRate: number,
 *                 externalPower: number}>} powerSupplyArray An array of objects
 *     with fields representing the battery charge, time when the charge
 *     measurement was taken, and whether there was external power connected at
 *     that time.
 * @param {Array.<{time: ?, sleepDuration: ?}>} systemResumedArray An array
 *     objects with fields 'time' and 'sleepDuration'. Each object corresponds
 *     to a system resume event. The 'time' field is for the time in
 *     milliseconds since the epoch when the system resumed. The 'sleepDuration'
 *     field is for the time in milliseconds the system spent in sleep/suspend
 *     state.
 */
function showBatteryChargeData(powerSupplyArray, systemResumedArray) {
  var chargeTimeData = [];
  var chargePlot = [
    {
      color: '#0000FF',
      data: []
    }
  ];
  var dischargeRateTimeData = [];
  var dischargeRatePlot = [
    {
      color: '#FF0000',
      data: []
    }
  ];
  var minDischargeRate = 1000;  // A high unrealistic number to begin with.
  var maxDischargeRate = -1000; // A low unrealistic number to begin with.
  for (var i = 0; i < powerSupplyArray.length; i++) {
    var j = Math.max(i - 1, 0);

    addTimeDataSample(chargePlot, chargeTimeData,
                      [powerSupplyArray[i].batteryPercent],
                      powerSupplyArray[i].time,
                      powerSupplyArray[j].time,
                      systemResumedArray);

    var dischargeRate = powerSupplyArray[i].batteryDischargeRate;
    minDischargeRate = Math.min(dischargeRate, minDischargeRate);
    maxDischargeRate = Math.max(dischargeRate, maxDischargeRate);
    addTimeDataSample(dischargeRatePlot,
                      dischargeRateTimeData,
                      [dischargeRate],
                      powerSupplyArray[i].time,
                      powerSupplyArray[j].time,
                      systemResumedArray);
  }
  if (minDischargeRate == maxDischargeRate) {
    // This means that all the samples had the same value. Hence, offset the
    // extremes by a bit so that the plot looks good.
    minDischargeRate -= 1;
    maxDischargeRate += 1;
  }

  var chargeCanvas = $('battery-charge-percentage-canvas');
  var dischargeRateCanvas = $('battery-discharge-rate-canvas');
  plotLineGraph(chargeCanvas, chargeTimeData, chargePlot, 0.00, 100.00, 3);
  plotLineGraph(dischargeRateCanvas,
                dischargeRateTimeData,
                dischargeRatePlot,
                minDischargeRate,
                maxDischargeRate,
                3);
}

function requestBatteryChargeData() {
  chrome.send('requestBatteryChargeData');
}

var powerUI = {
  showBatteryChargeData: showBatteryChargeData
};

document.addEventListener('DOMContentLoaded', function() {
  requestBatteryChargeData();
  $('battery-charge-reload-button').onclick = requestBatteryChargeData;
});
