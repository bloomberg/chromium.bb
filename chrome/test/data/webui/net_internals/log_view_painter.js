// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tests the behavior of stripCookiesAndLoginInfo.
 */
netInternalsTest.test('netInternalsLogViewPainterStripInfo', function() {
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
                  'User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64)'],
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
