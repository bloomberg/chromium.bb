// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  let {page, session, dp} = await testRunner.startWithFrameControl(
      'Tests renderer: hello world.');

  let RendererTestHelper =
      await testRunner.loadScript('../helpers/renderer-test-helper.js');
  let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
      await (new RendererTestHelper(testRunner, dp, page)).init();

  httpInterceptor.addResponse(
      `http://www.example.com/`,
      `<!doctype html><h1>Hello headless world!</h1>`);

  await virtualTimeController.grantInitialTime(500, 1000,
    null,
    async () => {
      testRunner.log(await session.evaluate('document.body.innerHTML'));
      frameNavigationHelper.logFrames();
      frameNavigationHelper.logScheduledNavigations();
      testRunner.completeTest();
    }
  );

  await frameNavigationHelper.navigate('http://www.example.com/');
})
