// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @typedef {{min: number, max: number, count: number}} */
let Bucket;

/** @typedef {{name: string, buckets: !Array<Bucket>}} */
let Histogram;

const TaskSchedulerInternals = {
  /**
   * Updates the histograms on the page.
   * @param {!Array<!Histogram>} histograms Array of histogram objects.
   */
  updateHistograms: function(histograms) {
    const histogramContainer = $('histogram-container');
    for (const i in histograms) {
      const histogram = histograms[i];
      const title = document.createElement('div');
      title.textContent = histogram.name;
      histogramContainer.appendChild(title);
      if (histogram.buckets.length > 0) {
        histogramContainer.appendChild(
            TaskSchedulerInternals.createHistogramTable(histogram.buckets));
      } else {
        const unavailable = document.createElement('div');
        unavailable.textContent = 'No Data Recorded';
        histogramContainer.appendChild(unavailable);
      }
    }
  },

  /**
   * Returns a table representation of the histogram buckets.
   * @param {Object} buckets The histogram buckets.
   * @return {Object} A table element representation of the histogram buckets.
   */
  createHistogramTable: function(buckets) {
    const table = document.createElement('table');
    const headerRow = document.createElement('tr');
    const dataRow = document.createElement('tr');
    for (const i in buckets) {
      const bucket = buckets[i];
      const header = document.createElement('th');
      header.textContent = `${bucket.min}-${bucket.max}`;
      headerRow.appendChild(header);
      const data = document.createElement('td');
      data.textContent = bucket.count;
      dataRow.appendChild(data);
    }
    table.appendChild(headerRow);
    table.appendChild(dataRow);
    return table;
  },

  /**
   * Handles callback from onGetTaskSchedulerData.
   * @param {Object} data Dictionary containing all task scheduler metrics.
   */
  onGetTaskSchedulerData: function(data) {
    $('status').textContent =
        data.instantiated ? 'Instantiated' : 'Not Instantiated';
    $('details').hidden = !data.instantiated;
    if (!data.instantiated) {
      return;
    }

    TaskSchedulerInternals.updateHistograms(data.histograms);
  }
};

document.addEventListener('DOMContentLoaded', function() {
  chrome.send('getTaskSchedulerData');
});
