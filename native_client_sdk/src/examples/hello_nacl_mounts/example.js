// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function moduleDidLoad() {
  common.hideModule();
}

// Called by the common.js module.
function domContentLoaded(name, tc, config, width, height) {
  window.webkitStorageInfo.requestQuota(window.PERSISTENT, 1024*1024,
      function(bytes) {
        common.updateStatus(
            'Allocated '+bytes+' bytes of persistant storage.');
        common.createNaClModule(name, tc, config, width, height);
        common.attachDefaultListeners();
      },
      function(e) { alert('Failed to allocate space') });
}

// Called by the common.js module.
function attachListeners() {
  var radioEls = document.querySelectorAll('input[type="radio"]');
  for (var i = 0; i < radioEls.length; ++i) {
    radioEls[i].addEventListener('click', onRadioClicked);
  }

  document.getElementById('fopenExecute').addEventListener('click', fopen);
  document.getElementById('fcloseExecute').addEventListener('click', fclose);
  document.getElementById('freadExecute').addEventListener('click', fread);
  document.getElementById('fwriteExecute').addEventListener('click', fwrite);
  document.getElementById('fseekExecute').addEventListener('click', fseek);
}

function onRadioClicked(e) {
  var divId = this.id.slice(5);  // skip "radio"
  var functionEls = document.querySelectorAll('.function');
  for (var i = 0; i < functionEls.length; ++i) {
    var visible = functionEls[i].id === divId;
    if (functionEls[i].id === divId)
      functionEls[i].removeAttribute('hidden');
    else
      functionEls[i].setAttribute('hidden');
  }
}

function addFilenameToSelectElements(filehandle, filename) {
  var text = '[' + filehandle + '] ' + filename;
  var selectEls = document.querySelectorAll('select.file-handle');
  for (var i = 0; i < selectEls.length; ++i) {
    var optionEl = document.createElement('option');
    optionEl.setAttribute('value', filehandle);
    optionEl.appendChild(document.createTextNode(text));
    selectEls[i].appendChild(optionEl);
  }
}

function removeFilenameFromSelectElements(filehandle) {
  var optionEls = document.querySelectorAll('select.file-handle > option');
  for (var i = 0; i < optionEls.length; ++i) {
    var optionEl = optionEls[i];
    if (optionEl.value == filehandle) {
      var selectEl = optionEl.parentNode;
      selectEl.removeChild(optionEl);
    }
  }
}

var filehandle_map = {};

function fopen(e) {
  var filename = document.getElementById('fopenFilename').value;
  var access = document.getElementById('fopenMode').value;
  nacl_module.postMessage(makeCall('fopen', filename, access));
}

function fopen_result(filename, filehandle) {
  filehandle_map[filehandle] = filename;

  addFilenameToSelectElements(filehandle, filename)
  logMessage('File ' + filename + ' opened successfully.');
}

function fclose(e) {
  var filehandle = document.getElementById('fcloseHandle').value;
  nacl_module.postMessage(makeCall('fclose', filehandle));
}

function fclose_result(filehandle) {
  var filename = filehandle_map[filehandle];
  removeFilenameFromSelectElements(filehandle, filename);
  logMessage('File ' + filename + ' closed successfully.');
}

function fread(e) {
  var filehandle = document.getElementById('freadHandle').value;
  var numBytes = document.getElementById('freadBytes').value;
  nacl_module.postMessage(makeCall('fread', filehandle, numBytes));
}

function fread_result(filehandle, data) {
  var filename = filehandle_map[filehandle];
  logMessage('Read "' + data + '" from file ' + filename + '.');
}

function fwrite(e) {
  var filehandle = document.getElementById('fwriteHandle').value;
  var data = document.getElementById('fwriteData').value;
  nacl_module.postMessage(makeCall('fwrite', filehandle, data));
}

function fwrite_result(filehandle, bytes_written) {
  var filename = filehandle_map[filehandle];
  logMessage('Wrote ' + bytes_written + ' bytes to file ' + filename + '.');
}

function fseek(e) {
  var filehandle = document.getElementById('fseekHandle').value;
  var offset = document.getElementById('fseekOffset').value;
  var whence = document.getElementById('fseekWhence').value;
  nacl_module.postMessage(makeCall('fseek', filehandle, offset, whence));
}

function fseek_result(filehandle, filepos) {
  var filename = filehandle_map[filehandle];
  logMessage('Seeked to location ' + filepos + ' in file ' + filename + '.');
}

/**
 * Return true when |s| starts with the string |prefix|.
 *
 * @param {string} s The string to search.
 * @param {string} prefix The prefix to search for in |s|.
 */
function startsWith(s, prefix) {
  // indexOf would search the entire string, lastIndexOf(p, 0) only checks at
  // the first index. See: http://stackoverflow.com/a/4579228
  return s.lastIndexOf(prefix, 0) === 0;
}

function logMessage(msg) {
  var logEl = document.getElementById('log');

  // Perform some basic escaping.
  msg = msg.replace(/&/g, '&amp;')
           .replace(/</g, '&lt;')
           .replace(/>/g, '&gt;')
           .replace(/"/g, '&quot;')
           .replace(/'/g, '&apos;');

  logEl.innerHTML += msg + '<br>';
}

function makeCall(func) {
  var message = func;
  for (var i = 1; i < arguments.length; ++i) {
    message += '\1' + arguments[i];
  }

  return message;
}

// Called by the common.js module.
function handleMessage(message_event) {
  var msg = message_event.data;
  if (startsWith(msg, 'Error:')) {
    logMessage(msg);
  } else {
    // Result from a function call.
    var params = msg.split('\1');
    var func_name = params[0];
    var func_result_name = func_name + '_result';
    var result_func = window[func_result_name];

    if (!result_func) {
      logMessage('Error: Bad message received from NaCl module.');
      return;
    }

    result_func.apply(null, params.slice(1));
  }
}
