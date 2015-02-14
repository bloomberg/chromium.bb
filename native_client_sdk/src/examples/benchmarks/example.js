// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function moduleDidLoad() {
}


// Add event listeners after the NaCl module has loaded.  These listeners will
// forward messages to the NaCl module via postMessage()
function attachListeners() {
  setTimeout(function() { common.updateStatus('READY TO RUN'); }, 200);
  document.getElementById('benchmark').addEventListener('click',
    function() {
      common.naclModule.postMessage({'message' : 'run_benchmark'});
      common.updateStatus('BENCHMARKING... (please wait)');
      var window = document.getElementById('result').contentWindow;
      window.document.writeln('<samp>Starting Benchmark Suite<br>');
      window.document.writeln('<table>');
    });
}


// Handle a message coming from the NaCl module.
function handleMessage(message_event) {
  if (message_event.data.message == 'benchmark_result') {
    // benchmark result
    var name = message_event.data.benchmark.name;
    var result = message_event.data.benchmark.result;
    var text = '<tr><th>' + name + ': </th><th>' + result + '</th></tr>';
    console.log(text);
    var window = document.getElementById('result').contentWindow;
    window.document.writeln(text);
  }
  if (message_event.data.message == 'benchmark_finish') {
    common.updateStatus('READY TO RUN');
    var window = document.getElementById('result').contentWindow;
    window.document.writeln('</table>');
    window.document.writeln('Finished<br><br></samp>');
  }
}
