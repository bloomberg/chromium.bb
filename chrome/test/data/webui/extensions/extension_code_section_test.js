// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-code-section. */
cr.define('extension_code_section_tests', function() {
  /** @enum {string} */
  var TestNames = {
    Layout: 'layout',
  };

  suite('ExtensionCodeSectionTest', function() {
    /** @type {chrome.developerPrivate.RequestFileSourceResponse} */
    var code = {
      beforeHighlight: 'this part before the highlight\nAnd this too\n',
      highlight: 'highlight this part\n',
      afterHighlight: 'this part after the highlight\n',
      message: 'Highlight message',
    };

    /** @type {extensions.CodeSection} */
    var codeSection;

    var couldNotDisplayCode = 'No code here';

    // Initialize an extension item before each test.
    setup(function() {
      PolymerTest.clearBody();
      codeSection = new extensions.CodeSection();
      codeSection.couldNotDisplayCode = couldNotDisplayCode;
      document.body.appendChild(codeSection);
    });

    test(assert(TestNames.Layout), function() {
      Polymer.dom.flush();

      var testIsVisible = extension_test_util.isVisible.bind(null, codeSection);
      expectFalse(!!codeSection.code);
      expectTrue(codeSection.isEmpty());
      expectTrue(codeSection.$$('#scroll-container').hidden);
      expectFalse(testIsVisible('#main'));
      expectTrue(testIsVisible('#no-code'));
      expectEquals('', codeSection.$['line-numbers'].textContent.trim());

      codeSection.code = code;
      expectTrue(testIsVisible('#main'));
      expectFalse(testIsVisible('#no-code'));

      var fullSpan = codeSection.$$('#source span');
      expectEquals(
          code.beforeHighlight + code.highlight + code.afterHighlight,
          fullSpan.textContent);
      var highlightSpan = codeSection.$$('#highlight');
      expectEquals(code.highlight, highlightSpan.textContent);
      expectEquals(code.message, highlightSpan.title);
      expectEquals(
          '1\n2\n3\n4', codeSection.$['line-numbers'].textContent.trim());
    });
  });

  return {
    TestNames: TestNames,
  };
});
