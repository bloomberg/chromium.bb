// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var lastModuleError = '';
var crashed = false;

function domContentLoaded(name, tc, config, width, height) {
  common.createNaClModule(name, tc, config, width, height);
  common.attachDefaultListeners();

  updateStatus('Page Loaded');
}

// Indicate success when the NaCl module has loaded.
function moduleDidLoad() {
  updateStatus('LOADED');
  setTimeout(boom, 2000);
}

function findAddress(addr, map) {
  if (map.length < 1) {
    return 'MAP Unavailable';
  }
  if (addr < map[0].offs)  {
    return 'Invalid Address';
  }

  for (var i=1; i < map.length; i++) {
    if (addr < map[i].offs) {
      var offs = addr - map[i-1].offs;
      var filename = map[i-1].file;

      // Force filename to 50 chars
      if (filename) {
        if (filename.length > 50) {
            filename = '...' + filename.substr(filename.length - 47);
        }
      } else {
        filename = 'Unknown';
      }
      while (filename.length < 50) {
        filename = ' ' + filename
      }
      return filename + ' '  + map[i-1].name + ' + 0x' + offs.toString(16);
    }
  }

  var last = map.length - 1;
  return filename + ' '  + map[last].name + ' + 0x' + offs.toString(16);
}

function buildTextMap(map) {
  map = map.split('\n');
  var orderedMap = [];
  for (var i=0; i < map.length; i++) {
    var line = map[i].replace('\t', ' ');
    var vals = line.split(' ');
    var obj = {
      offs: parseInt(vals[0], 16),
      name: vals[2],
      file: vals[3]
    }
    if (vals[1] && vals[1].toUpperCase() == 'T') {
      orderedMap.push(obj);
    }
  }
  orderedMap.sort(function(a,b) { return a.offs - b.offs; });
  return orderedMap;
}

function updateStack(traceinfo, map) {
  map = buildTextMap(map);
  var text = 'Stack Trace\n';
  for (var i=0; i < traceinfo.frames.length; i++) {
    var frame = traceinfo.frames[i];
    var addr =  findAddress(frame.prog_ctr, map)
    text += '[' + i.toString(10) + '] ' + addr + '\n';
  }
  document.getElementById('trace').value = text;
}

function fetchMap(url, traceinfo) {
  var xmlhttp = new XMLHttpRequest();
  xmlhttp.open('GET', url, true);
  xmlhttp.onload = function() {
    updateStack(traceinfo, this.responseText);
  }
  xmlhttp.traceinfo = traceinfo;
  xmlhttp.send();
}

// Handle a message coming from the NaCl module.
function handleMessage(message_event) {
  msg_type = message_event.data.substring(0, 4);
  msg_data = message_event.data.substring(5, message_event.data.length);
  if (msg_type == 'LOG:') {
    document.getElementById('log').value += msg_data + '\n';
    return;
  }
  if (msg_type == 'STS:') {
    updateStatus(msg_data);
  }
  if (msg_type == 'TRC:') {
    crashed = true;
    document.getElementById('json').value = msg_data;
    crash_info = JSON.parse(msg_data);
    updateStatus('Crash Reported');
    src = common.naclModule.getAttribute('path');
    fetchMap(src + '/debugging_' + crash_info['arch'] + '.map', crash_info);
    return;
  }
}

function handleCrash(message) {
  updateStatus(message);
}

function updateStatus(message) {
  common.updateStatus(message);

  if (message)
    document.getElementById('log').value += message + '\n'
}

function boom() {
  if (!crashed) {
    updateStatus('Send BOOM');
    common.naclModule.postMessage('BOOM');
  }
}
