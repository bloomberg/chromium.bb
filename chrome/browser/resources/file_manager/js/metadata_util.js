// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var parseUtil = {
  /**
   * Big endian read.  Most significant bytes come first.
   */
  parseBig: function(bytes, pos, width) {
    var rv = 0;
    switch(width) {
      case 4:
        rv = bytes[pos++] << 24;
      case 3:
        rv |= bytes[pos++] << 16;
      case 2:
        rv |= bytes[pos++] << 8;
      case 1:
        rv |= bytes[pos++];
    }
    return rv;
  },

  /**
   * Little endian read.  Least significant bytes come first.
   */
  parseLittle: function(bytes, pos, width) {
    var rv = 0;
    switch(width) {
      case 4:
        rv = bytes[pos + 3] << 24;
      case 3:
        rv |= bytes[pos + 2] << 16;
      case 2:
        rv |= bytes[pos + 1] << 8;
      case 1:
        rv |= bytes[pos];
    }
    return rv;
  },

  /**
   * Read a string.
   * TODO: implement UTF8.
   */
  parseString: function(bytes, pos, end) {
    var codes = [];
    while (pos < end) {
      codes.push(bytes[pos++]);
    }
    return String.fromCharCode.apply(null, codes);
  },

  base64Alphabet: ('ABCDEFGHIJKLMNOPQRSTUVWXYZ' +
                   'abcdefghijklmnopqrstuvwxyz' +
                   '0123456789+/').split(''),

  parseBase64: function(bytes, pos, end) {
    var rv = [];
    var chars = [];
    var padding = 0;

    while (pos < end) {
      var bits = bytes[pos++] << 16;

      if (pos < end) {
        bits |= bytes[pos++] << 8;

        if (pos < end) {
          bits |= bytes[pos++];
        } else {
          padding = 1;
        }
      } else {
        padding = 2;
      }

      chars[3] = parseUtil.base64Alphabet[bits & 63];
      chars[2] = parseUtil.base64Alphabet[(bits >> 6) & 63];
      chars[1] = parseUtil.base64Alphabet[(bits >> 12) & 63];
      chars[0] = parseUtil.base64Alphabet[(bits >> 18) & 63];

      rv.push.apply(rv, chars);
    }

    if (padding > 0)
      chars[chars.length - 1] = '=';
    if (padding > 1)
      chars[chars.length - 2] = '=';

    return rv.join('');
  },

  createImageDataUrl: function(bytes, pos, end) {
    var format;
    if (parseUtil.parseString(bytes, pos, 4) == '\x89PNG') {
      format = 'png';
    } else if (parseUtil.parseBig(bytes, pos, 2) == 0xFFD8) {
      format = 'jpeg';
    } else {
      format = 'unknown';
    }
    var b64 = parseUtil.parseBase64(bytes, pos, end);
    return 'data:image/' + format + ';base64,' + b64;
  }
}