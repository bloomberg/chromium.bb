// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var onRequest = chrome.experimental.webRequest.onRequest;
var RequestMatcher = chrome.experimental.webRequest.RequestMatcher;
var CancelRequest = chrome.experimental.webRequest.CancelRequest;
var RedirectRequest = chrome.experimental.webRequest.RedirectRequest;

runTests([
  function testCancelRequest() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onErrorOccurred",
          event: "onErrorOccurred",
          details: {
            url: getURL("simpleLoad/a.html"),
            fromCache: false,
            error: "net::ERR_BLOCKED_BY_CLIENT"
          }
        },
      ],
      [ ["onCompleted"] ]);

    onRequest.addRules(
      [ {'conditions': [
           new RequestMatcher({'path_suffix': ".html",
                               'resourceType': ["main_frame"],
                               'schemes': ["chrome-extension"]})],
         'actions': [new CancelRequest()]}
      ],
      function() {navigateAndWait(getURL("simpleLoad/a.html"));}
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
            url: getURL("complexLoad/a.html"),
            frameUrl: getURL("complexLoad/a.html")
          },
        },
        { label: "onBeforeRedirect",
          event: "onBeforeRedirect",
          details: {
            url: getURL("complexLoad/a.html"),
            redirectUrl: getURL("simpleLoad/a.html"),
            statusLine: "",
            statusCode: -1,
            fromCache: false,
          }
        },
        { label: "onBeforeRequest-b",
          event: "onBeforeRequest",
          details: {
            type: "main_frame",
            url: getURL("simpleLoad/a.html"),
            frameUrl: getURL("simpleLoad/a.html"),
          },
        },
        { label: "onCompleted",
          event: "onCompleted",
          details: {
            url: getURL("simpleLoad/a.html"),
            fromCache: false,
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
          }
        },
      ],
      [ ["onBeforeRequest-a", "onBeforeRedirect", "onBeforeRequest-b",
         "onCompleted"] ]);

    onRequest.addRules(
      [ {'conditions': [new RequestMatcher({'path_suffix': ".html"})],
         'actions': [
             new RedirectRequest({'redirectUrl': getURL("simpleLoad/a.html")})]}
      ],
      function() {navigateAndWait(getURL("complexLoad/a.html"));}
    );
  },
  ]);
