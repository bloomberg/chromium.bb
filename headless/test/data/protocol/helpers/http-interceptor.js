// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A helper class to allow renderer tests to conveniently intercept network
 * requests and provide HTTP headers and body.
 */
(class HttpInterceptor {
  /**
   * @param {!TestRunner} testRunner Host TestRunner instance.
   * @param {!Proxy} dp DevTools session protocol instance.
   * @param {!Page} page TestRunner.Page instance.
   */
  constructor(testRunner, dp) {
    this.testRunner_ = testRunner;
    this.dp_ = dp;
    this.responses_ = new Map();
  }

  /**
   * Initializes the helper returning reference to itself to allow assignment.
   *
   * @return {!object} HttpInterceptor reference.
   */
  async init() {
    await this.dp_.Network.enable();
    await this.dp_.Network.setRequestInterception(
        { patterns: [{ urlPattern: '*' }] });

    this.dp_.Network.onRequestIntercepted(event => {
      const url = event.params.request.url;
      if (this.responses_.has(url)) {
        this.testRunner_.log(`requested url: ${url}`);
      } else {
        this.testRunner_.log(`requested url: ${url} is not known`);
        this.logResponses();
      }
      const body = this.responses_.get(url).body || '';
      const headers = this.responses_.get(url).headers || [];
      const response = headers.join('\r\n') + '\r\n\r\n' + body;
      this.dp_.Network.continueInterceptedRequest({
        interceptionId: event.params.interceptionId,
        rawResponse: btoa(response)
      });
    });

    return this;
  }

  addResponse(url, body, headers) {
    this.responses_.set(url, {body, headers});
  }

  logResponses() {
    for (const [url, value] of this.responses_.entries()) {
      this.testRunner_.log(
          `url=${url}\nbody=${value.body}\nheaders=${value.headers}`);
    }
  }

});
