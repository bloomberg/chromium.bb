// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var onRequest = chrome.declarativeWebRequest.onRequest;
var AddResponseHeader =
    chrome.declarativeWebRequest.AddResponseHeader;
var RequestMatcher = chrome.declarativeWebRequest.RequestMatcher;
var CancelRequest = chrome.declarativeWebRequest.CancelRequest;
var RedirectByRegEx = chrome.declarativeWebRequest.RedirectByRegEx;
var RedirectRequest = chrome.declarativeWebRequest.RedirectRequest;
var RedirectToTransparentImage =
    chrome.declarativeWebRequest.RedirectToTransparentImage;
var RedirectToEmptyDocument =
    chrome.declarativeWebRequest.RedirectToEmptyDocument;
var SetRequestHeader =
    chrome.declarativeWebRequest.SetRequestHeader;
var RemoveRequestHeader =
    chrome.declarativeWebRequest.RemoveRequestHeader;
var RemoveResponseHeader =
    chrome.declarativeWebRequest.RemoveResponseHeader;
var IgnoreRules =
    chrome.declarativeWebRequest.IgnoreRules;

// Constants as functions, not to be called until after runTests.
function getURLEchoUserAgent() {
  return getServerURL('echoheader?User-Agent');
}

function getURLHttpSimple() {
  return getServerURL("files/extensions/api_test/webrequest/simpleLoad/a.html");
}

function getURLHttpSimpleB() {
  return getServerURL("files/extensions/api_test/webrequest/simpleLoad/b.html");
}

function getURLHttpComplex() {
  return getServerURL(
      "files/extensions/api_test/webrequest/complexLoad/a.html");
}

function getURLHttpRedirectTest() {
  return getServerURL(
      "files/extensions/api_test/webrequest/declarative/a.html");
}

