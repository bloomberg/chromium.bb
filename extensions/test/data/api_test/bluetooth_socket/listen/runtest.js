// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var uuid = '2de497f9-ab28-49db-b6d2-066ea69f1737';
var clientAddress = '11:12:13:14:15:16';

function testListen() {
  chrome.test.assertEq(2, sockets.length);

  // First socket should be the listening one.
  chrome.test.assertEq(serverSocketId, sockets[0].socketId);
  chrome.test.assertEq(false, sockets[0].persistent);
  chrome.test.assertEq('MyServiceName', sockets[0].name);
  chrome.test.assertEq(false, sockets[0].paused);
  chrome.test.assertEq(false, sockets[0].connected);
  chrome.test.assertEq(undefined, sockets[0].address);
  chrome.test.assertEq(uuid, sockets[0].uuid);

  // Second socket should be the client one, which unlike the server should
  // be created paused.
  chrome.test.assertEq(clientSocketId, sockets[1].socketId);
  chrome.test.assertEq(false, sockets[1].persistent);
  chrome.test.assertEq(undefined, sockets[1].name);
  chrome.test.assertEq(true, sockets[1].paused);
  chrome.test.assertEq(true, sockets[1].connected);
  chrome.test.assertEq(clientAddress, sockets[1].address);
  chrome.test.assertEq(uuid, sockets[1].uuid);

  chrome.test.succeed();
}

function startTests() {
  chrome.test.runTests([testListen]);
}

function expectError(message) {
  if (!chrome.runtime.lastError) {
    chrome.test.fail("Expected an error");
  }
  chrome.test.assertEq(message, chrome.runtime.lastError.message);
}

function failOnError() {
  if (chrome.runtime.lastError) {
    chrome.test.fail(chrome.runtime.lastError.message);
  }
}

function secondStage() {
  chrome.bluetoothSocket.getSockets(
    function(result) {
      failOnError();
      sockets = result;

      chrome.bluetoothSocket.disconnect(serverSocketId);
      chrome.bluetoothSocket.disconnect(clientSocketId);

      // Check for error conditions.
      chrome.bluetoothSocket.listenUsingRfcomm(
          1234, uuid,
          function() {
            expectError("Socket not found");

            chrome.bluetoothSocket.create(
              function(socket) {
                failOnError();
                chrome.bluetoothSocket.listenUsingRfcomm(
                  socket.socketId, 'not a valid uuid',
                  function() {
                    expectError("Invalid UUID");

                    chrome.bluetoothSocket.listenUsingRfcomm(
                      socket.socketId, '1234',
                      function() {
                        expectError("Permission denied");

                        chrome.bluetoothSocket.listenUsingL2cap(
                          socket.socketId, uuid, {'psm': 1234},
                          function() {
                            expectError("Invalid PSM");

                            chrome.bluetoothSocket.listenUsingL2cap(
                              socket.socketId, uuid, {'psm': 4369},
                              function() {
                                expectError("Invalid PSM");

                                chrome.bluetoothSocket.listenUsingL2cap(
                                  socket.socketId, uuid, {'psm': 13},
                                  function() {
                                    expectError("Invalid PSM");

                                    chrome.test.sendMessage(
                                        'ready', startTests);
                                  });
                              });
                          });
                      });
                  });
              });
          });
    });
}

chrome.bluetoothSocket.create(
  {'name': 'MyServiceName'},
  function(socket) {
    failOnError();

    serverSocketId = socket.socketId;

    chrome.bluetoothSocket.onAccept.addListener(
      function(info) {
        if (info.socketId != socket.socketId)
          return;

        clientSocketId = info.clientSocketId;
      });
    chrome.bluetoothSocket.onAcceptError.addListener(
      function(error_info) {
        chrome.test.fail(error_info.errorMessage);
      });

    chrome.bluetoothSocket.listenUsingRfcomm(
      socket.socketId, uuid,
      function() {
        failOnError();

        chrome.test.sendMessage('ready', secondStage);
      });
  });
