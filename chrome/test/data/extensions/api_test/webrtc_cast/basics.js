// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var sendTransport = chrome.webrtc.castSendTransport;
var tabCapture = chrome.tabCapture;
var udpTransport = chrome.webrtc.castUdpTransport;

chrome.test.runTests([
  function udpTransportCreate() {
    udpTransport.create(function(info) {
      udpTransport.start(
          info.transportId,
          {address: "127.0.0.1", port: 60613});
      udpTransport.stop(info.transportId);
      udpTransport.destroy(info.transportId);
      chrome.test.succeed();
    });
  },
  function sendTransportCreate() {
    tabCapture.capture({audio: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      udpTransport.create(function(stream, udpInfo) {
        sendTransport.create(
            udpInfo.transportId,
            stream.getAudioTracks()[0],
            function(stream, udpInfo, sendTransportId) {
          sendTransport.destroy(sendTransportId);
          udpTransport.destroy(udpInfo.transportId);
          stream.stop();
          chrome.test.succeed();
        }.bind(null, stream, udpInfo));
      }.bind(null, stream));
    });
  },
  function sendTransportGetCaps() {
    tabCapture.capture({audio: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      udpTransport.create(function(stream, udpInfo) {
        sendTransport.create(
            udpInfo.transportId,
            stream.getAudioTracks()[0],
            function(stream, udpInfo, sendTransportId) {
          sendTransport.getCaps(sendTransportId, function(
            stream, udpInfo, sendTransportId, caps) {
              sendTransport.destroy(sendTransportId);
              udpTransport.destroy(udpInfo.transportId);
             stream.stop();
             chrome.test.succeed();
          }.bind(null, stream, udpInfo, sendTransportId));
        }.bind(null, stream, udpInfo));
      }.bind(null, stream));
    });
  },
  function sendTransportCreateParams() {
    tabCapture.capture({audio: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      udpTransport.create(function(stream, udpInfo) {
        sendTransport.create(
            udpInfo.transportId,
            stream.getAudioTracks()[0],
            function(stream, udpInfo, sendTransportId) {
          // TODO(hclam): Pass a valid value for |remoteCaps|.
          var remoteCaps = {
            payloads: [],
            rtcpFeatures: [],
            fecMechanisms: [],
          };
          sendTransport.createParams(sendTransportId, remoteCaps, function(
            stream, udpInfo, sendTransportId, caps) {
              sendTransport.destroy(sendTransportId);
              udpTransport.destroy(udpInfo.transportId);
             stream.stop();
             chrome.test.succeed();
          }.bind(null, stream, udpInfo, sendTransportId));
        }.bind(null, stream, udpInfo));
      }.bind(null, stream));
    });
  },
  function sendTransportStart() {
    tabCapture.capture({audio: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      udpTransport.create(function(stream, udpInfo) {
        sendTransport.create(
            udpInfo.transportId,
            stream.getAudioTracks()[0],
            function(stream, udpInfo, sendTransportId) {
          // TODO(hclam): Pass a valid value for |params|.
          var params = {
            payloads: [],
            rtcpFeatures: [],
            fecMechanisms: [],
          };
          sendTransport.start(sendTransportId, params);
          sendTransport.stop(sendTransportId);
          sendTransport.destroy(sendTransportId);
          udpTransport.destroy(udpInfo.transportId);
          stream.stop();
          chrome.test.succeed();
        }.bind(null, stream, udpInfo));
      }.bind(null, stream));
    });
  },
]);