function getURLSetCookie() {
  return getServerURL('set-cookie?Foo=Bar');
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
      [ ["onErrorOccurred"] ]);
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

  function testRedirectRequest2() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onCompleted",
          event: "onCompleted",
          details: {
            ip: "127.0.0.1",
            url: getURLHttpRedirectTest(),
            fromCache: false,
            statusCode: 200,
            statusLine: "HTTP/1.0 200 OK",
          }
        },
        // We cannot wait for onCompleted signals because these are not sent
        // for data:// URLs.
        { label: "onBeforeRedirect-1",
          event: "onBeforeRedirect",
          details: {
            url: getServerURL(
                "files/extensions/api_test/webrequest/declarative/image.png"),
            redirectUrl: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEA" +
                "AAABCAYAAAAfFcSJAAAACklEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJ" +
                "ggg==",
            fromCache: false,
            statusCode: -1,
            statusLine: "",
            type: "image",
          }
        },
        { label: "onBeforeRedirect-2",
          event: "onBeforeRedirect",
          details: {
            frameId: 1,
            parentFrameId: 0,
            url: getServerURL(
                "files/extensions/api_test/webrequest/declarative/frame.html"),
            redirectUrl: "data:text/html,",
            fromCache: false,
            statusCode: -1,
            statusLine: "",
            type: "sub_frame",
          }
        },
      ],
      [ ["onCompleted"], ["onBeforeRedirect-1"], ["onBeforeRedirect-2"] ]);

    onRequest.addRules(
      [ {conditions: [
             new RequestMatcher({url: {pathSuffix: "image.png"}})],
         actions: [new RedirectToTransparentImage()]},
        {conditions: [
             new RequestMatcher({url: {pathSuffix: "frame.html"}})],
         actions: [new RedirectToEmptyDocument()]},
      ],
      function() {navigateAndWait(getURLHttpRedirectTest());}
    );
  },

  function testRedirectByRegEx() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onCompleted",
          event: "onCompleted",
          details: {
            ip: "127.0.0.1",
            url: getURLHttpSimpleB(),
            fromCache: false,
            statusCode: 200,
            statusLine: "HTTP/1.0 200 OK",
          }
        },
      ],
      [ ["onCompleted"] ]);

    onRequest.addRules(
      [ {conditions: [new RequestMatcher({url: {pathSuffix: ".html"}})],
         actions: [
             new RedirectByRegEx({from: "^(.*)/a.html$", to: "$1/b.html"})]}
      ],
      function() {navigateAndWait(getURLHttpSimple());}
    );
  },

  function testSetRequestHeader() {
    ignoreUnexpected = true;
    expect();  // Used for initialization.
    onRequest.addRules(
      [{conditions: [new RequestMatcher()],
        actions: [new SetRequestHeader({name: "User-Agent", value: "FoobarUA"})]
       }],
      function() {
        // Check the page content for our modified User-Agent string.
        navigateAndWait(getURLEchoUserAgent(), function() {
          chrome.test.listenOnce(chrome.extension.onRequest, function(request) {
            chrome.test.assertTrue(request.pass, "Request header was not set.");
          });
          chrome.tabs.executeScript(tabId,
            {
              code: "chrome.extension.sendRequest(" +
                    "{pass: document.body.innerText.indexOf('FoobarUA') >= 0});"
            });
        });
      });
  },

  function testRemoveRequestHeader() {
    ignoreUnexpected = true;
    expect();  // Used for initialization.
    onRequest.addRules(
      [{conditions: [new RequestMatcher()],
        actions: [new RemoveRequestHeader({name: "user-AGENT"})]
       }],
      function() {
        // Check the page content for our modified User-Agent string.
        navigateAndWait(getURLEchoUserAgent(), function() {
          chrome.test.listenOnce(chrome.extension.onRequest, function(request) {
            chrome.test.assertTrue(request.pass, "User-Agent was not removed.");
          });
          chrome.tabs.executeScript(tabId,
            {
              code: "chrome.extension.sendRequest(" +
                    "{pass: document.body.innerText.indexOf('Mozilla') == -1});"
            });
        });
      });
  },

  function testAddResponseHeader() {
    ignoreUnexpected = true;
    expect();  // Used for initialization.
    onRequest.addRules(
      [{conditions: [new RequestMatcher()],
        actions: [new AddResponseHeader({name: "Set-Cookie", value: "Bar=baz"})]
       }],
      function() {
        navigateAndWait(getURLEchoUserAgent(), function() {
          chrome.test.listenOnce(chrome.extension.onRequest, function(request) {
            chrome.test.assertTrue(request.pass, "Cookie was not added.");
          });
          chrome.tabs.executeScript(tabId,
            {
              code: "chrome.extension.sendRequest(" +
                    "{pass: document.cookie.indexOf('Bar') != -1});"
            });
        });
      });
  },

  function testRemoveResponseHeader() {
    ignoreUnexpected = true;
    expect();  // Used for initialization.
    onRequest.addRules(
      [{conditions: [new RequestMatcher()],
        actions: [new RemoveResponseHeader({name: "Set-Cookie",
                                            value: "FoO=bAR"})]
       }],
      function() {
        navigateAndWait(getURLSetCookie(), function() {
          chrome.test.listenOnce(chrome.extension.onRequest, function(request) {
            chrome.test.assertTrue(request.pass, "Cookie was not removed.");
          });
          chrome.tabs.executeScript(tabId,
            {
              code: "chrome.extension.sendRequest(" +
                    "{pass: document.cookie.indexOf('Foo') == -1});"
            });
        });
      });
  },

  function testPriorities() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onCompleted",
          event: "onCompleted",
          details: {
            url: getURLHttpSimple(),
            statusCode: 200,
            fromCache: false,
            statusLine: "HTTP/1.0 200 OK",
            ip: "127.0.0.1",
          }
        }
      ],
      [ ["onCompleted"] ]);

    onRequest.addRules(
      [ {conditions: [new RequestMatcher({url: {pathContains: "simpleLoad"}})],
         actions: [new CancelRequest()]},
        {conditions: [new RequestMatcher({url: {pathContains: "a.html"}})],
         actions: [new IgnoreRules({lowerPriorityThan: 200})],
         priority: 200}
      ],
      function() {navigateAndWait(getURLHttpSimple());}
    );
  },

  ]);
