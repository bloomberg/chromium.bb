// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;

var TABLE_NAME = 'en-us-comp8.ctb';
var TEXT = 'hello';
// Translation of the above string as a hexadecimal sequence of cells.
var CELLS = '1311070715';

var pendingCallback = null;
var pendingMessageId = -1;
var nextMessageId = 0;
var naclEmbed = null;

function loadLibrary(callback) {
  var embed = document.createElement('embed');
  embed.src = 'liblouis_nacl.nmf';
  embed.type = 'application/x-nacl';
  embed.width = 0;
  embed.height = 0;
  embed.setAttribute('tablesdir', 'tables');
  embed.addEventListener('load', function() {
    console.log("liblouis loaded");
    naclEmbed = embed;
    callback();
  }, false /* useCapture */);
  embed.addEventListener('error', function() {
    chrome.test.fail('liblouis load error');
  }, false /* useCapture */);
  embed.addEventListener('message', function(e) {
    var reply = JSON.parse(e.data);
    console.log('Message from liblouis: ' + e.data);
    pendingCallback(reply);
  }, false /* useCapture */);
  document.body.appendChild(embed);
}


function rpc(command, args, callback) {
  var messageId = '' + nextMessageId++;
  args['command'] = command;
  args['message_id'] = messageId;
  var json = JSON.stringify(args)
  console.log('Message to liblouis: ' + json);
  naclEmbed.postMessage(json);
  pendingCallback = callback;
  pendingMessageId = messageId;
}


function expectSuccessReply(callback) {
  return function(reply) {
    chrome.test.assertEq(pendingMessageId, reply['in_reply_to']);
    chrome.test.assertTrue(reply['error'] === undefined);
    chrome.test.assertTrue(reply['success']);
    if (callback) {
      callback(reply);
    }
  };
}


loadLibrary(function() {
  chrome.test.runTests([
  function testGetTranslator() {
    rpc('CheckTable', { 'table_name': TABLE_NAME},
       pass(expectSuccessReply()));
  },

  function testTranslateString() {
    rpc('Translate', { 'table_name': TABLE_NAME, 'text': TEXT},
        pass(expectSuccessReply(function(reply) {
          chrome.test.assertEq(CELLS, reply['cells']);
        })));
  },

  function testBackTranslateString() {
    rpc('BackTranslate', { 'table_name': TABLE_NAME, 'cells': CELLS},
        pass(expectSuccessReply(function(reply) {
          chrome.test.assertEq(TEXT, reply['text']);
        })));
  },
])});
