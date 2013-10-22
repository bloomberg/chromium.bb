// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function moduleDidLoad() {
  common.hideModule();
}

// Called by the common.js module.
function domContentLoaded(name, tc, config, width, height) {
  navigator.webkitPersistentStorage.requestQuota(1024 * 1024,
      function(bytes) {
        common.updateStatus(
            'Allocated ' + bytes + ' bytes of persistant storage.');
        common.attachDefaultListeners();
        common.createNaClModule(name, tc, config, width, height);
      },
      function(e) { alert('Failed to allocate space') });
}

// Called by the common.js module.
function attachListeners() {
  var radioEls = document.querySelectorAll('input[type="radio"]');
  for (var i = 0; i < radioEls.length; ++i) {
    radioEls[i].addEventListener('click', onRadioClicked);
  }

  // Wire up the 'click' event for each function's button.
  var functionEls = document.querySelectorAll('.function');
  for (var i = 0; i < functionEls.length; ++i) {
    var functionEl = functionEls[i];
    var id = functionEl.getAttribute('id');
    var buttonEl = functionEl.querySelector('button');

    // The function name matches the element id.
    var func = window[id];
    buttonEl.addEventListener('click', func);
  }
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

function addNameToSelectElements(cssClass, handle, name) {
  var text = '[' + handle + '] ' + name;
  var selectEls = document.querySelectorAll(cssClass);
  for (var i = 0; i < selectEls.length; ++i) {
    var optionEl = document.createElement('option');
    optionEl.setAttribute('value', handle);
    optionEl.appendChild(document.createTextNode(text));
    selectEls[i].appendChild(optionEl);
  }
}

function removeNameFromSelectElements(cssClass, handle) {
  var optionEls = document.querySelectorAll(cssClass + ' > option');
  for (var i = 0; i < optionEls.length; ++i) {
    var optionEl = optionEls[i];
    if (optionEl.value == handle) {
      var selectEl = optionEl.parentNode;
      selectEl.removeChild(optionEl);
    }
  }
}

var filehandle_map = {};
var dirhandle_map = {};

function fopen(e) {
  var filename = document.getElementById('fopenFilename').value;
  var access = document.getElementById('fopenMode').value;
  nacl_module.postMessage(makeCall('fopen', filename, access));
}

function fopenResult(filename, filehandle) {
  filehandle_map[filehandle] = filename;

  addNameToSelectElements('.file-handle', filehandle, filename);
  common.logMessage('File ' + filename + ' opened successfully.');
}

function fclose(e) {
  var filehandle = document.getElementById('fcloseHandle').value;
  nacl_module.postMessage(makeCall('fclose', filehandle));
}

function fcloseResult(filehandle) {
  var filename = filehandle_map[filehandle];
  removeNameFromSelectElements('.file-handle', filehandle, filename);
  common.logMessage('File ' + filename + ' closed successfully.');
}

function fread(e) {
  var filehandle = document.getElementById('freadHandle').value;
  var numBytes = document.getElementById('freadBytes').value;
  nacl_module.postMessage(makeCall('fread', filehandle, numBytes));
}

function freadResult(filehandle, data) {
  var filename = filehandle_map[filehandle];
  common.logMessage('Read "' + data + '" from file ' + filename + '.');
}

function fwrite(e) {
  var filehandle = document.getElementById('fwriteHandle').value;
  var data = document.getElementById('fwriteData').value;
  nacl_module.postMessage(makeCall('fwrite', filehandle, data));
}

function fwriteResult(filehandle, bytes_written) {
  var filename = filehandle_map[filehandle];
  common.logMessage('Wrote ' + bytes_written + ' bytes to file ' + filename +
      '.');
}

function fseek(e) {
  var filehandle = document.getElementById('fseekHandle').value;
  var offset = document.getElementById('fseekOffset').value;
  var whence = document.getElementById('fseekWhence').value;
  nacl_module.postMessage(makeCall('fseek', filehandle, offset, whence));
}

function fseekResult(filehandle, filepos) {
  var filename = filehandle_map[filehandle];
  common.logMessage('Seeked to location ' + filepos + ' in file ' + filename +
      '.');
}

function stat(e) {
  var filename = document.getElementById('statFilename').value;
  nacl_module.postMessage(makeCall('stat', filename));
}

function statResult(filename, size) {
  common.logMessage('File ' + filename + ' has size ' + size + '.');
}

function opendir(e) {
  var dirname = document.getElementById('opendirDirname').value;
  nacl_module.postMessage(makeCall('opendir', dirname));
}

function opendirResult(dirname, dirhandle) {
  dirhandle_map[dirhandle] = dirname;

  addNameToSelectElements('.dir-handle', dirhandle, dirname);
  common.logMessage('Directory ' + dirname + ' opened successfully.');
}

function readdir(e) {
  var dirhandle = document.getElementById('readdirHandle').value;
  nacl_module.postMessage(makeCall('readdir', dirhandle));
}

function readdirResult(dirhandle, ino, name) {
  var dirname = dirhandle_map[dirhandle];
  if (ino === '') {
    common.logMessage('End of directory.');
  } else {
    common.logMessage('Read entry ("' + name + '", ino = ' + ino +
                      ') from directory ' + dirname + '.');
  }
}

function closedir(e) {
  var dirhandle = document.getElementById('closedirHandle').value;
  nacl_module.postMessage(makeCall('closedir', dirhandle));
}

function closedirResult(dirhandle) {
  var dirname = dirhandle_map[dirhandle];
  delete dirhandle_map[dirhandle];

  removeNameFromSelectElements('.dir-handle', dirhandle, dirname);
  common.logMessage('Directory ' + dirname + ' closed successfully.');
}

function mkdir(e) {
  var dirname = document.getElementById('mkdirDirname').value;
  var mode = document.getElementById('mkdirMode').value;
  nacl_module.postMessage(makeCall('mkdir', dirname, mode));
}

function mkdirResult(dirname) {
  common.logMessage('Directory ' + dirname + ' created successfully.');
}

function rmdir(e) {
  var dirname = document.getElementById('rmdirDirname').value;
  nacl_module.postMessage(makeCall('rmdir', dirname));
}

function rmdirResult(dirname) {
  common.logMessage('Directory ' + dirname + ' removed successfully.');
}

function chdir(e) {
  var dirname = document.getElementById('chdirDirname').value;
  nacl_module.postMessage(makeCall('chdir', dirname));
}

function chdirResult(dirname) {
  common.logMessage('Changed directory to: ' + dirname + '.');
}

function getcwd(e) {
  nacl_module.postMessage(makeCall('getcwd'));
}

function getcwdResult(dirname) {
  common.logMessage('getcwd: ' + dirname + '.');
}

function gethostbyname(e) {
  var name = document.getElementById('gethostbynameName').value;
  nacl_module.postMessage(makeCall('gethostbyname', name));
}

function gethostbynameResult(name, addr_type) {
  common.logMessage('gethostbyname returned successfully');
  common.logMessage('h_name = ' + name + '.');
  common.logMessage('h_addr_type = ' + addr_type + '.');
  for (var i = 2; i < arguments.length; i++) {
    common.logMessage('Address number ' + (i-1) + ' = ' + arguments[i] + '.');
  }
}

function connect(e) {
  var host = document.getElementById('connectHost').value;
  var port = document.getElementById('connectPort').value;
  nacl_module.postMessage(makeCall('connect', host, port));
}

function connectResult(sockhandle) {
  common.logMessage('connected');
  addNameToSelectElements('.sock-handle', sockhandle, '[socket]');
}

function recv(e) {
  var handle = document.getElementById('recvHandle').value;
  var bufferSize = document.getElementById('recvBufferSize').value;
  nacl_module.postMessage(makeCall('recv', handle, bufferSize));
}

function recvResult(messageLen, message) {
  common.logMessage("received " + messageLen + ' bytes: ' + message);
}

function send(e) {
  var handle = document.getElementById('sendHandle').value;
  var message = document.getElementById('sendMessage').value;
  nacl_module.postMessage(makeCall('send', handle, message));
}

function sendResult(sentBytes) {
  common.logMessage("sent bytes: " + sentBytes);
}

function close(e) {
  var handle = document.getElementById('closeHandle').value;
  nacl_module.postMessage(makeCall('close', handle));
}

function closeResult(sock) {
  removeNameFromSelectElements('.sock-handle', sock, "[socket]");
  common.logMessage("closed socket: " + sock);
}

/**
 * Return true when |s| starts with the string |prefix|.
 *
 * @param {string} s The string to search.
 * @param {string} prefix The prefix to search for in |s|.
 * @return {boolean} Whether |s| starts with |prefix|.
 */
function startsWith(s, prefix) {
  // indexOf would search the entire string, lastIndexOf(p, 0) only checks at
  // the first index. See: http://stackoverflow.com/a/4579228
  return s.lastIndexOf(prefix, 0) === 0;
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
    common.logMessage(msg);
  } else {
    // Result from a function call.
    var params = msg.split('\1');
    var funcName = params[0];
    var funcResultName = funcName + 'Result';
    var resultFunc = window[funcResultName];

    if (!resultFunc) {
      common.logMessage('Error: Bad message ' + funcName +
                        ' received from NaCl module.');
      return;
    }

    resultFunc.apply(null, params.slice(1));
  }
}
