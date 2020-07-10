// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: in cross origin object.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(
      `http://foo.com/`,
      `<html>
        <body>
          <iframe id='myframe' src='http://bar.com/'></iframe>
          <script>
            window.onload = function() {
              try {
                var a = 0 in document.getElementById('myframe').contentWindow;
              } catch (e) {
                console.log(e.message);
              }
            };
          </script>
          <p>Pass</p>
        </body>
      </html>`);

  httpInterceptor.addResponse(
      `http://bar.com/`,
      `<html></html>`);

  await virtualTimeController.grantInitialTime(500, 1000,
    null,
    async () => {
      testRunner.log(await session.evaluate('document.body.innerText'));
      testRunner.completeTest();
    }
  );

  await frameNavigationHelper.navigate('http://foo.com/');
})
