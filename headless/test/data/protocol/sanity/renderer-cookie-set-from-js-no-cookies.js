// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: cookie set from js with cookies disabled.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(
      `http://www.example.com/`,
      `<html>
        <head>
          <script>
            document.cookie = 'SessionID=123';
          </script>
        </head>
        <body>Hello, World!</body>
      </html>`);

  await dp.Emulation.setDocumentCookieDisabled({disabled: true});

  await virtualTimeController.grantInitialTime(5000, 1000,
    null,
    async () => {
      const cookieIndex =
          await session.evaluate(`document.cookie.indexOf('SessionID')`);
      testRunner.log(cookieIndex < 0 ? 'pass' : 'FAIL');
      testRunner.completeTest();
    }
  );

  await frameNavigationHelper.navigate('http://www.example.com/');
})
