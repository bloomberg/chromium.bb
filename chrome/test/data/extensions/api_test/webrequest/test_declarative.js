// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var onRequest = chrome.declarativeWebRequest.onRequest;
var RequestMatcher = chrome.declarativeWebRequest.RequestMatcher;
var CancelRequest = chrome.declarativeWebRequest.CancelRequest;
var RedirectRequest = chrome.declarativeWebRequest.RedirectRequest;

function getURLHttpSimple() {
  return getServerURL("files/extensions/api_test/webrequest/simpleLoad/a.html");
}

function getURLHttpComplex() {
  return getServerURL(
      "files/extensions/api_test/webrequest/complexLoad/a.html");
}

runTests([
  function testCancelRequest() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onErrorOccurred",
          event: "onErrorOccurred",
          details: {
            url: getURLHttpSimple(),
            fromCache: false,
            error: "net::ERR_BLOCKED_BY_CLIENT"
          }
        },
      ],
      [ ["onCompleted"] ]);
    onRequest.addRules(
      [ {'conditions': [
           new RequestMatcher({
             'url': {
                 'pathSuffix': ".html",
                 'ports': [testServerPort, [1000, 2000]],
                 'schemes': ["http"]
             },
             'resourceType': ["main_frame"]})],
         'actions': [new CancelRequest()]}
      ],
      function() {navigateAndWait(getURLHttpSimple());}
    );
  },

  function testRedirectRequest() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onBeforeRequest-a",
          event: "onBeforeRequest",
          details: {
            type: "main_frame",
            url: getURLHttpComplex(),
            frameUrl: getURLHttpComplex()
          },
        },
        { label: "onBeforeRedirect",
          event: "onBeforeRedirect",
          details: {
            url: getURLHttpComplex(),
            redirectUrl: getURLHttpSimple(),
            statusLine: "",
            statusCode: -1,
            fromCache: false,
          }
        },
        { label: "onBeforeRequest-b",
          event: "onBeforeRequest",
          details: {
            type: "main_frame",
            url: getURLHttpSimple(),
            frameUrl: getURLHttpSimple(),
          },
        },
        { label: "onCompleted",
          event: "onCompleted",
          details: {
            ip: "127.0.0.1",
            url: getURLHttpSimple(),
            fromCache: false,
            statusCode: 200,
            statusLine: "HTTP/1.0 200 OK",
          }
        },
      ],
      [ ["onBeforeRequest-a", "onBeforeRedirect", "onBeforeRequest-b",
         "onCompleted"] ]);

    onRequest.addRules(
      [ {'conditions': [new RequestMatcher({'url': {'pathSuffix': ".html"}})],
         'actions': [
             new RedirectRequest({'redirectUrl': getURLHttpSimple()})]}
      ],
      function() {navigateAndWait(getURLHttpComplex());}
    );
  },
  ]);
