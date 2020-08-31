// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_next_e2e_test_base.js']);
GEN_INCLUDE(['../testing/mock_feedback.js']);

/**
 * Test fixture for LocaleOutputHelper.
 */
ChromeVoxLocaleOutputHelperTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
  #include "base/command_line.h"
  #include "ui/accessibility/accessibility_switches.h"
  #include "ui/base/ui_base_switches.h"
      `);
  }

  /** @override */
  testGenPreamble() {
    GEN(`
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(::switches::kLang, "en-US");
      `);
    super.testGenPreamble();
  }

  /** @override */
  setUp() {
    window.doCmd = this.doCmd;
    // Mock this api to return a predefined set of voices.
    chrome.tts.getVoices = function(callback) {
      callback([
        // All properties of TtsVoice object are optional.
        // https://developer.chrome.com/apps/tts#type-TtsVoice.
        {}, {voiceName: 'Android'}, {'lang': 'en-US'}, {'lang': 'fr-CA'},
        {'lang': 'es-ES'}, {'lang': 'it-IT'}, {'lang': 'ja-JP'},
        {'lang': 'ko-KR'}, {'lang': 'zh-TW'}, {'lang': 'ast'}, {'lang': 'pt'}
      ]);
    };
  }

  /**
   * @return {!MockFeedback}
   */
  createMockFeedback() {
    const mockFeedback =
        new MockFeedback(this.newCallback(), this.newCallback.bind(this));

    mockFeedback.install();
    return mockFeedback;
  }

  /**
   * Create a function which performs the command |cmd|.
   * @param {string} cmd
   * @return {function(): void}
   */
  doCmd(cmd) {
    return function() {
      CommandHandler.onCommand(cmd);
    };
  }

  /**
   * Calls mock version of chrome.tts.getVoices() to populate
   * LocaleOutputHelper's available voice list with a specific set of voices.
   */
  setAvailableVoices() {
    chrome.tts.getVoices(function(voices) {
      LocaleOutputHelper.instance.availableVoices_ = voices;
    });
  }

  get asturianAndJapaneseDoc() {
    return `
      <meta charset="utf-8">
      <p lang="ja">ど</p>
      <p lang="ast">
        Pretend that this text is Asturian. Testing three-letter language code logic.
      </p>
    `;
  }

  get buttonAndLinkDoc() {
    return `
      <body lang="es">
        <p>This is a paragraph, written in English.</p>
        <button type="submit">This is a button, written in English.</button>
        <a href="https://www.google.com">Este es un enlace.</a>
      </body>
    `;
  }

  get englishAndFrenchUnlabeledDoc() {
    return `
      <p>
        This entire object should be read in English, even the following French passage:
        salut mon ami! Ca va? Bien, et toi? It's hard to differentiate between latin-based languages.
      </p>
    `;
  }

  get englishAndKoreanUnlabeledDoc() {
    return `
      <meta charset="utf-8">
      <p>This text is written in English. 차에 한하여 중임할 수. This text is also written in English.</p>
    `;
  }

  get japaneseAndChineseUnlabeledDoc() {
    return `
      <meta charset="utf-8">
      <p id="text">
        天気はいいですね. 右万諭全中結社原済権人点掲年難出面者会追
      </p>
    `;
  }

  get japaneseAndEnglishUnlabeledDoc() {
    return `
      <meta charset="utf-8">
      <p>Hello, my name is 太田あきひろ. It's a pleasure to meet you. どうぞよろしくお願いします.</p>
    `;
  }

  get japaneseAndKoreanUnlabeledDoc() {
    return `
      <meta charset="utf-8">
      <p lang="ko">
        私は. 법률이 정하는 바에 의하여 대법관이 아닌 법관을 둘 수 있다
      </p>
    `;
  }

  get japaneseCharacterUnlabeledDoc() {
    return `
      <meta charset="utf-8">
      <p>ど</p>
    `;
  }

  get multipleLanguagesLabeledDoc() {
    return `
      <p lang="es">Hola.</p>
      <p lang="en">Hello.</p>
      <p lang="fr">Salut.</p>
      <span lang="it">Ciao amico.</span>
    `;
  }

  get japaneseAndInvalidLanguagesLabeledDoc() {
    return `
      <meta charset="utf-8">
      <p lang="ja">どうぞよろしくお願いします</p>
      <p lang="invalid-code">Test</p>
      <p lang="hello">Yikes</p>
    `;
  }

  get nestedLanguagesLabeledDoc() {
    return `
      <p id="breakfast" lang="en">In the morning, I sometimes eat breakfast.</p>
      <p id="lunch" lang="fr">Dans l'apres-midi, je dejeune.</p>
      <p id="greeting" lang="en">
        Hello it's a pleasure to meet you.
        <span lang="fr">Comment ca va?</span>Switching back to English.
        <span lang="es">Hola.</span>Goodbye.
      </p>
    `;
  }

  get vietnameseAndUrduLabeledDoc() {
    return `
      <p lang="vi">Vietnamese text.</p>
      <p lang="ur">Urdu text.</p>
    `;
  }

  get chineseDoc() {
    return `
      <p lang="en-us">United States</p>
      <p lang="zh">Chinese</p>
      <p lang="zh-hans">Simplified Chinese</p>
      <p lang="zh-hant">Traditional Chinese</p>
      <p lang="zh">Chinese</p>
    `;
  }

  get portugueseDoc() {
    return `
      <p lang="en-us">United States</p>
      <p lang="pt">Portuguese</p>
      <p lang="pt-br">Brazil</p>
      <p lang="pt-pt">Portugal</p>
      <p lang="pt">Portuguese</p>
    `;
  }
};

TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'MultipleLanguagesLabeledDocTest',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(this.multipleLanguagesLabeledDoc, function() {
        localStorage['languageSwitching'] = 'true';
        this.setAvailableVoices();
        mockFeedback.call(doCmd('jumpToTop'))
            .expectSpeechWithLocale('es', 'español: Hola.');
        mockFeedback.call(doCmd('nextLine'))
            .expectSpeechWithLocale('en', 'English: Hello.');
        mockFeedback.call(doCmd('nextLine'))
            .expectSpeechWithLocale('fr', 'français: Salut.');
        mockFeedback.call(doCmd('nextLine'))
            .expectSpeechWithLocale('it', 'italiano: Ciao amico.');
        mockFeedback.replay();
      });
    });

TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'NestedLanguagesLabeledDocTest',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(this.nestedLanguagesLabeledDoc, function() {
        localStorage['languageSwitching'] = 'true';
        this.setAvailableVoices();
        mockFeedback.call(doCmd('jumpToTop'))
            .expectSpeechWithLocale(
                'en', 'English: In the morning, I sometimes eat breakfast.');
        mockFeedback.call(doCmd('nextLine'))
            .expectSpeechWithLocale(
                'fr', 'français: Dans l\'apres-midi, je dejeune.');
        mockFeedback.call(doCmd('nextLine'))
            .expectSpeechWithLocale(
                'en', 'English: Hello it\'s a pleasure to meet you. ');
        mockFeedback.call(doCmd('nextLine'))
            .expectSpeechWithLocale('fr', 'français: Comment ca va?');
        mockFeedback.call(doCmd('nextLine'))
            .expectSpeechWithLocale(
                'en', 'English: Switching back to English. ');
        mockFeedback.call(doCmd('nextLine'))
            .expectSpeechWithLocale('es', 'español: Hola.');
        mockFeedback.call(doCmd('nextLine'))
            .expectSpeechWithLocale('en', 'English: Goodbye.');
        mockFeedback.replay();
      });
    });

TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'ButtonAndLinkDocTest', function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(this.buttonAndLinkDoc, function(root) {
        localStorage['languageSwitching'] = 'true';
        this.setAvailableVoices();
        mockFeedback
            .call(doCmd('jumpToTop'))
            // Use the author-provided language of 'es'.
            .expectSpeechWithLocale(
                'es', 'español: This is a paragraph, written in English.')
            .call(doCmd('nextObject'))
            .expectSpeechWithLocale(
                'es', 'This is a button, written in English.')
            .expectSpeechWithLocale(
                undefined, 'Button', 'Press Search+Space to activate')
            .call(doCmd('nextObject'))
            .expectSpeechWithLocale('es', 'Este es un enlace.')
            .expectSpeechWithLocale(undefined, 'Link');
        mockFeedback.replay();
      });
    });


TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'JapaneseAndEnglishUnlabeledDocTest',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(
          this.japaneseAndEnglishUnlabeledDoc, function(root) {
            localStorage['languageSwitching'] = 'true';
            this.setAvailableVoices();
            mockFeedback
                .call(doCmd('jumpToTop'))
                // Expect the node's contents to be read in one language
                // (English).
                // Language detection does not run on small runs of text, like
                // the one in this test, we are falling back on the UI language
                // of the browser, which is en-US. Please see testGenPreamble
                // for more details.
                .expectSpeechWithLocale(
                    'en-us',
                    'Hello, my name is 太田あきひろ. It\'s a pleasure to meet' +
                        ' you. どうぞよろしくお願いします.');
            mockFeedback.replay();
          });
    });


TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'EnglishAndKoreanUnlabeledDocTest',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(this.englishAndKoreanUnlabeledDoc, function(root) {
        localStorage['languageSwitching'] = 'true';
        this.setAvailableVoices();
        mockFeedback.call(doCmd('jumpToTop'))
            .expectSpeechWithLocale(
                'en-us',
                'This text is written in English. 차에 한하여 중임할 수.' +
                    ' This text is also written in English.');
        mockFeedback.replay();
      });
    });

TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'EnglishAndFrenchUnlabeledDocTest',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(this.englishAndFrenchUnlabeledDoc, function(root) {
        localStorage['languageSwitching'] = 'true';
        this.setAvailableVoices();
        mockFeedback.call(doCmd('jumpToTop'))
            .expectSpeechWithLocale(
                'en',
                'English: This entire object should be read in English, even' +
                    ' the following French passage: ' +
                    'salut mon ami! Ca va? Bien, et toi? It\'s hard to' +
                    ' differentiate between latin-based languages.');
        mockFeedback.replay();
      });
    });

TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'JapaneseCharacterUnlabeledDocTest',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(
          this.japaneseCharacterUnlabeledDoc, function(root) {
            localStorage['languageSwitching'] = 'true';
            this.setAvailableVoices();
            mockFeedback.call(doCmd('jumpToTop'))
                .expectSpeechWithLocale('en-us', 'ど');
            mockFeedback.replay();
          });
    });

TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'JapaneseAndChineseUnlabeledDocTest',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(this.japaneseAndChineseUnlabeledDoc, function(root) {
        localStorage['languageSwitching'] = 'true';
        this.setAvailableVoices();
        mockFeedback.call(doCmd('jumpToTop'))
            .expectSpeechWithLocale(
                'en-us',
                '天気はいいですね. 右万諭全中結社原済権人点掲年難出面者会追');
        mockFeedback.replay();
      });
    });

TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'JapaneseAndChineseLabeledDocTest',
    function() {
      const mockFeedback = this.createMockFeedback();
      // Only difference between doc used in this test and
      // this.japaneseAndChineseUnlabeledDoc is the lang="zh" attribute.
      this.runWithLoadedTree(
          `
        <meta charset="utf-8">
        <p lang="zh">
          天気はいいですね. 右万諭全中結社原済権人点掲年難出面者会追
        </p>
    `,
          function(root) {
            localStorage['languageSwitching'] = 'true';
            this.setAvailableVoices();
            mockFeedback.call(doCmd('jumpToTop'))
                .expectSpeechWithLocale(
                    'zh',
                    '中文: 天気はいいですね. 右万諭全中結社原済権人点掲年難出面者会追');
            mockFeedback.replay();
          });
    });

TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'JapaneseAndKoreanUnlabeledDocTest',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(this.japaneseAndKoreanUnlabeledDoc, function(root) {
        localStorage['languageSwitching'] = 'true';
        this.setAvailableVoices();
        // Language detection runs and assigns language of 'ko' to the node.
        mockFeedback.call(doCmd('jumpToTop'))
            .expectSpeechWithLocale(
                'ko',
                '한국어: 私は. 법률이 정하는 바에 의하여 대법관이 아닌 법관을 둘 수' +
                    ' 있다');
        mockFeedback.replay();
      });
    });

TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'AsturianAndJapaneseDocTest',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(this.asturianAndJapaneseDoc, function(root) {
        localStorage['languageSwitching'] = 'true';
        this.setAvailableVoices();
        mockFeedback.call(doCmd('jumpToTop'))
            .expectSpeechWithLocale('ja', '日本語: ど')
            .call(doCmd('nextObject'))
            .expectSpeechWithLocale(
                'ast',
                'asturianu: Pretend that this text is Asturian. Testing' +
                    ' three-letter language code logic.');
        mockFeedback.replay();
      });
    });


TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'LanguageSwitchingOffTest', function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(this.multipleLanguagesLabeledDoc, function(root) {
        localStorage['languageSwitching'] = 'false';
        this.setAvailableVoices();
        // Locale should not be set if the language switching feature is off.
        mockFeedback.call(doCmd('jumpToTop'))
            .expectSpeechWithLocale(undefined, 'Hola.')
            .call(doCmd('nextObject'))
            .expectSpeechWithLocale(undefined, 'Hello.')
            .call(doCmd('nextObject'))
            .expectSpeechWithLocale(undefined, 'Salut.')
            .call(doCmd('nextObject'))
            .expectSpeechWithLocale(undefined, 'Ciao amico.');
        mockFeedback.replay();
      });
    });

TEST_F('ChromeVoxLocaleOutputHelperTest', 'DefaultToUILocaleTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      this.japaneseAndInvalidLanguagesLabeledDoc, function(root) {
        localStorage['languageSwitching'] = 'true';
        this.setAvailableVoices();
        mockFeedback.call(doCmd('jumpToTop'))
            .expectSpeechWithLocale('ja', '日本語: どうぞよろしくお願いします')
            .call(doCmd('nextObject'))
            .expectSpeechWithLocale('en-us', 'English (United States): Test')
            .call(doCmd('nextObject'))
            .expectSpeechWithLocale('en-us', 'Yikes');
        mockFeedback.replay();
      });
});

TEST_F('ChromeVoxLocaleOutputHelperTest', 'NoAvailableVoicesTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.vietnameseAndUrduLabeledDoc, function(root) {
    localStorage['languageSwitching'] = 'true';
    this.setAvailableVoices();
    mockFeedback.call(doCmd('jumpToTop'))
        .expectSpeechWithLocale(
            'en-us', 'No voice available for language: Vietnamese')
        .call(doCmd('nextObject'))
        .expectSpeechWithLocale(
            'en-us', 'No voice available for language: Urdu');
    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxLocaleOutputHelperTest', 'WordNavigationTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.nestedLanguagesLabeledDoc, function() {
    localStorage['languageSwitching'] = 'true';
    this.setAvailableVoices();
    mockFeedback.call(doCmd('jumpToTop'))
        .expectSpeechWithLocale(
            'en', 'English: In the morning, I sometimes eat breakfast.')
        .call(doCmd('nextLine'))
        .expectSpeechWithLocale(
            'fr', 'français: Dans l\'apres-midi, je dejeune.')
        .call(doCmd('nextWord'))
        .expectSpeechWithLocale('fr', `l'apres`)
        .call(doCmd('nextWord'))
        .expectSpeechWithLocale('fr', `midi`)
        .call(doCmd('nextWord'))
        .expectSpeechWithLocale('fr', `je`)
        .call(doCmd('nextWord'))
        .expectSpeechWithLocale('fr', `dejeune`)
        .call(doCmd('nextWord'))
        .expectSpeechWithLocale('en', `English: Hello`)
        .call(doCmd('nextWord'))
        .expectSpeechWithLocale('en', `it's`)
        .call(doCmd('nextWord'))
        .expectSpeechWithLocale('en', `a`)
        .call(doCmd('nextWord'))
        .expectSpeechWithLocale('en', `pleasure`)
        .call(doCmd('nextWord'))
        .expectSpeechWithLocale('en', `to`)
        .call(doCmd('nextWord'))
        .expectSpeechWithLocale('en', `meet`)
        .call(doCmd('nextWord'))
        .expectSpeechWithLocale('en', `you`)
        .call(doCmd('nextWord'))
        .expectSpeechWithLocale('fr', `français: Comment`)
        .call(doCmd('previousWord'))
        .expectSpeechWithLocale('en', `English: you`)
        .replay();
  });
});

TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'CharacterNavigationTest', function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(this.nestedLanguagesLabeledDoc, function() {
        localStorage['languageSwitching'] = 'true';
        this.setAvailableVoices();
        mockFeedback.call(doCmd('jumpToTop'))
            .expectSpeechWithLocale(
                'en', 'English: In the morning, I sometimes eat breakfast.')
            .call(doCmd('nextLine'))
            .expectSpeechWithLocale(
                'fr', 'français: Dans l\'apres-midi, je dejeune.')
            .call(doCmd('nextCharacter'))
            .expectSpeechWithLocale('fr', `a`)
            .call(doCmd('nextCharacter'))
            .expectSpeechWithLocale('fr', `n`)
            .call(doCmd('nextCharacter'))
            .expectSpeechWithLocale('fr', `s`)
            .call(doCmd('nextCharacter'))
            .expectSpeechWithLocale('fr', ` `)
            .call(doCmd('nextCharacter'))
            .expectSpeechWithLocale('fr', `l`)
            .call(doCmd('nextLine'))
            .expectSpeechWithLocale(
                'en', `English: Hello it's a pleasure to meet you. `)
            .call(doCmd('nextCharacter'))
            .expectSpeechWithLocale('en', `e`)
            .call(doCmd('previousCharacter'))
            .expectSpeechWithLocale('en', `H`)
            .call(doCmd('previousCharacter'))
            .expectSpeechWithLocale('fr', `français: .`)
            .call(doCmd('previousCharacter'))
            .expectSpeechWithLocale('fr', `e`)
            .call(doCmd('previousCharacter'))
            .expectSpeechWithLocale('fr', `n`)
            .call(doCmd('previousCharacter'))
            .expectSpeechWithLocale('fr', `u`)
            .call(doCmd('previousCharacter'))
            .expectSpeechWithLocale('fr', `e`)
            .call(doCmd('previousCharacter'))
            .expectSpeechWithLocale('fr', `j`)
            .replay();
      });
    });

TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'SwitchBetweenChineseDialectsTest',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(this.chineseDoc, function() {
        localStorage['languageSwitching'] = 'true';
        this.setAvailableVoices();
        mockFeedback.call(doCmd('jumpToTop'))
            .expectSpeechWithLocale('en-us', 'United States')
            .call(doCmd('nextLine'))
            .expectSpeechWithLocale('zh', '中文: Chinese')
            .call(doCmd('nextLine'))
            .expectSpeechWithLocale(
                'zh-hans', '中文（简体）: Simplified Chinese')
            .call(doCmd('nextLine'))
            .expectSpeechWithLocale(
                'zh-hant', '中文（繁體）: Traditional Chinese')
            .call(doCmd('nextLine'))
            .expectSpeechWithLocale('zh', '中文: Chinese');
        mockFeedback.replay();
      });
    });

TEST_F(
    'ChromeVoxLocaleOutputHelperTest', 'SwitchBetweenPortugueseDialectsTest',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(this.portugueseDoc, function() {
        localStorage['languageSwitching'] = 'true';
        this.setAvailableVoices();
        mockFeedback.call(doCmd('jumpToTop'))
            .expectSpeechWithLocale('en-us', 'United States')
            .call(doCmd('nextLine'))
            .expectSpeechWithLocale('pt', 'português: Portuguese')
            .call(doCmd('nextLine'))
            .expectSpeechWithLocale('pt-br', 'português (Brasil): Brazil')
            .call(doCmd('nextLine'))
            .expectSpeechWithLocale('pt-pt', 'português (Portugal): Portugal')
            .call(doCmd('nextLine'))
            .expectSpeechWithLocale('pt', 'português: Portuguese');
        mockFeedback.replay();
      });
    });
