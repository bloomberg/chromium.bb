// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Run a cast v2 mirroring session for 10 seconds.

chrome.test.runTests([
  function sendTestPatterns() {
    // The receive port changes between browser_test invocations, and is passed
    // as an query parameter in the URL.
    let recvPort;
    let autoThrottling;
    let aesKey;
    let aesIvMask;
    try {
      const params = window.location.search;
      recvPort = parseInt(params.match(/(\?|&)port=(\d+)/)[2]);
      chrome.test.assertTrue(recvPort > 0);
      autoThrottling = (params.match(/(\?|&)autoThrottling=(true|false)/)[2] ==
                            'true');
      aesKey = params.match(/(\?|&)aesKey=([0-9A-F]{32})/)[2];
      aesIvMask = params.match(/(\?|&)aesIvMask=([0-9A-F]{32})/)[2];
    } catch (err) {
      chrome.test.fail("Error parsing params -- " + err.message);
      return;
    }

    const kMaxFrameRate = 30;
    chrome.tabCapture.capture(
        { video: true,
          audio: true,
          videoConstraints: {
              mandatory: autoThrottling ? ({
                  minWidth: 320,
                  minHeight: 180,
                  maxWidth: 1920,
                  maxHeight: 1080,
                  maxFrameRate: kMaxFrameRate,
                  enableAutoThrottling: true
              }) : ({
                  minWidth: 1920,
                  minHeight: 1080,
                  maxWidth: 1920,
                  maxHeight: 1080,
                  maxFrameRate: kMaxFrameRate,
              })
          }
        },
        captureStream => {
          if (!captureStream) {
            chrome.test.fail(chrome.runtime.lastError.message || 'null stream');
            return;
          }

          chrome.cast.streaming.session.create(
              captureStream.getAudioTracks()[0],
              captureStream.getVideoTracks()[0],
              (audioId, videoId, udpId) => {
                chrome.cast.streaming.udpTransport.setDestination(
                    udpId, { address: "127.0.0.1", port: recvPort } );
                const rtpStream = chrome.cast.streaming.rtpStream;
                rtpStream.onError.addListener(() => {
                  chrome.test.fail('RTP stream error');
                });
                const audioParams = rtpStream.getSupportedParams(audioId)[0];
                audioParams.payload.aesKey = aesKey;
                audioParams.payload.aesIvMask = aesIvMask;
                rtpStream.start(audioId, audioParams);
                const videoParams = rtpStream.getSupportedParams(videoId)[0];
                videoParams.payload.clockRate = kMaxFrameRate;
                videoParams.payload.aesKey = aesKey;
                videoParams.payload.aesIvMask = aesIvMask;
                rtpStream.start(videoId, videoParams);
                setTimeout(() => {
                  chrome.test.succeed();

                  rtpStream.stop(audioId);
                  rtpStream.stop(videoId);
                  rtpStream.destroy(audioId);
                  rtpStream.destroy(videoId);

                  const tracks = captureStream.getTracks();
                  for (let i = 0; i < tracks.length; ++i) {
                    tracks[i].stop();
                  }
                  chrome.test.assertFalse(captureStream.active);
                }, 15000);  // 15 seconds
              });
        });
  }
]);
