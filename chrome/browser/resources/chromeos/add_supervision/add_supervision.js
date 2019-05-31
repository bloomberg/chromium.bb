// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let server = null;
const proxy = addSupervision.mojom.AddSupervisionHandler.getProxy();

document.addEventListener('DOMContentLoaded', () => {
  proxy.getOAuthToken().then((result) => {
    const webviewUrl = loadTimeData.getString('webviewUrl');
    if (!webviewUrl.startsWith('https://families.google.com')) {
      console.error('webviewUrl is not from https://families.google.com');
      return;
    }
    const eventOriginFilter = loadTimeData.getString('eventOriginFilter');
    const webview =
        /** @type {!WebView} */ (document.querySelector('#webview'));

    const accessToken = result.oauthToken;
    const flowType = loadTimeData.getString('flowType');
    const platformVersion = loadTimeData.getString('platformVersion');

    const url = new URL(webviewUrl);
    url.searchParams.set('flowType', flowType);
    url.searchParams.set('platformVersion', platformVersion);
    url.searchParams.set('accessToken', accessToken);

    // Block any requests to URLs other than one specified
    // by eventOriginFilter.
    webview.request.onBeforeRequest.addListener(function(details) {
      return {cancel: !details.url.startsWith(eventOriginFilter)};
    }, {urls: ['<all_urls>']}, ['blocking']);

    webview.src = url.toString();

    // Set up the server.
    server = new AddSupervisionAPIServer(webview, url, eventOriginFilter);
  });
});
