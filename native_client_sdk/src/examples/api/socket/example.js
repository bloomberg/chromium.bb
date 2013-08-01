// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called by the common.js module.
function attachListeners() {
  document.getElementById('connectForm').addEventListener('submit', doConnect);
  document.getElementById('sendForm').addEventListener('submit', doSend);
  document.getElementById('listenForm').addEventListener('submit', doListen);
  document.getElementById('closeButton').addEventListener('click', doClose);
  if (typeof chrome.socket === 'undefined') {
    document.getElementById('createServer').style.display = 'none';
  }
}

// Called by the common.js module.
function moduleDidLoad() {
  // The module is not hidden by default so we can easily see if the plugin
  // failed to load.
  common.hideModule();
}

var msgTcpCreate = 't;'
var msgUdpCreate = 'u;'
var msgSend = 's;'
var msgClose = 'c;'

function doConnect(event) {
  // Send a request message. See also socket.cc for the request format.
  event.preventDefault();
  var hostname = document.getElementById('hostname').value;
  var type = document.getElementById('connect_type').value;
  common.logMessage(type);
  if (type == 'tcp')

{
    common.logMessage(type);
    common.naclModule.postMessage(msgTcpCreate + hostname);
}
  else
    common.naclModule.postMessage(msgUdpCreate + hostname);
}

function doSend(event) {
  // Send a request message. See also socket.cc for the request format.
  event.preventDefault();
  var message = document.getElementById('message').value;
  while (message.indexOf('\\n') > -1)
    message = message.replace('\\n', '\n');
  common.naclModule.postMessage(msgSend + message);
}

var listeningSocket = -1;
var creatingServer = false;

function runUDPEchoServer(port) {
  common.logMessage("Listening on UDP port: " + port);
  chrome.socket.create("udp", {}, function(createInfo) {
    listeningSocket = createInfo.socketId;
    creatingServer = false;
    chrome.socket.bind(listeningSocket,
                       '127.0.0.1',
                       port,
                       function(result) {
      if (result !== 0) {
        common.logMessage("Bind failed: " + result);
        return;
      }

      var recvCallback = function(recvFromInfo) {
        if (recvFromInfo.resultCode <= 0) {
          common.logMessage("recvFrom failed: " + recvFromInfo.resultCode);
          return;
        }

        chrome.socket.sendTo(listeningSocket,
                             recvFromInfo.data,
                             recvFromInfo.address,
                             recvFromInfo.port,
                             function(sendToInfo) {
          if (readInfo.resultCode < 0) {
            common.logMessage("SentTo failed: " + sendToInfo.bytesWritten);
            return;
          }
        })

        chrome.socket.recvFrom(listeningSocket, recvCallback);
      }

      chrome.socket.recvFrom(listeningSocket, recvCallback);
    })
  })
}

function runTCPEchoServer(port) {
  common.logMessage("Listening on TCP port: " + port);
  chrome.socket.create("tcp", {}, function(createInfo) {
    listeningSocket = createInfo.socketId;
    creatingServer = false;
    chrome.socket.listen(listeningSocket,
                         '0.0.0.0',
                         port,
                         10,
                         function(result) {
      if (result !== 0) {
        common.logMessage("Listen failed: " + result);
        return;
      }

      chrome.socket.accept(listeningSocket, function(acceptInfo) {
        if (result !== 0) {
          common.logMessage("Accept failed: " + result);
          return;
        }

        var newSock = acceptInfo.socketId;

        var readCallback = function(readInfo) {
          if (readInfo.resultCode < 0) {
            common.logMessage("Read failed: " + readInfo.resultCode);
            chrome.socket.destroy(newSock);
            return;
          }

          chrome.socket.write(newSock, readInfo.data, function(writeInfo) {})
          chrome.socket.read(newSock, readCallback);
        }

        chrome.socket.read(newSock, readCallback);
      })
    })
  })
}

function doListen(event) {
  // Listen a the given port.
  event.preventDefault();
  if (typeof chrome.socket === 'undefined') {
    common.logMessage("Local JavaScript echo server is only available in " +
                      "the packaged version of the this example.");
    return;
  }

  var port = document.getElementById('port').value;
  var type = document.getElementById('listen_type').value;
  port = parseInt(port);

  if (listeningSocket !== -1) {
     chrome.socket.destroy(listeningSocket);
     listeningSocket = -1;
  }

  if (creatingServer)
    return;
  creatingServer = true;

  if (type == 'tcp')
    runTCPEchoServer(port)
  else
    runUDPEchoServer(port)
}

function doClose() {
  // Send a request message. See also socket.cc for the request format.
  common.naclModule.postMessage(msgClose);
}

function handleMessage(message) {
  common.logMessage(message.data);
}
