// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flushTasks, isVisible} from 'chrome://test/test_util.m.js';
import {assertStyle, createTestProxy} from './test_support.js';

class MockSpeechRecognition {
  constructor() {
    this.startCalled = false;
    this.abortCalled = false;
    mockSpeechRecognition = this;
  }

  start() {
    this.startCalled = true;
  }

  abort() {
    this.abortCalled = true;
  }
}

/** @type {!MockSpeechRecognition} */
let mockSpeechRecognition;

suite('NewTabPageVoiceSearchOverlayTest', () => {
  /** @type {!VoiceSearchOverlayElement} */
  let voiceSearchOverlay;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(async () => {
    PolymerTest.clearBody();

    window.webkitSpeechRecognition = MockSpeechRecognition;

    testProxy = createTestProxy();
    testProxy.setResultFor('setTimeout', 0);
    BrowserProxy.instance_ = testProxy;

    voiceSearchOverlay = document.createElement('ntp-voice-search-overlay');
    document.body.appendChild(voiceSearchOverlay);
    await flushTasks();
  });

  test('creating overlay opens native dialog', () => {
    // Assert.
    assertTrue(voiceSearchOverlay.$.dialog.open);
  });

  test('creating overlay starts speech recognition', () => {
    // Assert.
    assertTrue(mockSpeechRecognition.startCalled);
  });

  test('creating overlay shows waiting text', () => {
    // Assert.
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot.querySelector('#texts *[text=waiting]')));
    assertFalse(
        voiceSearchOverlay.$.micContainer.classList.contains('listening'));
    assertFalse(
        voiceSearchOverlay.$.micContainer.classList.contains('receiving'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0');
  });

  test('on audio received shows speak text', () => {
    // Act.
    mockSpeechRecognition.onaudiostart();

    // Assert.
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot.querySelector('#texts *[text=speak]')));
    assertTrue(
        voiceSearchOverlay.$.micContainer.classList.contains('listening'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0');
  });

  test('on speech received starts volume animation', () => {
    // Arrange.
    testProxy.setResultFor('random', 0.5);

    // Act.
    mockSpeechRecognition.onspeechstart();

    // Assert.
    assertTrue(
        voiceSearchOverlay.$.micContainer.classList.contains('receiving'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0.5');
  });

  test('on result received shows recognized text', () => {
    // Arrange.
    testProxy.setResultFor('random', 0.5);

    // Act.
    mockSpeechRecognition.onresult({
      results: [
        {
          isFinal: false,
          0: {
            transcript: 'hello',
            confidence: 1,
          },
        },
        {
          isFinal: false,
          0: {
            transcript: 'world',
            confidence: 0,
          },
        },
      ],
      resultIndex: 0,
    });

    // Assert.
    const [intermediateResult, finalResult] =
        voiceSearchOverlay.shadowRoot.querySelectorAll(
            '#texts *[text=result] span');
    assertTrue(isVisible(intermediateResult));
    assertTrue(isVisible(finalResult));
    assertEquals(intermediateResult.innerText, 'hello');
    assertEquals(finalResult.innerText, 'world');
    assertTrue(
        voiceSearchOverlay.$.micContainer.classList.contains('receiving'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0.5');
  });

  test('on final result received queries google', async () => {
    // Arrange.
    const googleBaseUrl = 'https://google.com';
    loadTimeData.overrideValues({googleBaseUrl: googleBaseUrl});
    testProxy.setResultFor('random', 0);

    // Act.
    mockSpeechRecognition.onresult({
      results: [
        {
          isFinal: true,
          0: {
            transcript: 'hello world',
            confidence: 1,
          },
        },
      ],
      resultIndex: 0,
    });

    // Assert.
    const href = await testProxy.whenCalled('navigate');
    assertEquals(href, `${googleBaseUrl}/search?q=hello+world&gs_ivs=1`);
    assertFalse(
        voiceSearchOverlay.$.micContainer.classList.contains('listening'));
    assertFalse(
        voiceSearchOverlay.$.micContainer.classList.contains('receiving'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0');
  });

  test('on error received shows error text', () => {
    // Act.
    mockSpeechRecognition.onerror({error: 'audio-capture'});

    // Assert.
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot.querySelector('#texts *[text=error]')));
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot.querySelector('#errors *[error="2"]')));
    assertFalse(
        voiceSearchOverlay.$.micContainer.classList.contains('listening'));
    assertFalse(
        voiceSearchOverlay.$.micContainer.classList.contains('receiving'));
    assertStyle(voiceSearchOverlay.$.micVolume, '--mic-volume-level', '0');
  });

  test('on end received shows error text if no final result', () => {
    // Act.
    mockSpeechRecognition.onend();

    // Assert.
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot.querySelector('#texts *[text=error]')));
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot.querySelector('#errors *[error="2"]')));
  });

  test('on end received shows result text if final result', () => {
    // Act.
    mockSpeechRecognition.onresult({
      results: [
        {
          isFinal: true,
          0: {
            transcript: 'hello world',
            confidence: 1,
          },
        },
      ],
      resultIndex: 0,
    });
    mockSpeechRecognition.onend();

    // Assert.
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot.querySelector('#texts *[text=result]')));
  });

  const test_params = [
    {
      functionName: 'onaudiostart',
      arguments: [],
    },
    {
      functionName: 'onspeechstart',
      arguments: [],
    },
    {
      functionName: 'onresult',
      arguments: [{
        results: [
          {
            isFinal: false,
            0: {
              transcript: 'hello',
              confidence: 1,
            },
          },
        ],
        resultIndex: 0,
      }],
    },
    {
      functionName: 'onend',
      arguments: [],
    },
    {
      functionName: 'onerror',
      arguments: [{error: 'audio-capture'}],
    },
    {
      functionName: 'onnomatch',
      arguments: [],
    },
  ];

  test_params.forEach(function(param) {
    test(`${param.functionName} received resets timer`, async () => {
      // Act.
      // Need to account for previously set timers.
      testProxy.resetResolver('setTimeout');
      mockSpeechRecognition[param.functionName](...param.arguments);

      // Assert.
      await testProxy.whenCalled('setTimeout');
    });
  });

  test('on idle timeout shows error text', async () => {
    // Arrange.
    const [callback, _] = await testProxy.whenCalled('setTimeout');

    // Act.
    callback();

    // Assert.
    assertTrue(isVisible(
        voiceSearchOverlay.shadowRoot.querySelector('#texts *[text=error]')));
  });

  test('on error timeout closes overlay', async () => {
    // Arrange.
    // Need to account for previously set idle timer.
    testProxy.resetResolver('setTimeout');
    mockSpeechRecognition.onerror({error: 'audio-capture'});

    // Act.
    const [callback, _] = await testProxy.whenCalled('setTimeout');
    callback();

    // Assert.
    assertFalse(voiceSearchOverlay.$.dialog.open);
  });

  test('on idle timeout stops voice search', async () => {
    // Arrange.
    const [callback, _] = await testProxy.whenCalled('setTimeout');

    // Act.
    callback();

    // Assert.
    assertTrue(mockSpeechRecognition.abortCalled);
  });

  const retryTestParams = [
    {
      name: 'retry link',
      element: 'retryLink',
    },
    {
      name: 'mic button',
      element: 'micButton',
    }
  ];

  retryTestParams.forEach(param => {
    test(`${param.name} click starts voice search if in retry state`, () => {
      // Arrange.
      mockSpeechRecognition.onnomatch();
      mockSpeechRecognition.startCalled = false;

      // Act.
      voiceSearchOverlay.shadowRoot.querySelector(`#${param.element}`).click();

      // Assert.
      assertTrue(mockSpeechRecognition.startCalled);
    });
  });
});
