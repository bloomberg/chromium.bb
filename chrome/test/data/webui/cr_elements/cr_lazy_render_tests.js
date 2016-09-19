// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-lazy-render', function() {
  var lazy;
  var bind;

  suiteSetup(function() {
    return PolymerTest.importHtml(
        'chrome://resources/polymer/v1_0/paper-checkbox/paper-checkbox.html');
  });

  setup(function() {
    PolymerTest.clearBody();
    var template =
        '<template is="dom-bind" id="bind">' +
        '  <template is="cr-lazy-render" id="lazy">' +
        '    <h1>' +
        '      <paper-checkbox checked="{{checked}}"></paper-checkbox>' +
        '      {{name}}' +
        '    </h1>' +
        '  </template>' +
        '</template>';
    document.body.innerHTML = template;
    lazy = document.getElementById('lazy');
    bind = document.getElementById('bind');
  });

  test('stamps after get()', function() {
    assertFalse(!!document.body.querySelector('h1'));
    assertFalse(!!lazy.getIfExists());

    var inner = lazy.get();
    assertEquals('H1', inner.nodeName);
    assertEquals(inner, document.body.querySelector('h1'));
  });

  test('one-way binding works', function() {
    bind.name = 'Wings';

    var inner = lazy.get();
    assertNotEquals(-1, inner.textContent.indexOf('Wings'));
    bind.name = 'DC';
    assertNotEquals(-1, inner.textContent.indexOf('DC'));
  });

  test('two-way binding works', function() {
    bind.checked = true;

    var inner = lazy.get();
    var checkbox = document.querySelector('paper-checkbox');
    assertTrue(checkbox.checked);
    MockInteractions.tap(checkbox);
    assertFalse(checkbox.checked);
    assertFalse(bind.checked);
  });
});
