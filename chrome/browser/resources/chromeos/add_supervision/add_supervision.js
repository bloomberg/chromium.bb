// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let server = null;
const proxy = addSupervision.mojom.AddSupervisionHandler.getProxy();

document.addEventListener('DOMContentLoaded', () => {
  proxy.getOAuthToken().then((result) => {
    const url = loadTimeData.getString('webviewUrl');
    const eventOriginFilter = loadTimeData.getString('eventOriginFilter');
    const webview =
        /** @type {!WebView} */ (document.querySelector('#webview'));
    const oauthToken = result.oauthToken;

    // Block any requests to URLs other than one specified
    // by eventOriginFilter.
    webview.request.onBeforeRequest.addListener(function(details) {
      return {cancel: !details.url.startsWith(eventOriginFilter)};
    }, {urls: ['<all_urls>']}, ['blocking']);

    // Add the Authorizaton header, but only for URLs that prefix match the
    // eventOrigin filter.
    webview.request.onBeforeSendHeaders.addListener(function(details) {
      details.requestHeaders.push(
          {name: 'Authorization', value: 'Bearer ' + oauthToken});
      return {requestHeaders: details.requestHeaders};
    }, {urls: [eventOriginFilter + '/*']}, ['blocking', 'requestHeaders']);

    webview.src = url;

    // Set up the server.
    server = new AddSupervisionAPIServer(webview, url, eventOriginFilter);
  });
});
