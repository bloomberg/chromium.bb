// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('Registering chrome.runtime message listener...');

chrome.runtime.onMessageExternal.addListener((request, from, sendResponse) => {
  if (!request.start) {
    return;
  }

  const kCallbackTimeoutMillis = 10000;

  if (window.currentCaptureStream) {
    sendResponse({success: false, reason: 'Already started!'});
    return;
  }

  // Start capture and wait up to kCallbackTimeoutMillis for it to start.
  const startCapturePromise = new Promise((resolve, reject) => {
    const timeoutId = setTimeout(() => {
      reject(Error('chrome.tabCapture.capture() did not call back'));
    }, kCallbackTimeoutMillis);

    const captureOptions = {
      video: true,
      audio: true,
      videoConstraints: {
        mandatory: {
          minWidth: request.enableAutoThrottling ? 320 : 1920,
          minHeight: request.enableAutoThrottling ? 180 : 1080,
          maxWidth: 1920,
          maxHeight: 1080,
          maxFrameRate: (request.maxFrameRate || 30),
        }
      }
    };
    console.log('Starting tab capture...');
    chrome.tabCapture.capture(captureOptions, captureStream => {
      clearTimeout(timeoutId);
      if (captureStream) {
        console.log('Started tab capture.');
        resolve(captureStream);
      } else {
        if (chrome.runtime.lastError) {
          reject(chrome.runtime.lastError);
        } else {
          reject(Error('null stream'));
        }
      }
    });
  });

  // Then, start Cast Streaming and wait up to kCallbackTimeoutMillis for it to
  // start.
  const startStreamingPromise = startCapturePromise.then(captureStream => {
    return new Promise((resolve, reject) => {
      const timeoutId = setTimeout(() => {
        reject(Error(
          'chrome.cast.streaming.session.create() did not call back'));
      }, kCallbackTimeoutMillis);

      console.log('Starting Cast streaming...');
      chrome.cast.streaming.session.create(
          captureStream.getAudioTracks()[0],
          captureStream.getVideoTracks()[0],
          (audioId, videoId, udpId) => {
        clearTimeout(timeoutId);

        try {
          chrome.cast.streaming.udpTransport.setDestination(
              udpId, { address: '127.0.0.1', port: request.recvPort } );
          const rtpStream = chrome.cast.streaming.rtpStream;
          rtpStream.onError.addListener(() => {
            console.error('RTP stream error');
          });
          const audioParams = rtpStream.getSupportedParams(audioId)[0];
          audioParams.payload.aesKey = request.aesKey;
          audioParams.payload.aesIvMask = request.aesIvMask;
          rtpStream.start(audioId, audioParams);
          const videoParams = rtpStream.getSupportedParams(videoId)[0];
          videoParams.payload.clockRate = (request.maxFrameRate || 30);
          videoParams.payload.aesKey = request.aesKey;
          videoParams.payload.aesIvMask = request.aesIvMask;
          rtpStream.start(videoId, videoParams);

          console.log('Started Cast streaming.');
          window.currentCaptureStream = captureStream;
          resolve();
        } catch (error) {
          reject(error);
        }
      });
    });
  });

  startStreamingPromise.then(() => {
    console.log('Sending success response...');
    sendResponse({success: true});
  }).catch(error => {
    console.log('Sending error response...');
    let errorMessage;
    if (typeof error === 'object' &&
        ('stack' in error || 'message' in error)) {
      errorMessage = (error.stack || error.message);
    } else {
      errorMessage = String(error);
    }
    sendResponse({success: false, reason: errorMessage});
  });

  return true;  // Indicate that sendResponse() will be called asynchronously.
});
