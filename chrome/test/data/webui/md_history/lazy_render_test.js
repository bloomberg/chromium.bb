// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.lazy_render_test', function() {
  function registerTests() {
    suite('history-lazy-render', function() {

      setup(function() {
        PolymerTest.clearBody();
        var template =
            '<template is="dom-bind" id="bind">' +
            '  <template is="history-lazy-render" id="lazy">' +
            '    <h1>' +
            '      <paper-checkbox checked="{{checked}}"></paper-checkbox>' +
            '      {{name}}' +
            '    </h1>' +
            '  </template>' +
            '</template>';
        document.body.innerHTML = template;
      });

      test('stamps after get()', function() {
        var lazy = document.getElementById('lazy');

        assertFalse(!!document.body.querySelector('h1'));
        assertFalse(!!lazy.getIfExists());

        return lazy.get().then(function(inner) {
          assertEquals('H1', inner.nodeName);
          assertEquals(inner, document.body.querySelector('h1'));
        });
      });

      test('one-way binding works', function() {
        var bind = document.getElementById('bind');
        bind.name = 'Wings';
        var lazy = document.getElementById('lazy');

        return lazy.get().then(function(inner) {
          assertNotEquals(-1, inner.textContent.indexOf('Wings'));
          bind.name = 'DC';
          assertNotEquals(-1, inner.textContent.indexOf('DC'));
        });
      });

      test('two-way binding works', function() {
        var bind = document.getElementById('bind');
        bind.checked = true;

        var lazy = document.getElementById('lazy');

        return lazy.get().then(function(inner) {
          var checkbox = document.querySelector('paper-checkbox');
          assertTrue(checkbox.checked);
          MockInteractions.tap(checkbox);
          assertFalse(checkbox.checked);
          assertFalse(bind.checked);
        });
      });
    });
  }

  return {
    registerTests: registerTests,
  }
});
