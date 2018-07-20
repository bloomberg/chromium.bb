// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Fetches data from the Ukm service and updates the DOM to display it as a
 * list.
 */
function updateUkmData() {
  cr.sendWithPromise('requestUkmData').then((ukmData) => {
    $('state').innerText = ukmData.state ? 'True' : 'False';
    $('clientid').innerText = ukmData.client_id;
    $('sessionid').innerText = ukmData.session_id;

    sourceDiv = $('sources');
    for (const source of ukmData.sources) {
      const sourceElement = document.createElement('h3');
      if (source.url !== undefined)
        sourceElement.innerText = `Id: ${source.id} Url: ${source.url}`;
      else
        sourceElement.innerText = `Id: ${source.id}`;
      sourceDiv.appendChild(sourceElement);

      for (const entry of source.entries) {
        const entryElement = document.createElement('h4');
        entryElement.innerText = `Entry: ${entry.name}`;
        sourceDiv.appendChild(entryElement);

        if (entry.metrics === undefined)
          continue;
        for (const metric of entry.metrics) {
          const metricElement = document.createElement('h5');
          metricElement.innerText =
              `Metric: ${metric.name} Value: ${metric.value}`;
          sourceDiv.appendChild(metricElement);
        }
      }
    }
  });
}

updateUkmData();
