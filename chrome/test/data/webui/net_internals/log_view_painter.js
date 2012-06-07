// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['net_internals_test.js']);

// Anonymous namespace
(function() {

/**
 * Check that stripCookiesAndLoginInfo correctly removes cookies and login
 * information.
 */
TEST_F('NetInternalsTest', 'netInternalsLogViewPainterStripInfo', function() {
  // Each entry in |expectations| is a list consisting of a header element
  // before and after applying the filter.  If the second entry is null, the
  // element should be unmodified.
  var expectations = [
    ['set-cookie: blah', 'set-cookie: [value was stripped]'],
    ['set-cookie2: blah', 'set-cookie2: [value was stripped]'],
    ['cookie: blah', 'cookie: [value was stripped]'],
    ['authorization: NTLM blah', 'authorization: NTLM [value was stripped]'],

    ['proxy-authorization: Basic blah',
     'proxy-authorization: Basic [value was stripped]'],

    ['WWW-Authenticate: Basic realm="Something, or another"', null],

    ['WWW-Authenticate: Negotiate blah-token-blah',
     'WWW-Authenticate: Negotiate [value was stripped]'],

    ['WWW-Authenticate: NTLM asdllk2j3l423lk4j23l4kj',
     'WWW-Authenticate: NTLM [value was stripped]'],

    ['WWW-Authenticate: Kerberos , Negotiate asdfasdfasdfasfa', null],
    ['WWW-Authenticate: Kerberos, Negotiate asdfasdfasdfasfa', null],
    ['WWW-Authenticate: Digest , Negotiate asdfasdfasdfasfa', null],
    ['WWW-Authenticate: Digest realm="Foo realm", Negotiate asdf', null],
    ['WWW-Authenticate: Kerberos,Digest,Basic', null],
    ['WWW-Authenticate: Digest realm="asdfasdf", nonce=5, qop="auth"', null],
    ['WWW-Authenticate: Basic realm=foo,foo=bar , Digest ', null],
    ['Proxy-Authenticate: Basic realm="Something, or another"', null],

    ['Proxy-Authenticate: Negotiate blah-token-blah',
     'Proxy-Authenticate: Negotiate [value was stripped]'],

    ['Proxy-Authenticate: NTLM asdllk2j3l423lk4j23l4kj',
     'Proxy-Authenticate: NTLM [value was stripped]'],

    ['Proxy-Authenticate: Kerberos , Negotiate asdfasdfa', null],
    ['Proxy-Authenticate: Kerberos, Negotiate asdfasdfa', null],
    ['Proxy-Authenticate: Digest , Negotiate asdfasdfa', null],
    ['Proxy-Authenticate: Digest realm="Foo realm", Negotiate asdfasdfa', null],
    ['Proxy-Authenticate: Kerberos,Digest,Basic', null],
    ['Proxy-Authenticate: Digest realm="asdfasdf", nonce=5, qop="auth"', null],
    ['Proxy-Authenticate: Basic realm=foo,foo=bar , Digest ', null]
  ];

  for (var i = 0; i < expectations.length; ++i) {
    var expectation = expectations[i];
    // Position within params.headers where the authentication information goes.
    for (var position = 0; position < 3; ++position) {
      var entry = {
          'params': {
              'headers': [
                  'Host: clients1.google.com',
                  'Connection: keep-alive',
                  'User-Agent: Mozilla/5.0'],
              'line': 'GET / HTTP/1.1\r\n'},
          'phase': 0,
          'source': {'id': 329, 'type': 1},
          'time': '22468349',
          'type': 104};

      entry.params.headers[position] = expectation[0];
      var stripped = stripCookiesAndLoginInfo(entry);
      // The entry should be duplicated, so the original still has the deleted
      // information.
      expectNotEquals(stripped, entry);
      if (expectation[1] == null) {
        expectEquals(stripped.params.headers[position], expectation[0]);
      } else {
        expectEquals(stripped.params.headers[position], expectation[1]);
      }
    }
  }
  testDone();
});

/**
 * Tests the formatting of log entries to fixed width text.
 */
TEST_F('NetInternalsTest', 'netInternalsLogViewPainterPrintAsText', function() {
  // Add a DOM node to draw the log entries into.
  var div = addNode(document.body, 'div');

  // Helper function to run a particular "test case". This comprises an input
  // and the resulting formatted text expectation.
  function runTestCase(testCase) {
    div.innerHTML = '';
    timeutil.setTimeTickOffset(testCase.tickOffset);
    printLogEntriesAsText(testCase.logEntries, div,
                          testCase.enableSecurityStripping);

    // Strip any trailing newlines, since the whitespace when using innerText
    // can be a bit unpredictable.
    var actualText = div.innerText;
    actualText = actualText.replace(/^\s+|\s+$/g, '');

    expectEquals(testCase.expectedText, actualText);
  }

  runTestCase(painterTestURLRequest());
  runTestCase(painterTestNetError());
  runTestCase(painterTestHexEncodedBytes());
  runTestCase(painterTestCertVerifierJob());
  runTestCase(painterTestProxyConfig());
  runTestCase(painterTestDontStripCookiesURLRequest());
  runTestCase(painterTestStripCookiesURLRequest());
  runTestCase(painterTestDontStripCookiesSPDYSession());
  runTestCase(painterTestStripCookiesSPDYSession());
  runTestCase(painterTestExtraCustomParameter());
  runTestCase(painterTestMissingCustomParameter());
  runTestCase(painterTestSSLVersionFallback());

  testDone();
});

/**
 * Test case for a URLRequest. This includes custom formatting for load flags,
 * request/response HTTP headers, dependent sources, as well as basic
 * indentation and grouping.
 */
function painterTestURLRequest() {
  var testCase = {};
  testCase.tickOffset = '1337911098446';

  testCase.logEntries = [
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534778',
      'type': LogEventType.REQUEST_ALIVE
    },
    {
      'params': {
        'load_flags': 68223104,
        'method': 'GET',
        'priority': 4,
        'url': 'http://www.google.com/'
      },
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534792',
      'type': LogEventType.URL_REQUEST_START_JOB
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534800',
      'type': LogEventType.URL_REQUEST_START_JOB
    },
    {
      'params': {
        'load_flags': 68223104,
        'method': 'GET',
        'priority': 4,
        'url': 'http://www.google.com/'
      },
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534802',
      'type': LogEventType.URL_REQUEST_START_JOB
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534809',
      'type': LogEventType.HTTP_CACHE_GET_BACKEND
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534810',
      'type': LogEventType.HTTP_CACHE_GET_BACKEND
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534811',
      'type': LogEventType.HTTP_CACHE_OPEN_ENTRY
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534816',
      'type': LogEventType.HTTP_CACHE_OPEN_ENTRY
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534817',
      'type': LogEventType.HTTP_CACHE_ADD_TO_ENTRY
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534818',
      'type': LogEventType.HTTP_CACHE_ADD_TO_ENTRY
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534823',
      'type': LogEventType.HTTP_CACHE_READ_INFO
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534827',
      'type': LogEventType.HTTP_CACHE_READ_INFO
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534830',
      'type': LogEventType.HTTP_STREAM_REQUEST
    },
    {
      'params': {
        'source_dependency': {
          'id': 149,
          'type': 11
        }
      },
      'phase': LogEventPhase.PHASE_NONE,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534898',
      'type': LogEventType.HTTP_STREAM_REQUEST_BOUND_TO_JOB
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534902',
      'type': LogEventType.HTTP_STREAM_REQUEST
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534906',
      'type': LogEventType.HTTP_TRANSACTION_SEND_REQUEST
    },
    {
      'params': {
        'headers': [
          'Host: www.google.com',
          'Connection: keep-alive',
          'User-Agent: Mozilla/5.0',
          'Accept: text/html',
          'Accept-Encoding: gzip,deflate,sdch',
          'Accept-Language: en-US,en;q=0.8',
          'Accept-Charset: ISO-8859-1'
        ],
        'line': 'GET / HTTP/1.1\r\n'
      },
      'phase': LogEventPhase.PHASE_NONE,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534910',
      'type': LogEventType.HTTP_TRANSACTION_SEND_REQUEST_HEADERS
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534915',
      'type': LogEventType.HTTP_TRANSACTION_SEND_REQUEST
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534916',
      'type': LogEventType.HTTP_TRANSACTION_READ_HEADERS
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534917',
      'type': LogEventType.HTTP_STREAM_PARSER_READ_HEADERS
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534987',
      'type': LogEventType.HTTP_STREAM_PARSER_READ_HEADERS
    },
    {
      'params': {
        'headers': [
          'HTTP/1.1 200 OK',
          'Date: Tue, 05 Jun 2012 02:50:33 GMT',
          'Expires: -1',
          'Cache-Control: private, max-age=0',
          'Content-Type: text/html; charset=UTF-8',
          'Content-Encoding: gzip',
          'Server: gws',
          'Content-Length: 23798',
        ]
      },
      'phase': LogEventPhase.PHASE_NONE,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534989',
      'type': LogEventType.HTTP_TRANSACTION_READ_RESPONSE_HEADERS
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534992',
      'type': LogEventType.HTTP_TRANSACTION_READ_HEADERS
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534993',
      'type': LogEventType.HTTP_CACHE_WRITE_INFO
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535023',
      'type': LogEventType.HTTP_CACHE_WRITE_INFO
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535024',
      'type': LogEventType.HTTP_CACHE_WRITE_DATA
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535062',
      'type': LogEventType.HTTP_CACHE_WRITE_DATA
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535062',
      'type': LogEventType.HTTP_CACHE_WRITE_INFO
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535075',
      'type': LogEventType.HTTP_CACHE_WRITE_INFO
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535081',
      'type': LogEventType.URL_REQUEST_START_JOB
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535537',
      'type': LogEventType.HTTP_TRANSACTION_READ_BODY
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535538',
      'type': LogEventType.HTTP_TRANSACTION_READ_BODY
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535538',
      'type': LogEventType.HTTP_CACHE_WRITE_DATA
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535541',
      'type': LogEventType.HTTP_CACHE_WRITE_DATA
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535542',
      'type': LogEventType.HTTP_TRANSACTION_READ_BODY
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535545',
      'type': LogEventType.HTTP_TRANSACTION_READ_BODY
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535546',
      'type': LogEventType.HTTP_CACHE_WRITE_DATA
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535548',
      'type': LogEventType.HTTP_CACHE_WRITE_DATA
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535548',
      'type': LogEventType.HTTP_TRANSACTION_READ_BODY
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535548',
      'type': LogEventType.HTTP_TRANSACTION_READ_BODY
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535549',
      'type': LogEventType.HTTP_CACHE_WRITE_DATA
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535549',
      'type': LogEventType.HTTP_CACHE_WRITE_DATA
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535550',
      'type': LogEventType.HTTP_TRANSACTION_READ_BODY
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535550',
      'type': LogEventType.HTTP_TRANSACTION_READ_BODY
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535550',
      'type': LogEventType.HTTP_CACHE_WRITE_DATA
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535551',
      'type': LogEventType.HTTP_CACHE_WRITE_DATA
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535552',
      'type': LogEventType.HTTP_TRANSACTION_READ_BODY
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535553',
      'type': LogEventType.HTTP_TRANSACTION_READ_BODY
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535553',
      'type': LogEventType.HTTP_CACHE_WRITE_DATA
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535556',
      'type': LogEventType.HTTP_CACHE_WRITE_DATA
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535556',
      'type': LogEventType.HTTP_TRANSACTION_READ_BODY
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535559',
      'type': LogEventType.HTTP_TRANSACTION_READ_BODY
    },
    {
      'phase': LogEventPhase.PHASE_BEGIN,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535559',
      'type': LogEventType.HTTP_CACHE_WRITE_DATA
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535559',
      'type': LogEventType.HTTP_CACHE_WRITE_DATA
    },
    {
      'phase': LogEventPhase.PHASE_END,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953535567',
      'type': LogEventType.REQUEST_ALIVE
    }
  ];

  testCase.expectedText =
't=1338864633224 [st=  0] +REQUEST_ALIVE  [dt=789]\n' +
't=1338864633238 [st= 14]    URL_REQUEST_START_JOB  [dt=8]\n' +
'                            --> load_flags = 68223104 ' +
    '(ENABLE_LOAD_TIMING | MAIN_FRAME | MAYBE_USER_GESTURE ' +
    '| VERIFY_EV_CERT)\n' +
'                            --> method = "GET"\n' +
'                            --> priority = 4\n' +
'                            --> url = "http://www.google.com/"\n' +
't=1338864633248 [st= 24]   +URL_REQUEST_START_JOB  [dt=279]\n' +
'                            --> load_flags = 68223104 ' +
    '(ENABLE_LOAD_TIMING | MAIN_FRAME | MAYBE_USER_GESTURE ' +
    '| VERIFY_EV_CERT)\n' +
'                            --> method = "GET"\n' +
'                            --> priority = 4\n' +
'                            --> url = "http://www.google.com/"\n' +
't=1338864633255 [st= 31]      HTTP_CACHE_GET_BACKEND  [dt=1]\n' +
't=1338864633257 [st= 33]      HTTP_CACHE_OPEN_ENTRY  [dt=5]\n' +
't=1338864633263 [st= 39]      HTTP_CACHE_ADD_TO_ENTRY  [dt=1]\n' +
't=1338864633269 [st= 45]      HTTP_CACHE_READ_INFO  [dt=4]\n' +
't=1338864633276 [st= 52]     +HTTP_STREAM_REQUEST  [dt=72]\n' +
't=1338864633344 [st=120]        HTTP_STREAM_REQUEST_BOUND_TO_JOB\n' +
'                                --> source_dependency = 149 ' +
    '(HTTP_STREAM_JOB)\n' +
't=1338864633348 [st=124]     -HTTP_STREAM_REQUEST\n' +
't=1338864633352 [st=128]     +HTTP_TRANSACTION_SEND_REQUEST  [dt=9]\n' +
't=1338864633356 [st=132]        HTTP_TRANSACTION_SEND_REQUEST_HEADERS\n' +
'                                --> GET / HTTP/1.1\n' +
'                                    Host: www.google.com\n' +
'                                    Connection: keep-alive\n' +
'                                    User-Agent: Mozilla/5.0\n' +
'                                    Accept: text/html\n' +
'                                    Accept-Encoding: gzip,deflate,sdch\n' +
'                                    Accept-Language: en-US,en;q=0.8\n' +
'                                    Accept-Charset: ISO-8859-1\n' +
't=1338864633361 [st=137]     -HTTP_TRANSACTION_SEND_REQUEST\n' +
't=1338864633362 [st=138]     +HTTP_TRANSACTION_READ_HEADERS  [dt=76]\n' +
't=1338864633363 [st=139]        HTTP_STREAM_PARSER_READ_HEADERS  [dt=70]\n' +
't=1338864633435 [st=211]        HTTP_TRANSACTION_READ_RESPONSE_HEADERS\n' +
'                                --> HTTP/1.1 200 OK\n' +
'                                    Date: Tue, 05 Jun 2012 02:50:33 GMT\n' +
'                                    Expires: -1\n' +
'                                    Cache-Control: private, max-age=0\n' +
'                                    Content-Type: text/html; charset=UTF-8\n' +
'                                    Content-Encoding: gzip\n' +
'                                    Server: gws\n' +
'                                    Content-Length: 23798\n' +
't=1338864633438 [st=214]     -HTTP_TRANSACTION_READ_HEADERS\n' +
't=1338864633439 [st=215]      HTTP_CACHE_WRITE_INFO  [dt=30]\n' +
't=1338864633470 [st=246]      HTTP_CACHE_WRITE_DATA  [dt=38]\n' +
't=1338864633508 [st=284]      HTTP_CACHE_WRITE_INFO  [dt=13]\n' +
't=1338864633527 [st=303]   -URL_REQUEST_START_JOB\n' +
't=1338864633983 [st=759]    HTTP_TRANSACTION_READ_BODY  [dt=1]\n' +
't=1338864633984 [st=760]    HTTP_CACHE_WRITE_DATA  [dt=3]\n' +
't=1338864633988 [st=764]    HTTP_TRANSACTION_READ_BODY  [dt=3]\n' +
't=1338864633992 [st=768]    HTTP_CACHE_WRITE_DATA  [dt=2]\n' +
't=1338864633994 [st=770]    HTTP_TRANSACTION_READ_BODY  [dt=0]\n' +
't=1338864633995 [st=771]    HTTP_CACHE_WRITE_DATA  [dt=0]\n' +
't=1338864633996 [st=772]    HTTP_TRANSACTION_READ_BODY  [dt=0]\n' +
't=1338864633996 [st=772]    HTTP_CACHE_WRITE_DATA  [dt=1]\n' +
't=1338864633998 [st=774]    HTTP_TRANSACTION_READ_BODY  [dt=1]\n' +
't=1338864633999 [st=775]    HTTP_CACHE_WRITE_DATA  [dt=3]\n' +
't=1338864634002 [st=778]    HTTP_TRANSACTION_READ_BODY  [dt=3]\n' +
't=1338864634005 [st=781]    HTTP_CACHE_WRITE_DATA  [dt=0]\n' +
't=1338864634013 [st=789] -REQUEST_ALIVE';

  return testCase;
}

/**
 * Tests the custom formatting of net_errors across several different event
 * types.
 */
function painterTestNetError() {
  var testCase = {};
  testCase.tickOffset = '1337911098446';

  testCase.logEntries = [
     {
       'phase': LogEventPhase.PHASE_BEGIN,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675448',
       'type': LogEventType.REQUEST_ALIVE
     },
     {
       'params': {
         'load_flags': 68223104,
         'method': 'GET',
         'priority': 4,
         'url': 'http://www.doesnotexistdomain.com/'
       },
       'phase': LogEventPhase.PHASE_BEGIN,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675455',
       'type': LogEventType.URL_REQUEST_START_JOB
     },
     {
       'phase': LogEventPhase.PHASE_END,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675460',
       'type': LogEventType.URL_REQUEST_START_JOB
     },
     {
       'params': {
         'load_flags': 68223104,
         'method': 'GET',
         'priority': 4,
         'url': 'http://www.doesnotexistdomain.com/'
       },
       'phase': LogEventPhase.PHASE_BEGIN,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675460',
       'type': LogEventType.URL_REQUEST_START_JOB
     },
     {
       'phase': LogEventPhase.PHASE_BEGIN,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675469',
       'type': LogEventType.HTTP_CACHE_GET_BACKEND
     },
     {
       'phase': LogEventPhase.PHASE_END,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675469',
       'type': LogEventType.HTTP_CACHE_GET_BACKEND
     },
     {
       'phase': LogEventPhase.PHASE_BEGIN,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675469',
       'type': LogEventType.HTTP_CACHE_OPEN_ENTRY
     },
     {
       'params': {
         'net_error': -2
       },
       'phase': LogEventPhase.PHASE_END,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675470',
       'type': LogEventType.HTTP_CACHE_OPEN_ENTRY
     },
     {
       'phase': LogEventPhase.PHASE_BEGIN,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675471',
       'type': LogEventType.HTTP_CACHE_CREATE_ENTRY
     },
     {
       'phase': LogEventPhase.PHASE_END,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675473',
       'type': LogEventType.HTTP_CACHE_CREATE_ENTRY
     },
     {
       'phase': LogEventPhase.PHASE_BEGIN,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675473',
       'type': LogEventType.HTTP_CACHE_ADD_TO_ENTRY
     },
     {
       'phase': LogEventPhase.PHASE_END,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675474',
       'type': LogEventType.HTTP_CACHE_ADD_TO_ENTRY
     },
     {
       'phase': LogEventPhase.PHASE_BEGIN,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675474',
       'type': LogEventType.HTTP_STREAM_REQUEST
     },
     {
       'phase': LogEventPhase.PHASE_END,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675699',
       'type': LogEventType.HTTP_STREAM_REQUEST
     },
     {
       'params': {
         'net_error': -105
       },
       'phase': LogEventPhase.PHASE_END,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675705',
       'type': LogEventType.URL_REQUEST_START_JOB
     },
     {
       'params': {
         'net_error': -105
       },
       'phase': LogEventPhase.PHASE_END,
       'source': {
         'id': 318,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '953675923',
       'type': LogEventType.REQUEST_ALIVE
     }
  ];

  testCase.expectedText =
't=1338864773894 [st=  0] +REQUEST_ALIVE  [dt=475]\n' +
't=1338864773901 [st=  7]    URL_REQUEST_START_JOB  [dt=5]\n' +
'                            --> load_flags = 68223104 (ENABLE_LOAD_TIMING | ' +
    'MAIN_FRAME | MAYBE_USER_GESTURE | VERIFY_EV_CERT)\n' +
'                            --> method = "GET"\n' +
'                            --> priority = 4\n' +
'                            --> url = "http://www.doesnotexistdomain.com/"\n' +
't=1338864773906 [st= 12]   +URL_REQUEST_START_JOB  [dt=245]\n' +
'                            --> load_flags = 68223104 (ENABLE_LOAD_TIMING | ' +
    'MAIN_FRAME | MAYBE_USER_GESTURE | VERIFY_EV_CERT)\n' +
'                            --> method = "GET"\n' +
'                            --> priority = 4\n' +
'                            --> url = "http://www.doesnotexistdomain.com/"\n' +
't=1338864773915 [st= 21]      HTTP_CACHE_GET_BACKEND  [dt=0]\n' +
't=1338864773915 [st= 21]      HTTP_CACHE_OPEN_ENTRY  [dt=1]\n' +
'                              --> net_error = -2 (ERR_FAILED)\n' +
't=1338864773917 [st= 23]      HTTP_CACHE_CREATE_ENTRY  [dt=2]\n' +
't=1338864773919 [st= 25]      HTTP_CACHE_ADD_TO_ENTRY  [dt=1]\n' +
't=1338864773920 [st= 26]      HTTP_STREAM_REQUEST  [dt=225]\n' +
't=1338864774151 [st=257]   -URL_REQUEST_START_JOB\n' +
'                            --> net_error = -105 (ERR_NAME_NOT_RESOLVED)\n' +
't=1338864774369 [st=475] -REQUEST_ALIVE\n' +
'                          --> net_error = -105 (ERR_NAME_NOT_RESOLVED)';

  return testCase;
}

/**
 * Tests the formatting of bytes sent/received as hex + ASCII. Note that the
 * test data was truncated which is why the byte_count doesn't quite match the
 * hex_encoded_bytes.
 */
function painterTestHexEncodedBytes() {
  var testCase = {};
  testCase.tickOffset = '1337911098473';

  testCase.logEntries = [
     {
       'params': {
         'source_dependency': {
           'id': 634,
           'type': 4
         }
       },
       'phase': LogEventPhase.PHASE_BEGIN,
       'source': {
         'id': 637,
         'type': LogSourceType.SOCKET
       },
       'time': '953918459',
       'type': LogEventType.SOCKET_ALIVE
     },
     {
       'params': {
         'address_list': [
           '184.30.253.15:80'
         ]
       },
       'phase': LogEventPhase.PHASE_BEGIN,
       'source': {
         'id': 637,
         'type': LogSourceType.SOCKET
       },
       'time': '953918460',
       'type': LogEventType.TCP_CONNECT
     },
     {
       'params': {
         'address': '184.30.253.15:80'
       },
       'phase': LogEventPhase.PHASE_BEGIN,
       'source': {
         'id': 637,
         'type': LogSourceType.SOCKET
       },
       'time': '953918461',
       'type': LogEventType.TCP_CONNECT_ATTEMPT
     },
     {
       'phase': LogEventPhase.PHASE_END,
       'source': {
         'id': 637,
         'type': LogSourceType.SOCKET
       },
       'time': '953918464',
       'type': LogEventType.TCP_CONNECT_ATTEMPT
     },
     {
       'params': {
         'source_address': '127.0.0.1:54041'
       },
       'phase': LogEventPhase.PHASE_END,
       'source': {
         'id': 637,
         'type': LogSourceType.SOCKET
       },
       'time': '953918465',
       'type': LogEventType.TCP_CONNECT
     },
     {
       'params': {
         'source_dependency': {
           'id': 628,
           'type': 11
         }
       },
       'phase': LogEventPhase.PHASE_BEGIN,
       'source': {
         'id': 637,
         'type': LogSourceType.SOCKET
       },
       'time': '953918472',
       'type': LogEventType.SOCKET_IN_USE
     },
     {
       'params': {
         'byte_count': 780,
         'hex_encoded_bytes': '474554202F66617669636F6E2E69636F20485454502' +
                              'F312E310D0A486F73743A207777772E6170706C652E' +
                              '636F6D0D0A436F6E6E656374696F6E3A20'
       },
       'phase': LogEventPhase.PHASE_NONE,
       'source': {
         'id': 637,
         'type': LogSourceType.SOCKET
       },
       'time': '953918484',
       'type': LogEventType.SOCKET_BYTES_SENT
     },
     {
       'params': {
         'byte_count': 1024,
         'hex_encoded_bytes': '485454502F312E3120323030204F4B0D0A4C6173742' +
                              'D4D6F6469666965643A204D6F6E2C20313920446563' +
                              '20323031312032323A34363A353920474D'
       },
       'phase': LogEventPhase.PHASE_NONE,
       'source': {
         'id': 637,
         'type': LogSourceType.SOCKET
       },
       'time': '953918596',
       'type': LogEventType.SOCKET_BYTES_RECEIVED
     }
  ];

  testCase.expectedText =
't=1338865016932 [st=  0] +SOCKET_ALIVE  [dt=?]\n' +
'                          --> source_dependency = 634 (CONNECT_JOB)\n' +
't=1338865016933 [st=  1]   +TCP_CONNECT  [dt=5]\n' +
'                            --> address_list = ["184.30.253.15:80"]\n' +
't=1338865016934 [st=  2]      TCP_CONNECT_ATTEMPT  [dt=3]\n' +
'                              --> address = "184.30.253.15:80"\n' +
't=1338865016938 [st=  6]   -TCP_CONNECT\n' +
'                            --> source_address = "127.0.0.1:54041"\n' +
't=1338865016945 [st= 13]   +SOCKET_IN_USE  [dt=?]\n' +
'                            --> source_dependency = 628 (HTTP_STREAM_JOB)\n' +
't=1338865016957 [st= 25]      SOCKET_BYTES_SENT\n' +
'                              --> byte_count = 780\n' +
'                              --> hex_encoded_bytes =\n' +
'                                47 45 54 20 2F 66 61 76 69 63 6F 6E 2E 69 ' +
    '63 6F 20 48 54 54   GET /favicon.ico HTT\n' +
'                                50 2F 31 2E 31 0D 0A 48 6F 73 74 3A 20 77 ' +
    '77 77 2E 61 70 70   P/1.1..Host: www.app\n' +
'                                6C 65 2E 63 6F 6D 0D 0A 43 6F 6E 6E 65 63 ' +
    '74 69 6F 6E 3A 20   le.com..Connection: \n' +
't=1338865017069 [st=137]      SOCKET_BYTES_RECEIVED\n' +
'                              --> byte_count = 1024\n' +
'                              --> hex_encoded_bytes =\n' +
'                                48 54 54 50 2F 31 2E 31 20 32 30 30 20 4F ' +
    '4B 0D 0A 4C 61 73   HTTP/1.1 200 OK..Las\n' +
'                                74 2D 4D 6F 64 69 66 69 65 64 3A 20 4D 6F ' +
    '6E 2C 20 31 39 20   t-Modified: Mon, 19 \n' +
'                                44 65 63 20 32 30 31 31 20 32 32 3A 34 36 ' +
    '3A 35 39 20 47 4D   Dec 2011 22:46:59 GM';

  return testCase;
}

/**
 * Tests the formatting of certificates.
 */
function painterTestCertVerifierJob() {
  var testCase = {};
  testCase.tickOffset = '1337911098481';

  testCase.logEntries = [
     {
       'params': {
         'certificates': [
           '-----BEGIN CERTIFICATE-----\n1\n-----END CERTIFICATE-----\n',
           '-----BEGIN CERTIFICATE-----\n2\n-----END CERTIFICATE-----\n',
         ]
       },
       'phase': LogEventPhase.PHASE_BEGIN,
       'source': {
         'id': 752,
         'type': LogSourceType.CERT_VERIFIER_JOB
       },
       'time': '954124663',
       'type': LogEventType.CERT_VERIFIER_JOB
     },
     {
       'phase': LogEventPhase.PHASE_END,
       'source': {
         'id': 752,
         'type': LogSourceType.CERT_VERIFIER_JOB
       },
       'time': '954124697',
       'type': LogEventType.CERT_VERIFIER_JOB
     }
  ];

  testCase.expectedText =
    't=1338865223144 [st=0]  CERT_VERIFIER_JOB  [dt=34]\n' +
    '                        --> certificates =\n' +
    '                               -----BEGIN CERTIFICATE-----\n' +
    '                               1\n' +
    '                               -----END CERTIFICATE-----\n' +
    '                               \n' +
    '                               -----BEGIN CERTIFICATE-----\n' +
    '                               2\n' +
    '                               -----END CERTIFICATE-----';

  return testCase;
}

/**
 * Tests the formatting of proxy configurations.
 */
function painterTestProxyConfig() {
  var testCase = {};
  testCase.tickOffset = '1337911098481';

  testCase.logEntries = [
     {
       'params': {
         'new_config': {
           'auto_detect': true,
           'bypass_list': [
             '*.local',
             'foo',
             '<local>'
           ],
           'pac_url': 'https://config/wpad.dat',
           'single_proxy': 'cache-proxy:3128'
         },
         'old_config': {
           'auto_detect': true
         }
       },
       'phase': LogEventPhase.PHASE_NONE,
       'source': {
         'id': 814,
         'type': LogSourceType.NONE
       },
       'time': '954443578',
       'type': LogEventType.PROXY_CONFIG_CHANGED
     }
  ];

  testCase.expectedText =
    't=1338865542059 [st=0]  PROXY_CONFIG_CHANGED\n' +
    '                        --> old_config =\n' +
    '                               Auto-detect\n' +
    '                        --> new_config =\n' +
    '                               (1) Auto-detect\n' +
    '                               (2) PAC script: https://config/wpad.dat\n' +
    '                               (3) Proxy server: cache-proxy:3128\n' +
    '                                   Bypass list: \n' +
    '                                     *.local\n' +
    '                                     foo\n' +
    '                                     <local>';

  return testCase;
}

/**
 * Tests that cookies are NOT stripped from URLRequests when stripping is
 * disabled.
 */
function painterTestDontStripCookiesURLRequest() {
  var testCase = {};
  testCase.tickOffset = '1337911098139';

  testCase.logEntries = [
     {
       'params': {
         'headers': [
           'HTTP/1.1 301 Moved Permanently',
           'Cache-Control: private',
           'Content-Length: 23',
           'Content-Type: text/html',
           'Location: http://msdn.microsoft.com',
           'Server: Microsoft-IIS/7.5',
           'Set-Cookie: MyMagicPony',
           'P3P: CP=\"ALL\"',
           'X-Powered-By: ASP.NET',
           'X-UA-Compatible: IE=EmulateIE7',
           'Date: Tue, 05 Jun 2012 21:06:45 GMT',
           'Connection: close'
         ]
       },
       'phase': LogEventPhase.PHASE_NONE,
       'source': {
         'id': 829,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '1019307339',
       'type': LogEventType.HTTP_TRANSACTION_READ_RESPONSE_HEADERS
     },
     {
       'params': {
         'headers': [
           'Host: msdn.microsoft.com',
           'Connection: keep-alive',
           'User-Agent: Mozilla/5.0',
           'Accept: text/html',
           'Accept-Encoding: gzip,deflate,sdch',
           'Accept-Language: en-US,en;q=0.8',
           'Accept-Charset: ISO-8859-1',
           'Cookie: MyMagicPony'
         ],
         'line': 'GET / HTTP/1.1\r\n'
       },
       'phase': LogEventPhase.PHASE_NONE,
       'source': {
         'id': 829,
         'type': LogSourceType.URL_REQUEST
       },
       'time': '1019307458',
       'type': LogEventType.HTTP_TRANSACTION_SEND_REQUEST_HEADERS
     }
  ];

  testCase.expectedText =
't=1338930405478 [st=  0]  HTTP_TRANSACTION_READ_RESPONSE_HEADERS\n' +
'                          --> HTTP/1.1 301 Moved Permanently\n' +
'                              Cache-Control: private\n' +
'                              Content-Length: 23\n' +
'                              Content-Type: text/html\n' +
'                              Location: http://msdn.microsoft.com\n' +
'                              Server: Microsoft-IIS/7.5\n' +
'                              Set-Cookie: MyMagicPony\n' +
'                              P3P: CP="ALL"\n' +
'                              X-Powered-By: ASP.NET\n' +
'                              X-UA-Compatible: IE=EmulateIE7\n' +
'                              Date: Tue, 05 Jun 2012 21:06:45 GMT\n' +
'                              Connection: close\n' +
't=1338930405597 [st=119]  HTTP_TRANSACTION_SEND_REQUEST_HEADERS\n' +
'                          --> GET / HTTP/1.1\n' +
'                              Host: msdn.microsoft.com\n' +
'                              Connection: keep-alive\n' +
'                              User-Agent: Mozilla/5.0\n' +
'                              Accept: text/html\n' +
'                              Accept-Encoding: gzip,deflate,sdch\n' +
'                              Accept-Language: en-US,en;q=0.8\n' +
'                              Accept-Charset: ISO-8859-1\n' +
'                              Cookie: MyMagicPony';

  return testCase;
}

/**
 * Tests that cookies are stripped from URLRequest when stripping is enabled.
 */
function painterTestStripCookiesURLRequest() {
  var testCase = painterTestDontStripCookiesURLRequest();
  testCase.enableSecurityStripping = true;
  testCase.expectedText =
    testCase.expectedText.replace(/MyMagicPony/g, '[value was stripped]');
  return testCase;
}

/**
 * Tests that cookies are NOT stripped from SPDY sessions when stripping is not
 * enabled. This test data was pieced together in order to get a "cookie" and
 * "set-cookie" header.
 */
function painterTestDontStripCookiesSPDYSession() {
  var testCase = {};
  testCase.tickOffset = '1337911097783';

  testCase.logEntries = [
    {
      'params': {
        'flags': 1,
        'headers': [
          ':host: mail.google.com',
          ':method: GET',
          ':path: /a/google.com',
          ':scheme: https',
          ':version: HTTP/1.1',
          'accept: text/html',
          'accept-charset: ISO-8859-1',
          'accept-encoding: gzip,deflate,sdch',
          'accept-language: en-US,en;q=0.8',
          'cookie: MyLittlePony',
          'user-agent: Mozilla/5.0'
        ],
        'stream_id': 1
      },
      'phase': LogEventPhase.PHASE_NONE,
      'source': {
        'id': 153,
        'type': LogSourceType.SPDY_SESSION
      },
      'time': '1012984638',
      'type': LogEventType.SPDY_SESSION_SYN_STREAM
    },
    {
      'params': {
        'flags': 0,
        'headers': [
          ':status: 204 No Content',
          ':version: HTTP/1.1',
          'date: Tue, 05 Jun 2012 19:21:30 GMT',
          'server: GSE',
          'set-cookie: MyLittlePony',
          'x-random-header: sup'
        ],
        'stream_id': 5
      },
      'phase': LogEventPhase.PHASE_NONE,
      'source': {
        'id': 153,
        'type': LogSourceType.SPDY_SESSION
      },
      'time': '1012992266',
      'type': LogEventType.SPDY_SESSION_SYN_REPLY
    }
  ];

  testCase.expectedText =
    't=1338924082421 [st=   0]  SPDY_SESSION_SYN_STREAM\n' +
    '                           --> flags = 1\n' +
    '                           --> :host: mail.google.com\n' +
    '                               :method: GET\n' +
    '                               :path: /a/google.com\n' +
    '                               :scheme: https\n' +
    '                               :version: HTTP/1.1\n' +
    '                               accept: text/html\n' +
    '                               accept-charset: ISO-8859-1\n' +
    '                               accept-encoding: gzip,deflate,sdch\n' +
    '                               accept-language: en-US,en;q=0.8\n' +
    '                               cookie: MyLittlePony\n' +
    '                               user-agent: Mozilla/5.0\n' +
    '                           --> stream_id = 1\n' +
    't=1338924090049 [st=7628]  SPDY_SESSION_SYN_REPLY\n' +
    '                           --> flags = 0\n' +
    '                           --> :status: 204 No Content\n' +
    '                               :version: HTTP/1.1\n' +
    '                               date: Tue, 05 Jun 2012 19:21:30 GMT\n' +
    '                               server: GSE\n' +
    '                               set-cookie: MyLittlePony\n' +
    '                               x-random-header: sup\n' +
    '                           --> stream_id = 5';

  return testCase;
}

/**
 * Tests that cookies are stripped from SPDY sessions when stripping is enabled.
 */
function painterTestStripCookiesSPDYSession() {
  var testCase = painterTestDontStripCookiesSPDYSession();
  testCase.enableSecurityStripping = true;
  testCase.expectedText =
      testCase.expectedText.replace(/MyLittlePony/g, '[value was stripped]');
  return testCase;
}

/**
 * Tests that when there are more custom parameters than we expect for an
 * event type, they are printed out in addition to the custom formatting.
 */
function painterTestExtraCustomParameter() {
  var testCase = {};
  testCase.tickOffset = '1337911098446';

  testCase.logEntries = [
    {
      'params': {
        'headers': [
          'Host: www.google.com',
          'Connection: keep-alive'
        ],
        'line': 'GET / HTTP/1.1\r\n',
        // This is unexpected!
        'hello': 'yo dawg, i heard you like strings'
      },
      'phase': LogEventPhase.PHASE_NONE,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534910',
      'type': LogEventType.HTTP_TRANSACTION_SEND_REQUEST_HEADERS
    },
  ];

  testCase.expectedText =
    't=1338864633356 [st=0]  HTTP_TRANSACTION_SEND_REQUEST_HEADERS\n' +
    '                        --> GET / HTTP/1.1\n' +
    '                            Host: www.google.com\n' +
    '                            Connection: keep-alive\n' +
    '                        --> hello = "yo dawg, i heard you like strings"';

  return testCase;
}

/**
 * Tests that when the custom parameters for an event type don't match
 * what we expect, we fall back to default formatting.
 */
function painterTestMissingCustomParameter() {
  var testCase = {};
  testCase.tickOffset = '1337911098446';

  testCase.logEntries = [
    {
      'params': {
        // The expectation is for this to be called "headers".
        'headersWRONG': [
          'Host: www.google.com',
          'Connection: keep-alive'
        ],
        'line': 'GET / HTTP/1.1\r\n'
      },
      'phase': LogEventPhase.PHASE_NONE,
      'source': {
        'id': 146,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '953534910',
      'type': LogEventType.HTTP_TRANSACTION_SEND_REQUEST_HEADERS
    },
  ];

  testCase.expectedText =
't=1338864633356 [st=0]  HTTP_TRANSACTION_SEND_REQUEST_HEADERS\n' +
'                        --> headersWRONG = ["Host: www.google.com",' +
    '"Connection: keep-alive"]\n' +
'                        --> line = "GET / HTTP/1.1\\r\\n"';

  return testCase;
}

/**
 * Tests the formatting for an SSL version fallback event.
 */
function painterTestSSLVersionFallback() {
  var testCase = {};
  testCase.tickOffset = '1337911098400';

  testCase.logEntries = [
    {
      'params': {
        'host_and_port': 'www-927.ibm.com:443',
        'net_error': -107,
        'version_after': 0x301,
        'version_before': 0x302
      },
        'phase': LogEventPhase.PHASE_NONE,
        'source': {
          'id': 124,
          'type': LogSourceType.URL_REQUEST
        },
        'time': '1119062679',
        'type': LogEventType.SSL_VERSION_FALLBACK
    },
    {
      'params': {
        'host_and_port': 'www-927.ibm.com:443',
        'net_error': -107,
        'version_after': 0x300,
        'version_before': 0x301
      },
      'phase': LogEventPhase.PHASE_NONE,
      'source': {
        'id': 124,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '1119062850',
      'type': LogEventType.SSL_VERSION_FALLBACK
    },
    {
      'params': {
        'version_after': 0x123456,
        'version_before': 0x300
      },
      'phase': LogEventPhase.PHASE_NONE,
      'source': {
        'id': 124,
        'type': LogSourceType.URL_REQUEST
      },
      'time': '1119062850',
      'type': LogEventType.SSL_VERSION_FALLBACK
    },
  ];

  testCase.expectedText =
't=1339030161079 [st=  0]  SSL_VERSION_FALLBACK\n' +
'                          --> TLS 1.1 ==> TLS 1.0\n' +
'                          --> host_and_port = "www-927.ibm.com:443"\n' +
'                          --> net_error = -107 (ERR_SSL_PROTOCOL_ERROR)\n' +
't=1339030161250 [st=171]  SSL_VERSION_FALLBACK\n' +
'                          --> TLS 1.0 ==> SSL 3.0\n' +
'                          --> host_and_port = "www-927.ibm.com:443"\n' +
'                          --> net_error = -107 (ERR_SSL_PROTOCOL_ERROR)\n' +
't=1339030161250 [st=171]  SSL_VERSION_FALLBACK\n' +
'                          --> SSL 3.0 ==> SSL 0x123456';

  return testCase;
}

})();  // Anonymous namespace
