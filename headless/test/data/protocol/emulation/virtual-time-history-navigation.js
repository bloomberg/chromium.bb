// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests virtual time with history navigation.`);

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  helper.onceRequest('http://foo.com/').fulfill(
      FetchHelper.makeContentResponse(`
          <script> console.log(document.location.href); </script>
          <iframe src='/a/'></iframe>`)
  );

  helper.onceRequest('http://foo.com/a/').fulfill(
      FetchHelper.makeContentResponse(`
          <script> console.log(document.location.href); </script>`)
  );

  helper.onceRequest('http://bar.com/').fulfill(
      FetchHelper.makeContentResponse(`
          <script> console.log(document.location.href); </script>
          <iframe src='/b/' id='frame_b'></iframe>
          <iframe src='/c/'></iframe>`)
  );

  helper.onceRequest('http://bar.com/b/').fulfill(
      FetchHelper.makeContentResponse(`
          <script> console.log(document.location.href); </script>
          <iframe src='/d/'></iframe>`)
  );

  helper.onceRequest('http://bar.com/c/').fulfill(
      FetchHelper.makeContentResponse(`
          <script> console.log(document.location.href); </script>`)
  );

  helper.onceRequest('http://bar.com/d/').fulfill(
      FetchHelper.makeContentResponse(`
          <script> console.log(document.location.href); </script>`)
  );

  helper.onceRequest('http://bar.com/e/').fulfill(
      FetchHelper.makeContentResponse(`
          <script> console.log(document.location.href); </script>
          <iframe src='/f/'></iframe>`)
  );

  helper.onceRequest('http://bar.com/f/').fulfill(
    FetchHelper.makeContentResponse(`
        <script> console.log(document.location.href); </script>`)
  );

  const testCommands = [
      `document.location.href = 'http://bar.com/'`,
      `document.getElementById('frame_b').src = '/e/'`,
      `history.back()`,
      `history.forward()`,
      `history.go(-1)`];

  dp.Emulation.onVirtualTimeBudgetExpired(async data => {
    if (!testCommands.length) {
      testRunner.completeTest();
      return;
    }
    const command = testCommands.shift();
    testRunner.log(command);
    await session.evaluate(command);
    await dp.Emulation.setVirtualTimePolicy({
        policy: 'pauseIfNetworkFetchesPending', budget: 5000});
  });

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending', budget: 5000,
      waitForNavigation: true});
  dp.Page.navigate({url: 'http://foo.com/'});
})
