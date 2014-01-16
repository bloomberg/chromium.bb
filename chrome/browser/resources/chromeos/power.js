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
  * @param {Array.<Object>} plots An array of objects with fields 'data' and
  *     'color'. The field 'data' is an array of samples to be plotted as a
  *     line graph with 'color'. The elements in the 'data' array are ordered
  *     corresponding to their sampling time in the argument 'tData'. Also,
  *     the number of elements in the data array should be the same as in the
  *     time array 'tData' above.
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
      printErrorText(loadTimeData.getString('timeAndPlotDataMismatch'));
      return;
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
    drawLine(xOrigin, yOrigin + height / 4,
             xOrigin + width - 1, yOrigin + height / 4,
             gridColor);
    drawLine(xOrigin, yOrigin + height / 2,
             xOrigin + width - 1, yOrigin + height / 2, gridColor);
    drawLine(xOrigin, yOrigin + 3 * height / 4,
             xOrigin + width - 1, yOrigin + 3 * height / 4,
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
      for (var i = 0; i < size; i++) {
        var xPos = xOrigin + Math.floor(i / (size - 1) * (width - 1));
        var val = yData[i];
        var yPos = yOrigin + height - 1 -
                   Math.round((val - yMin) / yValRange * (height - 1));
        if (i == 0) {
          ctx.moveTo(xPos, yPos);
        } else {
          ctx.lineTo(xPos, yPos);
        }
      }
      ctx.stroke();
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
      var yPos = yOrigin + height - 1 -
                 Math.round((val - yMin) / (yMax - yMin) * (height - 1));
      ctx.fillStyle = '#000';
      ctx.fillRect(x - 2, yPos - 2, 4, 4);

      // Draw the val to right of the intersection.
      var valStr = valueToString(val);
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

/**
 * Display the battery charge vs time on a line graph.
 *
 * @param {Array.<Object>} powerSupplyArray An array of objects with fields
 *     representing the battery charge, time when the charge measurement was
 *     taken, and whether there was external power connected at that time.
 */
function showBatteryChargeData(powerSupplyArray) {
  var tData = [];
  var chargePlot = [
    {
      color: '#0000FF',
      data: []
    }
  ];
  var dischargeRatePlot = [
    {
      color: '#FF0000',
      data: []
    }
  ];
  var minDischargeRate = 1000;  // A high unrealistic number to begin with.
  var maxDischargeRate = -1000; // A low unrealistic number to begin with.
  for (var i = 0; i < powerSupplyArray.length; i++) {
    var time = new Date(powerSupplyArray[i].time);
    tData[i] = time.toLocaleTimeString();

    chargePlot[0].data[i] = powerSupplyArray[i].battery_percent;

    var dischargeRate = powerSupplyArray[i].battery_discharge_rate;
    dischargeRatePlot[0].data[i] = dischargeRate;
    minDischargeRate = Math.min(dischargeRate, minDischargeRate);
    maxDischargeRate = Math.max(dischargeRate, maxDischargeRate);
  }
  if (minDischargeRate == maxDischargeRate) {
    // This means that all the samples had the same value. Hence, offset the
    // extremes by a bit so that the plot looks good.
    minDischargeRate -= 1;
    maxDischargeRate += 1;
  }

  var chargeCanvas = $('battery-charge-percentage-canvas');
  var dischargeRateCanvas = $('battery-discharge-rate-canvas');
  plotLineGraph(chargeCanvas, tData, chargePlot, 0.00, 100.00, 3);
  plotLineGraph(dischargeRateCanvas,
                tData,
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
