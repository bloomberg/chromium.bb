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
  var traffic_data = '';
  traffic_data += '===\n';
  traffic_data += 'Client Server Traffic\n';
  traffic_data += '===\n';
  traffic_data += JSON.stringify(trafficRecord, null, 2);
  traffic_data += '\n';

  trafficDump.textContent = traffic_data;
}
})();
