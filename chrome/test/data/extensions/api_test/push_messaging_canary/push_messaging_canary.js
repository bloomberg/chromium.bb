// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var clientId = '';
var clientSecret =  '';
var refreshToken = '';
const payload = 'Hello push messaging!';

// As part of the canary test, verify that we got what we expected.
function verifyDetails(details) {
  chrome.test.assertEq(payload, details.payload);
  chrome.test.assertEq(1, details.subchannelId);
  console.log("push message arrived and matches expectations!");
}

// The test to be run.
function testEventDispatch() {
  chrome.pushMessaging.onMessage.addListener(
      chrome.test.callbackPass(verifyDetails));
  chrome.test.sendMessage('ready');
}

function startTestWithCredentials(paramClientId, paramClientSecret,
                                  paramRefreshToken) {
  console.log('startTestWithCredentials(' + paramClientId + ',  ' +
              paramClientSecret + ',  ' + paramRefreshToken + ')' );
  clientId = paramClientId;
  clientSecret = paramClientSecret;
  refreshToken = paramRefreshToken;
  console.log("Getting channel Id");
  // Start by getting the channel Id, the callback will continue the test.
  chrome.pushMessaging.getChannelId(getChannelIdCallback);
}

// Once we get the channel ID, start a push.
function getChannelIdCallback(details) {
  console.log("channelId callback arrived, channel Id is '" +
              details.channelId + "'");
  var channelId = details.channelId;
  getAccessToken(channelId);
}

// This function will go to the server and ask it to send us
// a push message so we can test the round trip.  The first stage
// in our mission is to get an access token to use to post the
// request.
function getAccessToken(channelId) {
  var tokenRequest = new XMLHttpRequest();
  var tokenUrl = 'https://accounts.google.com/o/oauth2/token';
  tokenRequest.open('POST', tokenUrl, true);
  tokenRequest.setRequestHeader('Content-Type',
                                'application/x-www-form-urlencoded');
  var tokenData = 'client_secret=' + clientSecret + '&' +
                  'grant_type=refresh_token&' +
                  'refresh_token=' + refreshToken + '&' +
                  'client_id=' + clientId;
  tokenRequest.onreadystatechange = function () {
    if (tokenRequest.readyState === 4) {
      if (tokenRequest.status === 200) {
        console.log("First XHR returned, " + tokenRequest.response);

        // Parse the access token out of the XHR message.
        var parsedResponse = JSON.parse(tokenRequest.response);
        var accessToken = parsedResponse.access_token;

        askServerToSendPushMessageWithToken(accessToken, channelId);
      } else {
        console.log('Error sending first XHR, status is ' +
                    tokenRequest.statusText);
      }
    }
  }

  // Send the XHR with the data we need.
  console.log("Sending first XHR, data is " + tokenData);
  tokenRequest.send(tokenData);
}

// Now that we have an access token, use it to send the message.
function askServerToSendPushMessageWithToken(accessToken, channelId) {
  // Setup the push request, using the access token we just got.

  var channelNum = 1;
  var pushURL ='https://www.googleapis.com/chromewebstore/v1.1/notifications';
  var pushData = { "channelId": channelId, "subchannelId": channelNum,
                   "payload": payload};
  var pushRequest = new XMLHttpRequest();
  pushRequest.open('POST', pushURL, true);
  // Set the headers for the push request, including the parsed accessToken.
  pushRequest.setRequestHeader('Authorization', 'Bearer ' + accessToken);
  pushRequest.setRequestHeader('Content-Type', 'application/json');
  pushRequest.onreadystatechange = function () {
    if (pushRequest.readyState === 4) {
      if (pushRequest.status === 200) {
        console.log("second XHR returned, " + pushRequest.response);
      } else {
        console.log('Error sending second XHR, status is ' +
                    pushRequest.status + ' ' +
                    pushRequest.statusText + ' body is ' +
                    pushRequest.response);
      }
    }
  }

  // Send the push request.
  console.log("sending second XHR, data is " + pushData + " url is " + pushURL);
  pushRequest.send(JSON.stringify(pushData));
}

// Run the test
chrome.test.runTests([testEventDispatch]);
