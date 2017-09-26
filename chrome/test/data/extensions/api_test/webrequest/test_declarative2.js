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
var AddRequestCookie = chrome.declarativeWebRequest.AddRequestCookie;
var AddResponseCookie = chrome.declarativeWebRequest.AddResponseCookie;
var EditRequestCookie = chrome.declarativeWebRequest.EditRequestCookie;
var EditResponseCookie = chrome.declarativeWebRequest.EditResponseCookie;
var RemoveRequestCookie = chrome.declarativeWebRequest.RemoveRequestCookie;
var RemoveResponseCookie = chrome.declarativeWebRequest.RemoveResponseCookie;

// Constants as functions, not to be called until after runTests.
function getURLEchoUserAgent() {
  return getServerURL('echoheader?User-Agent');
}

function getURLHttpSimple() {
  return getServerURL("extensions/api_test/webrequest/simpleLoad/a.html");
}

function getURLOfHTMLWithThirdParty() {
  // Returns the URL of a HTML document with a third-party resource.
  return getServerURL(
      "extensions/api_test/webrequest/declarative/third-party.html");
}

function getURLSetCookie() {
  return getServerURL('set-cookie?Foo=Bar');
}

function getURLSetCookie2() {
  return getServerURL('set-cookie?passedCookie=Foo&editedCookie=Foo&' +
                      'deletedCookie=Foo');
}

function getURLEchoCookie() {
  return getServerURL('echoheader?Cookie');
}

function getURLHttpXHRData() {
  return getServerURL("extensions/api_test/webrequest/xhr/data.json",
                      "b.com");
}

runTests([

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
            statusLine: "HTTP/1.1 200 OK",
            ip: "127.0.0.1",
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
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

  function testEditRequestCookies() {
    ignoreUnexpected = true;
    expect();
    var cookie1 = {name: "requestCookie1", value: "foo"};
    var cookie2 = {name: "requestCookie2", value: "foo"};
    onRequest.addRules(
      [ {conditions: [new RequestMatcher({})],
         actions: [
           // We exploit the fact that cookies are first added, then modified
           // and finally removed.
           new AddRequestCookie({cookie: cookie1}),
           new AddRequestCookie({cookie: cookie2}),
           new EditRequestCookie({filter: {name: "requestCookie1"},
                                  modification: {value: "bar"}}),
           new RemoveRequestCookie({filter: {name: "requestCookie2"}})
         ]}
      ],
      function() {
        navigateAndWait(getURLEchoCookie(), function() {
          chrome.test.listenOnce(chrome.extension.onRequest, function(request) {
            chrome.test.assertTrue(request.pass, "Invalid cookies. " +
                JSON.stringify(request.cookies));
          });
          chrome.tabs.executeScript(tabId, {code:
              "function hasCookie(name, value) {" +
              "  var entry = name + '=' + value;" +
              "  return document.body.innerText.indexOf(entry) >= 0;" +
              "};" +
              "var result = {};" +
              "result.pass = hasCookie('requestCookie1', 'bar') && " +
              "              !hasCookie('requestCookie1', 'foo') && " +
              "              !hasCookie('requestCookie2', 'foo');" +
              "result.cookies = document.body.innerText;" +
              "chrome.extension.sendRequest(result);"});
        });
      }
    );
  },

  function testEditResponseCookies() {
    ignoreUnexpected = true;
    expect();
    onRequest.addRules(
      [ {conditions: [new RequestMatcher({})],
         actions: [
           new AddResponseCookie({cookie: {name: "addedCookie", value: "Foo"}}),
           new EditResponseCookie({filter: {name: "editedCookie"},
                                   modification: {value: "bar"}}),
           new RemoveResponseCookie({filter: {name: "deletedCookie"}})
         ]}
      ],
      function() {
        navigateAndWait(getURLSetCookie2(), function() {
          chrome.test.listenOnce(chrome.extension.onRequest, function(request) {
            chrome.test.assertTrue(request.pass, "Invalid cookies. " +
                JSON.stringify(request.cookies));
          });
          chrome.tabs.executeScript(tabId, {code:
              "var cookies = document.cookie.split('; ');" +
              "var cookieMap = {};" +
              "for (var i = 0; i < cookies.length; ++i) {" +
              "  var cookieParts = cookies[i].split('=');" +
              "  cookieMap[cookieParts[0]] = cookieParts[1];" +
              "}" +
              "var result = {};" +
              "result.cookies = cookieMap;" +
              "result.pass = (cookieMap.passedCookie === 'Foo') &&" +
              "              (cookieMap.addedCookie === 'Foo') &&" +
              "              (cookieMap.editedCookie === 'bar') &&" +
              "              !cookieMap.hasOwnProperty('deletedCookie');" +
              "chrome.extension.sendRequest(result);"});
        });
      }
    );
  },

  function testRequestHeaders() {
    ignoreUnexpected = true;
    expect(
      [
        { label: "onErrorOccurred",
          event: "onErrorOccurred",
          details: {
            url: getURLHttpSimple(),
            fromCache: false,
            error: "net::ERR_BLOCKED_BY_CLIENT",
            initiator: getServerDomain(initiators.BROWSER_INITIATED)
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
             'requestHeaders': [{ nameContains: "" }],
             'excludeRequestHeaders': [{ valueContains: ["", "value123"] }]
              })],
         'actions': [new CancelRequest()]}
      ],
      function() {navigateAndWait(getURLHttpSimple());}
    );
  },
  ]);
