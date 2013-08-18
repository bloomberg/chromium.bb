// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['net_internals_test.js']);

// Anonymous namespace
(function() {

TEST_F('NetInternalsTest', 'netInternalsWaterfallView', function() {
  function runTestCase(testCase) {
    var eventPairs = WaterfallRow.findUrlRequestEvents(testCase.sourceEntry);
    expectEquals(eventPairs.length, testCase.expectedEventPairs.length);
    for (var i = 0; i < eventPairs.length; ++i) {
      expectEquals(eventPairs[i].startEntry.time,
          testCase.expectedEventPairs[i].startTime);
      expectEquals(eventPairs[i].endEntry.time,
          testCase.expectedEventPairs[i].endTime);
      expectEquals(eventPairs[i].startEntry.type,
          testCase.expectedEventPairs[i].eventType);
      expectEquals(eventPairs[i].endEntry.type,
          testCase.expectedEventPairs[i].eventType);
    }
    SourceTracker.getInstance().clearEntries_();
  }

  function expectedEventPairsWithoutTcp() {
    var expectedEventPairs = [
      {
        startTime: "369047367",
        endTime: "369047371",
        eventType: EventType.PROXY_SERVICE
      },
      {
        startTime: "369047371",
        endTime: "369047372",
        eventType: EventType.HOST_RESOLVER_IMPL_REQUEST
      },
      {
        startTime: "369047398",
        endTime: "369047437",
        eventType: EventType.SSL_CONNECT
      },
      {
        startTime: "369047444",
        endTime: "369047474",
        eventType: EventType.HTTP_TRANSACTION_READ_HEADERS
      }
    ];
    return expectedEventPairs;
  }

  function logEntriesWithoutTcpConnection() {
    var logEntries = [
      {
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 111,
          "type": EventSourceType.URL_REQUEST
        },
        "time": "369047366",
        "type": EventType.REQUEST_ALIVE
      },
      {
        "params": {
          "load_flags": 16480,
          "method": "GET",
          "priority": 1,
          "url":
              "https://www.google.com/searchdomaincheck?format=url&type=chrome"
        },
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 111,
          "type": EventSourceType.URL_REQUEST
        },
        "time": "369047366",
        "type": EventType.URL_REQUEST_START_JOB
      },
      {
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 111,
          "type": EventSourceType.URL_REQUEST
        },
        "time": "369047367",
        "type": EventType.HTTP_STREAM_REQUEST
      },
      {
        "params": {
          "original_url": "https://www.google.com/",
          "priority": 1,
          "url": "https://www.google.com/"
        },
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 112,
          "type": EventSourceType.HTTP_STREAM_JOB
        },
        "time": "369047367",
        "type": EventType.HTTP_STREAM_JOB
      },
      {
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 112,
          "type": EventSourceType.HTTP_STREAM_JOB
        },
        "time": "369047367",
        "type": EventType.PROXY_SERVICE
      },
      {
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 112,
          "type": EventSourceType.HTTP_STREAM_JOB
        },
        "time": "369047371",
        "type": EventType.PROXY_SERVICE
      },
      {
        "params": {
          "source_dependency": {
            "id": 113,
            "type": EventSourceType.HOST_RESOLVER_IMPL_REQUEST
          }
        },
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 112,
          "type": EventSourceType.HTTP_STREAM_JOB
        },
        "time": "369047371",
        "type": EventType.HOST_RESOLVER_IMPL
      },
      {
        "params": {
          "address_family": 0,
          "allow_cached_response": true,
          "host": "www.google.com:443",
          "is_speculative": false,
          "priority": 3,
          "source_dependency": {
            "id": 112,
            "type": EventSourceType.HTTP_STREAM_JOB
          }
        },
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 113,
          "type": EventSourceType.HOST_RESOLVER_IMPL_REQUEST
        },
        "time": "369047371",
        "type": EventType.HOST_RESOLVER_IMPL_REQUEST
      },
      {
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 113,
          "type": EventSourceType.HOST_RESOLVER_IMPL_REQUEST
        },
        "time": "369047372",
        "type": EventType.HOST_RESOLVER_IMPL_REQUEST
      },
      {
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 112,
          "type": EventSourceType.HTTP_STREAM_JOB
        },
        "time": "369047372",
        "type": EventType.HOST_RESOLVER_IMPL
      },
      {
        "params": {
          "group_name": "ssl/www.google.com:443"
        },
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 115,
          "type": EventSourceType.CONNECT_JOB
        },
        "time": "369047372",
        "type": EventType.SOCKET_POOL_CONNECT_JOB
      },
      {
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 115,
          "type": EventSourceType.CONNECT_JOB
        },
        "time": "369047437",
        "type": EventType.SOCKET_POOL_CONNECT_JOB
      },
      {
        "params": {
          "source_dependency": {
            "id": 124,
            "type": EventSourceType.SOCKET
          }
        },
        "phase": EventPhase.PHASE_NONE,
        "source": {
          "id": 112,
          "type": EventSourceType.HTTP_STREAM_JOB
        },
        "time": "369047438",
        "type": EventType.SOCKET_POOL_BOUND_TO_SOCKET
      },
      {
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 112,
          "type": EventSourceType.HTTP_STREAM_JOB
        },
        "time": "369047438",
        "type": EventType.SOCKET_POOL
      },
      {
        "params": {
          "next_proto_status": "negotiated",
          "proto": "spdy/3",
          "server_protos": ""
        },
        "phase": EventPhase.PHASE_NONE,
        "source": {
          "id": 112,
          "type": EventSourceType.HTTP_STREAM_JOB
        },
        "time": "369047438",
        "type": EventType.HTTP_STREAM_REQUEST_PROTO
      },
      {
        "params": {
          "source_dependency": {
            "id": 126,
            "type": EventSourceType.HOST_RESOLVER_IMPL_REQUEST
          }
        },
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 112,
          "type": EventSourceType.HTTP_STREAM_JOB
        },
        "time": "369047438",
        "type": EventType.HOST_RESOLVER_IMPL
      },
      {
        "params": {
          "address_family": 0,
          "allow_cached_response": true,
          "host": "www.google.com:443",
          "is_speculative": false,
          "priority": 3,
          "source_dependency": {
            "id": 112,
            "type": EventSourceType.HTTP_STREAM_JOB
          }
        },
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 126,
          "type": EventSourceType.HOST_RESOLVER_IMPL_REQUEST
        },
        "time": "369047438",
        "type": EventType.HOST_RESOLVER_IMPL_REQUEST
      },
      {
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 112,
          "type": EventSourceType.HTTP_STREAM_JOB
        },
        "time": "369047439",
        "type": EventType.HOST_RESOLVER_IMPL
      },
      {
        "params": {
          "source_dependency": {
            "id": 112,
            "type": EventSourceType.HTTP_STREAM_JOB
          }
        },
        "phase": EventPhase.PHASE_NONE,
        "source": {
          "id": 111,
          "type": EventSourceType.URL_REQUEST
        },
        "time": "369047440",
        "type": EventType.HTTP_STREAM_REQUEST_BOUND_TO_JOB
      },
      {
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 111,
          "type": EventSourceType.URL_REQUEST
        },
        "time": "369047440",
        "type": EventType.HTTP_STREAM_REQUEST
      },
      {
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 112,
          "type": EventSourceType.HTTP_STREAM_JOB
        },
        "time": "369047440",
        "type": EventType.HTTP_STREAM_JOB
      },
      {
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 111,
          "type": EventSourceType.URL_REQUEST
        },
        "time": "369047444",
        "type": EventType.HTTP_TRANSACTION_READ_HEADERS
      },
      {
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 111,
          "type": EventSourceType.URL_REQUEST
        },
        "time": "369047474",
        "type": EventType.HTTP_TRANSACTION_READ_HEADERS
      },
      {
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 111,
          "type": EventSourceType.URL_REQUEST
        },
        "time": "369047475",
        "type": EventType.URL_REQUEST_START_JOB
      },
      {
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 111,
          "type": EventSourceType.URL_REQUEST
        },
        "time": "369047476",
        "type": EventType.REQUEST_ALIVE
      }
    ];
    return logEntries;
  }

  function urlRequestConnectionAfterProxyService() {
    var socketLogEntries = [
      {
        "params": {
          "source_dependency": {
            "id": 115,
            "type": EventSourceType.CONNECT_JOB,
          },
          "phase": EventPhase.PHASE_BEGIN
        },
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET,
        },
        "time": "369047210",
        "type": EventType.SOCKET_ALIVE
      },
      {
        "params": {
          "address_list": [
            "[2607:f8b0:4006:802::1010]:443",
            "74.125.226.211:443",
            "74.125.226.212:443",
            "74.125.226.208:443",
            "74.125.226.209:443",
            "74.125.226.210:443"
          ]
        },
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET
        },
        "time": "369047390",
        "type": EventType.TCP_CONNECT
      },
      {
        "params": {
          "source_address": "[2620:0:1004:2:be30:5bff:fedb:49b2]:42259"
        },
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET
        },
        "time": "369047397",
        "type": EventType.TCP_CONNECT
      },
      {
        "params": {
          "source_dependency": {
            "id": 124,
            "type": EventSourceType.SOCKET
          }
        },
        "phase": EventPhase.PHASE_NONE,
        "source": {
          "id": 115,
          "type": EventSourceType.CONNECT_JOB
        },
        "time": "369047398",
        "type": EventType.SOCKET_POOL_BOUND_TO_SOCKET
      },
      {
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET
        },
        "time": "369047398",
        "type": EventType.SSL_CONNECT
      },
      {
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET
        },
        "time": "369047437",
        "type": EventType.SSL_CONNECT
      }
    ];

    var tcpConnect = {
      startTime: "369047390",
      endTime: "369047397",
      eventType: EventType.TCP_CONNECT
    };

    var logEntries = logEntriesWithoutTcpConnection().concat(socketLogEntries);
    g_browser.receivedLogEntries(logEntries);

    var testCase = {};
    testCase.sourceEntry = SourceTracker.getInstance().getSourceEntry(111);

    var expectedEventPairs = expectedEventPairsWithoutTcp();
    expectedEventPairs.splice(2, 0, tcpConnect);
    testCase.expectedEventPairs = expectedEventPairs;

    return testCase;
  }

  function urlRequestConnectionInProxyService() {
    var socketLogEntries = [
      {
        "params": {
          "source_dependency": {
            "id": 115,
            "type": EventSourceType.CONNECT_JOB,
          },
          "phase": EventPhase.PHASE_BEGIN
        },
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET,
        },
        "time": "369047210",
        "type": EventType.SOCKET_ALIVE
      },
      {
        "params": {
          "address_list": [
            "[2607:f8b0:4006:802::1010]:443",
            "74.125.226.211:443",
            "74.125.226.212:443",
            "74.125.226.208:443",
            "74.125.226.209:443",
            "74.125.226.210:443"
          ]
        },
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET
        },
        "time": "369047370",
        "type": EventType.TCP_CONNECT
      },
      {
        "params": {
          "source_address": "[2620:0:1004:2:be30:5bff:fedb:49b2]:42259"
        },
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET
        },
        "time": "369047397",
        "type": EventType.TCP_CONNECT
      },
      {
        "params": {
          "source_dependency": {
            "id": 124,
            "type": EventSourceType.SOCKET
          }
        },
        "phase": EventPhase.PHASE_NONE,
        "source": {
          "id": 115,
          "type": EventSourceType.CONNECT_JOB
        },
        "time": "369047398",
        "type": EventType.SOCKET_POOL_BOUND_TO_SOCKET
      },
      {
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET
        },
        "time": "369047398",
        "type": EventType.SSL_CONNECT
      },
      {
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET
        },
        "time": "369047437",
        "type": EventType.SSL_CONNECT
      }
    ];

    var tcpConnect = {
      startTime: "369047371",
      endTime: "369047397",
      eventType: EventType.TCP_CONNECT
    };

    var logEntries = logEntriesWithoutTcpConnection().concat(socketLogEntries);
    g_browser.receivedLogEntries(logEntries);

    var testCase = {};
    testCase.sourceEntry = SourceTracker.getInstance().getSourceEntry(111);

    var expectedEventPairs = expectedEventPairsWithoutTcp();
    expectedEventPairs.splice(2, 0, tcpConnect);
    testCase.expectedEventPairs = expectedEventPairs;

    return testCase;
  }

  function urlRequestConnectionBeforeProxyService() {

    var socketLogEntries = [
      {
        "params": {
          "source_dependency": {
            "id": 115,
            "type": EventSourceType.CONNECT_JOB,
          },
          "phase": EventPhase.PHASE_BEGIN
        },
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET,
        },
        "time": "369047210",
        "type": EventType.SOCKET_ALIVE
      },
      {
        "params": {
          "address_list": [
            "[2607:f8b0:4006:802::1010]:443",
            "74.125.226.211:443",
            "74.125.226.212:443",
            "74.125.226.208:443",
            "74.125.226.209:443",
            "74.125.226.210:443"
          ]
        },
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET
        },
        "time": "369047350",
        "type": EventType.TCP_CONNECT
      },
      {
        "params": {
          "source_address": "[2620:0:1004:2:be30:5bff:fedb:49b2]:42259"
        },
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET
        },
        "time": "369047357",
        "type": EventType.TCP_CONNECT
      },
      {
        "params": {
          "source_dependency": {
            "id": 124,
            "type": EventSourceType.SOCKET
          }
        },
        "phase": EventPhase.PHASE_NONE,
        "source": {
          "id": 115,
          "type": EventSourceType.CONNECT_JOB
        },
        "time": "369047398",
        "type": EventType.SOCKET_POOL_BOUND_TO_SOCKET
      },
      {
        "phase": EventPhase.PHASE_BEGIN,
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET
        },
        "time": "369047398",
        "type": EventType.SSL_CONNECT
      },
      {
        "phase": EventPhase.PHASE_END,
        "source": {
          "id": 124,
          "type": EventSourceType.SOCKET
        },
        "time": "369047437",
        "type": EventType.SSL_CONNECT
      }
    ];
    var logEntries = logEntriesWithoutTcpConnection().concat(socketLogEntries);
    g_browser.receivedLogEntries(logEntries);

    var testCase = {};
    testCase.sourceEntry = SourceTracker.getInstance().getSourceEntry(111);
    testCase.expectedEventPairs = expectedEventPairsWithoutTcp();

    return testCase;
  }

  runTestCase(urlRequestConnectionAfterProxyService());
  runTestCase(urlRequestConnectionInProxyService());
  runTestCase(urlRequestConnectionBeforeProxyService());

  testDone();
});

})();  // Anonymous namespace