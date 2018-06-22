// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const htmlWithScript =
     `<!DOCTYPE html>
      <link rel="import" href="/import.html">
      <script src="/script.js"></script>
      <script type="module" src="/module.js"></script>`;

const htmlImport =
    `<script>console.log("imported html");</script>`

const script =
    `console.log("ran script");`

const module =
    `console.log("ran module");`

const server = new Map([
  ['http://test.com/index.html', { body: htmlWithScript }],
  ['http://test.com/import.html', { body: htmlImport }],
  ['http://test.com/script.js', { body: script }],
  ['http://test.com/module.js',
    { body: module,
      headers: ['HTTP/1.1 200 OK', 'Content-Type: application/javascript'] }
  ]]);

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that an html import followed by a pending script does not break ` +
      `virtual time.`);
  await dp.Runtime.enable();
  await dp.Page.enable();
  await dp.Network.enable();
  await dp.Network.setRequestInterception({ patterns: [{ urlPattern: '*' }] });
  dp.Network.onRequestIntercepted(event => {
    let body = server.get(event.params.request.url).body || '';
    let headers = server.get(event.params.request.url).headers || [];
    dp.Network.continueInterceptedRequest({
      interceptionId: event.params.interceptionId,
      rawResponse: btoa(headers.join('\r\n') + '\r\n\r\n' + body)
    });
  });

  dp.Runtime.onConsoleAPICalled(data => {
    const text = data.params.args[0].value;
    testRunner.log(text);
  });

  dp.Emulation.onVirtualTimeBudgetExpired(async data => {
    testRunner.completeTest();
  });

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending', budget: 5000,
      waitForNavigation: true});
  dp.Page.navigate({url: 'http://test.com/index.html'});
})
