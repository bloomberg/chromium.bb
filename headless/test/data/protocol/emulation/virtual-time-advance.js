// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests that virtual time advances.`);
  await dp.Page.enable();
  await dp.Runtime.enable();

  dp.Emulation.onVirtualTimePaused(data =>
      testRunner.log(`Paused @ ${data.params.virtualTimeElapsed}ms`));
  dp.Emulation.onVirtualTimeAdvanced(data =>
      testRunner.log(`Advanced to ${data.params.virtualTimeElapsed}ms`));

  dp.Runtime.onConsoleAPICalled(data => {
    const text = data.params.args[0].value;
    testRunner.log(text);
    if (text === 'pass')
      testRunner.completeTest();
  });

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  dp.Page.navigate({url: testRunner.url('resources/virtual-time-advance.html')});
  await dp.Emulation.setVirtualTimePolicy({
      policy: 'pauseIfNetworkFetchesPending', budget: 5000, waitForNavigation: true});
})

