// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var trafficToTextButton = document.getElementById('traffic-to-text');
  var trafficDump = document.getElementById('traffic-dump');
  trafficToTextButton.addEventListener('click', function(event) {
    chrome.sync.getClientServerTraffic(printData);
});

function printData(trafficRecord) {
  var trafficData = '';
  trafficData += '===\n';
  trafficData += 'Client Server Traffic\n';
  trafficData += '===\n';
  trafficData += JSON.stringify(trafficRecord, null, 2);
  trafficData += '\n';

  trafficDump.textContent = trafficData;
}
})();
