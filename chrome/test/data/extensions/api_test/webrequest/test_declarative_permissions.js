// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See permissionless/rules.js for the rules that this test uses.

runTests([
  // Test that it's possible to redirect within the same domain and port
  // (ignoring the scheme) without host permissions.
  function testRedirectSameDomain() {
    ignoreUnexpected = true;
    var testURL = getServerURL(
        'files/extensions/api_test/webrequest/simpleLoad/a.html',
        'www.a.com', 'https');
    expect(
      [
        { label: 'onBeforeRedirect',
          event: 'onBeforeRedirect',
          details: {
            url: testURL,
            redirectUrl: getServerURL('files/nonexistent/redirected'),
            statusCode: -1,
            statusLine: '',
            fromCache: false,
          }
        },
      ],
      [ ['onBeforeRedirect'] ]
    );
    navigateAndWait(testURL);
  },

  // Test that it's not possible to redirect to a different domain
  // without host permissions on the original domain. We should still
  // load the original URL.
  function testCannotRedirectDifferentDomains() {
    ignoreUnexpected = true;
    var testURL = getServerURL(
        'files/extensions/api_test/webrequest/simpleLoad/b.html');
    expect(
      [
        { label: 'onCompleted',
          event: 'onCompleted',
          details: {
            url: testURL,
            fromCache: false,
            ip: '127.0.0.1',
            statusCode: 200,
            statusLine: 'HTTP/1.0 200 OK',
          }
        },
      ],
      [ ['onCompleted'] ]
    );
    navigateAndWait(testURL);
  },

  // Test that it's possible to redirect by regex within the same
  // domain and port (ignoring the scheme) without host permissions.
  function testRedirectByRegexSameDomain() {
    ignoreUnexpected = true;
    var testURL = getServerURL(
        'files/extensions/api_test/webrequest/simpleLoad/fake.html');
    expect(
      [
        { label: 'onBeforeRedirect',
          event: 'onBeforeRedirect',
          details: {
            url: testURL,
            redirectUrl: getServerURL(
                'files/extensions/api_test/webrequest/simpleLoad/b.html'),
            statusCode: -1,
            statusLine: '',
            fromCache: false,
          }
        },
      ],
      [ ['onBeforeRedirect'] ]
    );
    navigateAndWait(testURL);
  },

  // Test that it's possible to redirect to a blank page/image
  // without host permissions.
  function testRedirectToEmpty() {
    var testURL = getServerURL('files/nonexistent/blank.html');
    ignoreUnexpected = true;
    expect(
      [
        { label: 'onBeforeRedirect',
          event: 'onBeforeRedirect',
          details: {
            url: testURL,
            redirectUrl: 'data:text/html,',
            statusCode: -1,
            statusLine: '',
            fromCache: false,
          }
        },
      ],
      [ ['onBeforeRedirect'] ]
    );
    navigateAndWait(testURL);
  },

  // Test that it's possible to cancel a request without host permissions.
  function testCancelRequest() {
    ignoreUnexpected = true;
    var testURL = getServerURL('files/nonexistent/cancel.html');
    expect(
      [
        { label: 'onErrorOccurred',
          event: 'onErrorOccurred',
          details: {
            url: testURL,
            fromCache: false,
            error: 'net::ERR_BLOCKED_BY_CLIENT'
          }
        },
      ],
      [ ['onErrorOccurred'] ]
    );
    navigateAndWait(testURL);
  },

]);
