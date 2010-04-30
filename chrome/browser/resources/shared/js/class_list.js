// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements the HTML5 HTMLElement classList property.
 */

// TODO(arv): Remove this when classList is available in WebKit.
// https://bugs.webkit.org/show_bug.cgi?id=20709

if (typeof document.createElement('div').classList == 'undefined') {

function DOMTokenList(el) {
  this.el_ = el;
}

(function() {

  var re = /\s+/;

  function split(el) {
    var cn = el.className.replace(/^\s+|\s$/g, '');
    if (cn == '')
      return [];
    return cn.split(re);
  }

  function DOMException_(code) {
    this.code = code;
  }
  DOMException_.prototype = DOMException.prototype;

  function assertValidToken(s) {
    if (s == '')
      throw new DOMException_(DOMException.SYNTAX_ERR);
    if (re.test(s))
      throw new DOMException_(DOMException.INVALID_CHARACTER_ERR);
  }

  DOMTokenList.prototype = {
    contains: function(token) {
      assertValidToken(token);
      return split(this.el_).indexOf(token) >= 0;
    },
    add: function(token) {
      assertValidToken(token);
      if (this.contains(token))
        return;
      var cn = this.el_.className;
      this.el_.className += (cn == '' || re.test(cn.slice(-1)) ? '' : ' ') +
          token;
    },
    remove: function(token) {
      assertValidToken(token);
      var names = split(this.el_);
      var s = [];
      for (var i = 0; i < names.length; i++) {
        if (names[i] != token)
          s.push(names[i])
      }
      this.el_.className = s.join(' ');
    },
    toggle: function(token) {
      assertValidToken(token);
      if (this.contains(token)) {
        this.remove(token);
        return false;
      } else {
        this.add(token);
        return true;
      }
    },
    get length() {
      return split(this.el_).length;
    },
    item: function(index) {
      return split(this.el_)[index];
    }
  };

})();

HTMLElement.prototype.__defineGetter__('classList', function() {
  var tl = new DOMTokenList(this);
  // Override so that we reuse the same DOMTokenList and so that
  // el.classList == el.classList
  this.__defineGetter__('classList', function() {
    return tl;
  });
  return tl;
});

} // if
