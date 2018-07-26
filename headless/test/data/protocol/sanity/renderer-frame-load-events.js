// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank(
      'Tests renderer: content security policy.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(`http://example.com/`, null,
      ['HTTP/1.1 302 Found', 'Location: http://example.com/1']);

  httpInterceptor.addResponse(`http://example.com/1`,
      `<html><frameset>
        <frame src="http://example.com/frameA/" id="frameA">
        <frame src="http://example.com/frameB/" id="frameB">
      </frameset></html>`);

  httpInterceptor.addResponse(`http://example.com/frameA/`,
      `<html><head><script>
        document.location="http://example.com/frameA/1"
      </script></head></html>`);

  httpInterceptor.addResponse(`http://example.com/frameB/`,
      `<html><head><script>
        document.location="http://example.com/frameB/1"
      </script></head></html>`);

  httpInterceptor.addResponse(`http://example.com/frameA/1`,
      `<html><body>FRAME A 1</body></html>`);

  httpInterceptor.addResponse(`http://example.com/frameB/1`,
      `<html><body>FRAME B 1
        <iframe src="http://example.com/frameB/1/iframe/" id="iframe"></iframe>
      </body></html>`);

  httpInterceptor.addResponse(`http://example.com/frameB/1/iframe/`,
      `<html><head><script>
        document.location="http://example.com/frameB/1/iframe/1"
      </script></head></html>`);

  httpInterceptor.addResponse(`http://example.com/frameB/1/iframe/1`,
      `<html><body>IFRAME 1</body><html>`);

  await virtualTimeController.grantInitialTime(500, 1000,
    null,
    async () => {
      testRunner.log(await session.evaluate('document.body.innerHTML'));
      frameNavigationHelper.logFrames();
      frameNavigationHelper.logScheduledNavigations();
      testRunner.completeTest();
    }
  );

  await frameNavigationHelper.navigate('http://example.com/');
})
