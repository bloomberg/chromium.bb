// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getStyleURL() {
  // This is not a style sheet, but it must still be classified as "stylesheet"
  // because it is loaded as a style sheet.
  return getServerURL('empty.html?as-style');
}

function getFontURL() {
  // Not a font, but will be loaded as a font.
  return getServerURL('empty.html?as-font');
}

function getWorkerURL() {
  // This file is empty, so it does not generate JavaScript errors when loaded
  // as a worker script.
  return getServerURL('empty.html?as-worker');
}

function getPingURL() {
  return getServerURL('empty.html?as-ping');
}

function getBeaconURL() {
  return getServerURL('empty.html?as-beacon');
}

runTests([
  function typeStylesheet() {
    expect([
      { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          type: 'stylesheet',
          url: getStyleURL(),
          frameUrl: 'unknown frame URL',
          // tabId 0 = tab opened by test runner;
          // tabId 1 = this tab.
          tabId: 1,
        }
      },
      { label: 'onBeforeSendHeaders',
        event: 'onBeforeSendHeaders',
        details: {
          type: 'stylesheet',
          url: getStyleURL(),
          tabId: 1,
        },
      },
      { label: 'onSendHeaders',
        event: 'onSendHeaders',
        details: {
          type: 'stylesheet',
          url: getStyleURL(),
          tabId: 1,
        },
      },
      { label: 'onHeadersReceived',
        event: 'onHeadersReceived',
        details: {
          type: 'stylesheet',
          url: getStyleURL(),
          tabId: 1,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      },
      { label: 'onResponseStarted',
        event: 'onResponseStarted',
        details: {
          type: 'stylesheet',
          url: getStyleURL(),
          tabId: 1,
          ip: '127.0.0.1',
          fromCache: false,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      },
      { label: 'onCompleted',
        event: 'onCompleted',
        details: {
          type: 'stylesheet',
          url: getStyleURL(),
          tabId: 1,
          ip: '127.0.0.1',
          fromCache: false,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      }],
      [['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
        'onHeadersReceived', 'onResponseStarted', 'onCompleted']]);
    var style = document.createElement('link');
    style.rel = 'stylesheet';
    style.type = 'text/css';
    style.href = getStyleURL();
    document.body.appendChild(style);
  },

  function typeFont() {
    expect([
      { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          type: 'font',
          url: getFontURL(),
          frameUrl: 'unknown frame URL',
          // tabId 0 = tab opened by test runner;
          // tabId 1 = this tab.
          tabId: 1,
        }
      },
      { label: 'onBeforeSendHeaders',
        event: 'onBeforeSendHeaders',
        details: {
          type: 'font',
          url: getFontURL(),
          tabId: 1,
        },
      },
      { label: 'onSendHeaders',
        event: 'onSendHeaders',
        details: {
          type: 'font',
          url: getFontURL(),
          tabId: 1,
        },
      },
      { label: 'onHeadersReceived',
        event: 'onHeadersReceived',
        details: {
          type: 'font',
          url: getFontURL(),
          tabId: 1,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      },
      { label: 'onResponseStarted',
        event: 'onResponseStarted',
        details: {
          type: 'font',
          url: getFontURL(),
          tabId: 1,
          ip: '127.0.0.1',
          fromCache: false,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      },
      { label: 'onCompleted',
        event: 'onCompleted',
        details: {
          type: 'font',
          url: getFontURL(),
          tabId: 1,
          ip: '127.0.0.1',
          fromCache: false,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      }],
      [['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
        'onHeadersReceived', 'onResponseStarted', 'onCompleted']]);

    new FontFace('allegedly-a-font-family', 'url(' + getFontURL() + ')').load();
  },

  function typeWorker() {
    expect([
      { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          type: 'script',
          url: getWorkerURL(),
          frameUrl: 'unknown frame URL',
          // tabId 0 = tab opened by test runner;
          // tabId 1 = this tab.
          tabId: 1,
        }
      },
      { label: 'onBeforeSendHeaders',
        event: 'onBeforeSendHeaders',
        details: {
          type: 'script',
          url: getWorkerURL(),
          tabId: 1,
        },
      },
      { label: 'onSendHeaders',
        event: 'onSendHeaders',
        details: {
          type: 'script',
          url: getWorkerURL(),
          tabId: 1,
        },
      },
      { label: 'onHeadersReceived',
        event: 'onHeadersReceived',
        details: {
          type: 'script',
          url: getWorkerURL(),
          tabId: 1,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      },
      { label: 'onResponseStarted',
        event: 'onResponseStarted',
        details: {
          type: 'script',
          url: getWorkerURL(),
          tabId: 1,
          ip: '127.0.0.1',
          fromCache: false,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      },
      { label: 'onCompleted',
        event: 'onCompleted',
        details: {
          type: 'script',
          url: getWorkerURL(),
          tabId: 1,
          ip: '127.0.0.1',
          fromCache: false,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      }],
      [['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
        'onHeadersReceived', 'onResponseStarted', 'onCompleted']]);

    new Worker(getWorkerURL());

    // TODO(robwu): add tests for SharedWorker and ServiceWorker.
    // (probably same as above, but using -1 because they are not specific to
    //  any tab. In general we really need a lot more test coverage for the
    //  interaction between Service workers and extensions.)
  },

  function typePing() {
    expect([
      { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          type: 'ping',
          method: 'POST',
          url: getPingURL(),
          frameUrl: 'unknown frame URL',
          // TODO(robwu): Ping / beacons are not associated with a tab/frame
          // because they are detached requests. However, it would be useful if
          // the frameId and tabId are set to the source of the request.
          // See crbug.com/522124.
          frameId: -1,
          tabId: -1,
        }
      },
      { label: 'onBeforeSendHeaders',
        event: 'onBeforeSendHeaders',
        details: {
          type: 'ping',
          method: 'POST',
          url: getPingURL(),
          frameId: -1,
          tabId: -1,
        },
      },
      { label: 'onSendHeaders',
        event: 'onSendHeaders',
        details: {
          type: 'ping',
          method: 'POST',
          url: getPingURL(),
          frameId: -1,
          tabId: -1,
        },
      },
      { label: 'onHeadersReceived',
        event: 'onHeadersReceived',
        details: {
          type: 'ping',
          method: 'POST',
          url: getPingURL(),
          frameId: -1,
          tabId: -1,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      },
      { label: 'onResponseStarted',
        event: 'onResponseStarted',
        details: {
          type: 'ping',
          method: 'POST',
          url: getPingURL(),
          frameId: -1,
          tabId: -1,
          ip: '127.0.0.1',
          fromCache: false,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      },
      { label: 'onCompleted',
        event: 'onCompleted',
        details: {
          type: 'ping',
          method: 'POST',
          url: getPingURL(),
          frameId: -1,
          tabId: -1,
          ip: '127.0.0.1',
          fromCache: false,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      }],
      [['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
        'onHeadersReceived', 'onResponseStarted', 'onCompleted']]);

    var a = document.createElement('a');
    a.ping = getPingURL();
    a.href = 'javascript:';
    a.click();
  },

  function typeBeacon() {
    expect([
      { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          type: 'ping',
          method: 'POST',
          url: getBeaconURL(),
          frameUrl: 'unknown frame URL',
          // TODO(robwu): these IDs should not be -1. See comment at typePing.
          frameId: -1,
          tabId: -1,
        }
      },
      { label: 'onBeforeSendHeaders',
        event: 'onBeforeSendHeaders',
        details: {
          type: 'ping',
          method: 'POST',
          url: getBeaconURL(),
          frameId: -1,
          tabId: -1,
        },
      },
      { label: 'onSendHeaders',
        event: 'onSendHeaders',
        details: {
          type: 'ping',
          method: 'POST',
          url: getBeaconURL(),
          frameId: -1,
          tabId: -1,
        },
      },
      { label: 'onHeadersReceived',
        event: 'onHeadersReceived',
        details: {
          type: 'ping',
          method: 'POST',
          url: getBeaconURL(),
          frameId: -1,
          tabId: -1,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      },
      { label: 'onResponseStarted',
        event: 'onResponseStarted',
        details: {
          type: 'ping',
          method: 'POST',
          url: getBeaconURL(),
          frameId: -1,
          tabId: -1,
          ip: '127.0.0.1',
          fromCache: false,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      },
      { label: 'onCompleted',
        event: 'onCompleted',
        details: {
          type: 'ping',
          method: 'POST',
          url: getBeaconURL(),
          frameId: -1,
          tabId: -1,
          ip: '127.0.0.1',
          fromCache: false,
          statusLine: 'HTTP/1.1 200 OK',
          statusCode: 200,
        },
      }],
      [['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
        'onHeadersReceived', 'onResponseStarted', 'onCompleted']]);

    navigator.sendBeacon(getBeaconURL(), 'beacon data');
  },
]);
