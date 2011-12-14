// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.accounts', function() {
  const Event = cr.Event;

  // Email alias only, assuming it's a gmail address.
  //   e.g. 'john'
  //        {name: 'john', email: 'john@gmail.com'}
  const format1String =
      '^\\s*([\\w\\.!#\\$%&\'\\*\\+-\\/=\\?\\^`\\{\\|\\}~]+)\\s*$';
  // Email address only.
  //   e.g. 'john@chromium.org'
  //        {name: 'john', email: 'john@chromium.org'}
  const format2String =
      '^\\s*([\\w\\.!#\\$%&\'\\*\\+-\\/=\\?\\^`\\{\\|\\}~]+)@' +
      '([A-Za-z0-9\-]{2,63}\\..+)\\s*$';
  // Full format.
  //   e.g. '"John Doe" <john@chromium.org>'
  //        {name: 'John doe', email: 'john@chromium.org'}
  const format3String =
      '^\\s*"{0,1}([^"]+)"{0,1}\\s*' +
      '<([\\w\\.!#\\$%&\'\\*\\+-\\/=\\?\\^`\\{\\|\\}~]+@' +
      '[A-Za-z0-9\-]{2,63}\\..+)>\\s*$';

  /**
   * Creates a new user name edit element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLInputElement}
   */
  var UserNameEdit = cr.ui.define('input');

  UserNameEdit.prototype = {
    __proto__: HTMLInputElement.prototype,

    /**
     * Called when an element is decorated as a user name edit.
     */
    decorate: function() {
      this.pattern = format1String + '|' + format2String + '|' +
                     format3String;

      this.onkeypress = this.handleKeyPress_.bind(this);
    },


    /**
     * Parses given str for user info.
     *
     * Note that the email parsing is based on RFC 5322 and does not support
     * IMA (Internationalized Email Address). We take only the following chars
     * as valid for an email alias (aka local-part):
     *   - Letters: a–z, A–Z
     *   - Digits: 0-9
     *   - Characters: ! # $ % & ' * + - / = ? ^ _ ` { | } ~
     *   - Dot: . (Note that we did not cover the cases that dot should not
     *       appear as first or last character and should not appear two or
     *       more times in a row.)
     *
     * @param {string} str A string to parse.
     * @return {{name: string, email: string}} User info parsed from the string.
     */
    parse: function(str) {
      const format1 = new RegExp(format1String);
      const format2 = new RegExp(format2String);
      const format3 = new RegExp(format3String);

      var matches = format1.exec(str);
      if (matches) {
        return {
          name: matches[1],
          email: matches[1] + '@gmail.com'
        };
      }

      matches = format2.exec(str);
      if (matches) {
        return {
          name: matches[1],
          email: matches[1] + '@' + matches[2]
        };
      }

      matches = format3.exec(str);
      if (matches) {
        return {
          name: matches[1],
          email: matches[2]
        };
      }

      return null;
    },

    /**
     * Handler for key press event.
     * @private
     * @param {!Event} e The keypress event object.
     */
    handleKeyPress_: function(e) {
      // Enter
      if (e.keyCode == 13) {
        var user = this.parse(this.value);
        if (user) {
          var e = new Event('add');
          e.user = user;
          this.dispatchEvent(e);
        }

        this.select();
      }
    }
  };

  return {
    UserNameEdit: UserNameEdit
  };
});

