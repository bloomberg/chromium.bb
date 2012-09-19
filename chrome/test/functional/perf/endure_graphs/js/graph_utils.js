/*
  Copyright (c) 2012 The Chromium Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

/**
 * @fileoverview Collection of functions which operate on graph data.
 */

var graphUtils = window['graphUtils'] || {};

/**
 * Interpolate given multiple lines of graphs, and returns the lines of
 * the graphs where each line has the same number of points and x coordinates.
 *
 * For example,
 * <pre>
 * [[[0, 1], [2, 3]],  // 1st line
 *  [[1, 3]]]  // 2nd line
 * </pre>
 * will be converted to
 * <pre>
 * [[[0, 1], [1, 2], [2, 3]],  // [1, 2] is interpolated.
 *  [[0, 0], [1, 3], [2, 0]]]  // [0, 0] and [2, 0] are interpolated.
 * </pre>
 * where every line has points at x=0, 1 and 2.
 * Interpolated data points are marked with a property
 * {@code point.interpolated == true}.
 *
 * @param {Array.<Array.<Array.<number>>>} plotData List of arrays that
 *     represent individual lines. The line itself is an Array of points.
 * @return {Array.<Array.<Array.<number>>>} An interpolated {@code plotData}.
 *     The original {@code plotData} is not affected.
 */
graphUtils.interpolate = function(plotData) {
  var interpolated = [];  // resulting interpolated {@code plotData}
  var unconsumed = [];  // indices to unconsumed points in {@code plotData}
  for (var i = 0; i < plotData.length; ++i) {
    interpolated.push([]);
    unconsumed.push(0);
  }

  // Returns the next x-coordinate to interpolate if any, or null.
  function nextX() {
    var index = null;
    for (var i = 0; i < unconsumed.length; ++i) {
      if (0 <= unconsumed[i] && unconsumed[i] < plotData[i].length &&
          (index == null ||
           plotData[i][unconsumed[i]][0] <
           plotData[index][unconsumed[index]][0])) {
        index = i;
      }
    }
    return index == null ? null : plotData[index][unconsumed[index]][0];
  }

  for (var x = nextX(); x != null; x = nextX()) {  // for all x
    for (var i = 0; i < plotData.length; ++i) {  // for all lines
      var y = 0;
      var hasPoint = false;
      if (0 <= unconsumed[i] && unconsumed[i] < plotData[i].length) {
        var p = plotData[i][unconsumed[i]];
        if (p[0] <= x) {
          y = p[1];  // The original line has a point at x.
          hasPoint = true;
        } else if (unconsumed[i] == 0) {
          y = 0;  // y = 0 before the first point
        } else {
          // Interpolate a point.
          var p0 = plotData[i][unconsumed[i] - 1];
          y = (x - p0[0]) / (p[0] - p0[0]) * (p[1] - p0[1]) + p0[1];
        }
      }  // else y = 0 because it's out of range.

      var point = [x, y];
      if (!hasPoint) {
        point.interpolated = true;
      }
      interpolated[i].push(point);
    }

    // Consume {@code plotData} by incrementing indices in {@code unconsumed}.
    for (var i = 0; i < unconsumed.length; ++i) {
      if (0 <= unconsumed[i] && unconsumed[i] < plotData[i].length &&
          plotData[i][unconsumed[i]][0] <= x) {
        ++unconsumed[i];
      }
    }
  }

  return interpolated;
};

/**
 * Creates and returns a set of stacked graphs, assuming the given
 * {@code plotData} is interpolated by {@code graphUtils.interpolate}.
 *
 * For example,
 * <pre>
 * [[[0, 1], [1, 2]],  // 1st line
 *  [[0, 1], [1, 3]],  // 2nd line
 *  [[0, 2], [1, 1]]]  // 3rd line
 * </pre>
 * will be converted to
 * <pre>
 * [[[0, 1], [1, 2]],  // 1st
 *  [[0, 2], [1, 5]],  // 1st + 2nd
 *  [[0, 4], [1, 6]]]  // 1st + 2nd + 3rd
 * </pre>
 *
 * @param {Array.<Array.<Array.<number>>>} plotData List of arrays that
 *     represent individual lines. The line itself is an Array of points.
 * @return {Array.<Array.<Array.<number>>>} A stacked {@code plotData}.
 *     The original {@code plotData} is not affected.
 */
graphUtils.stackFrontToBack = function(plotData) {
  if (!(plotData && plotData[0] && plotData[0].length > 0)) {
    return [];
  }

  var stacked = [];
  for (var i = 0; i < plotData.length; ++i) {
    stacked.push([]);
  }

  for (var j = 0; j < plotData[0].length; ++j) {
    for (var i = 0; i < plotData.length; ++i) {
      var point = [
        plotData[i][j][0],
        plotData[i][j][1] +
            (i == 0 ? 0 : stacked[i - 1][j][1])];
      if (plotData[i][j].interpolated) {
        point.interpolated = true;
      }
      stacked[i].push(point);
    }
  }

  return stacked;
};
