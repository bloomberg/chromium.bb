// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

ByteReader = function(arrayBuffer, opt_offset, opt_length) {
  opt_offset = opt_offset || 0;
  opt_length = opt_length || (arrayBuffer.byteLength - opt_offset);
  this.view_ = new DataView(arrayBuffer, opt_offset, opt_length);
  this.pos_ = 0;
  this.seekStack_ = [];
  this.setByteOrder(ByteReader.BIG_ENDIAN);
};

// Static const and methods.

ByteReader.LITTLE_ENDIAN = 0;  // Intel, 0x1234 is [0x34, 0x12]
ByteReader.BIG_ENDIAN = 1;  // Motorola, 0x1234 is [0x12, 0x34]

ByteReader.SEEK_BEG = 0;  // Seek relative to the beginning of the buffer.
ByteReader.SEEK_CUR = 1;  // Seek relative to the current position.
ByteReader.SEEK_END = 2;  // Seek relative to the end of the buffer.

/**
 * Throw an error if (0 > pos >= end) or if (pos + size > end).
 *
 * Static utility function.
 */
ByteReader.validateRead = function(pos, size, end) {
  if (pos < 0 || pos >= end)
    throw new Error('Invalid read position');

  if (pos + size > end)
    throw new Error('Read past end of buffer');
};

/**
 * Read as a sequence of characters, returning them as a single string.
 *
 * This is a static utility function.  There is a member function with the
 * same name which side-effects the current read position.
 */
ByteReader.readString = function(dataView, pos, size, opt_end) {
  ByteReader.validateRead(pos, size, opt_end || dataView.byteLength);

  var codes = [];

  for (var i = 0; i < size; ++i)
    codes.push(dataView.getUint8(pos + i));

  return String.fromCharCode.apply(null, codes);
};

/**
 * Read as a sequence of characters, returning them as a single string.
 *
 * This is a static utility function.  There is a member function with the
 * same name which side-effects the current read position.
 */
ByteReader.readNullTerminatedString = function(dataView, pos, size, opt_end) {
  ByteReader.validateRead(pos, size, opt_end || dataView.byteLength);

  var codes = [];

  for (var i = 0; i < size; ++i) {
    var code = dataView.getUint8(pos + i);
    if (code == 0) break;
    codes.push(code);
  }

  return String.fromCharCode.apply(null, codes);
};

ByteReader.base64Alphabet_ =
    ('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/').
    split('');

/**
 * Read as a sequence of bytes, returning them as a single base64 encoded
 * string.
 *
 * This is a static utility function.  There is a member function with the
 * same name which side-effects the current read position.
 */
ByteReader.readBase64 = function(dataView, pos, size, opt_end) {
  ByteReader.validateRead(pos, size, opt_end || dataView.byteLength);

  var rv = [];
  var chars = [];
  var padding = 0;

  for (var i = 0; i < size; /* incremented inside */) {
    var bits = dataView.getUint8(pos + (i++)) << 16;

    if (i < size) {
      bits |= dataView.getUint8(pos + (i++)) << 8;

      if (i < size) {
        bits |= dataView.getUint8(pos + (i++));
      } else {
        padding = 1;
      }
    } else {
      padding = 2;
    }

    chars[3] = ByteReader.base64Alphabet_[bits & 63];
    chars[2] = ByteReader.base64Alphabet_[(bits >> 6) & 63];
    chars[1] = ByteReader.base64Alphabet_[(bits >> 12) & 63];
    chars[0] = ByteReader.base64Alphabet_[(bits >> 18) & 63];

    rv.push.apply(rv, chars);
  }

  if (padding > 0)
    rv[rv.length - 1] = '=';
  if (padding > 1)
    rv[rv.length - 2] = '=';

  return rv.join('');
};

/**
 * Read as an image encoded in a data url.
 *
 * This is a static utility function.  There is a member function with the
 * same name which side-effects the current read position.
 */
ByteReader.readImage = function(dataView, pos, size, opt_end) {
  opt_end = opt_end || dataView.byteLength;
  ByteReader.validateRead(pos, size, opt_end);

  var format;
  if (ByteReader.readString(dataView, pos, 4, opt_end) == '\x89PNG') {
    format = 'png';
  } else if (dataView.getUint16(pos, false) == 0xFFD8) {  // Always big endian.
    format = 'jpeg';
  } else {
    format = 'unknown';
  }

  var b64 = ByteReader.readBase64(dataView, pos, size, opt_end);
  return 'data:image/' + format + ';base64,' + b64;
};

// Instance methods.

/**
 * Return true if the requested number of bytes can be read from the buffer.
 */
ByteReader.prototype.canRead = function(size) {
   return this.pos_ + size <= this.view_.byteLength;
},

/**
 * Return true if the current position is past the end of the buffer.
 */
ByteReader.prototype.eof = function() {
  return this.pos_ >= this.view_.byteLength;
};

/**
 * Return true if the current position is before the beginning of the buffer.
 */
ByteReader.prototype.bof = function() {
  return this.pos_ < 0;
};

/**
 * Return true if the current position is outside the buffer.
 */
ByteReader.prototype.beof = function() {
  return this.pos_ >= this.view_.byteLength || this.pos_ < 0;
};

/**
 * Set the expected byte ordering for future reads.
 */
ByteReader.prototype.setByteOrder = function(order) {
  this.littleEndian_ = order == ByteReader.LITTLE_ENDIAN;
};

/**
 * Throw an error if the reader is at an invalid position, or if a read a read
 * of |size| would put it in one.
 *
 * You may optionally pass opt_end to override what is considered to be the
 * end of the buffer.
 */
ByteReader.prototype.validateRead = function(size, opt_end) {
  if (typeof opt_end == 'undefined')
    opt_end = this.view_.byteLength;

  ByteReader.validateRead(this.view_, this.pos_, size, opt_end);
};

ByteReader.prototype.readScalar = function(width, opt_signed, opt_end) {
  var method = opt_signed ? 'getInt' : 'getUint';

  switch (width) {
    case 1:
      method += '8';
      break;

    case 2:
      method += '16';
      break;

    case 4:
      method += '32';
      break;

    case 8:
      method += '64';
      break;

    default:
      throw new Error('Invalid width: ' + width);
      break;
  }

  this.validateRead(width, opt_end);
  var rv = this.view_[method](this.pos_, this.littleEndian_);
  this.pos_ += width;
  return rv;
}

/**
 * Read as a sequence of characters, returning them as a single string.
 *
 * Adjusts the current position on success.  Throws an exception if the
 * read would go past the end of the buffer.
 */
ByteReader.prototype.readString = function(size, opt_end) {
  var rv = ByteReader.readString(this.view_, this.pos_, size, opt_end);
  this.pos_ += size;
  return rv;
};


/**
 * Read as a sequence of characters, returning them as a single string.
 *
 * Adjusts the current position on success.  Throws an exception if the
 * read would go past the end of the buffer.
 */
ByteReader.prototype.readNullTerminatedString = function(size, opt_end) {
  var rv = ByteReader.readNullTerminatedString(this.view_,
                                               this.pos_,
                                               size,
                                               opt_end);
  this.pos_ += rv.length;

  if (rv.length < size) {
    // If we've stopped reading because we found '0' but didn't hit size limit
    // then we should skip additional '0' character
    this.pos_++;
  }

  return rv;
};


/**
 * Read as an array of numbers.
 *
 * Adjusts the current position on success.  Throws an exception if the
 * read would go past the end of the buffer.
 */
ByteReader.prototype.readSlice = function(size, opt_end,
                                          opt_arrayConstructor) {
  this.validateRead(width, opt_end);

  var arrayConstructor = opt_arrayConstructor || Uint8Array;
  var slice = new arrayConstructor(
      this.view_.buffer, this.view_.byteOffset + this.pos, size);
  this.pos_ += size;

  return slice;
};

/**
 * Read as a sequence of bytes, returning them as a single base64 encoded
 * string.
 *
 * Adjusts the current position on success.  Throws an exception if the
 * read would go past the end of the buffer.
 */
ByteReader.prototype.readBase64 = function(size, opt_end) {
  var rv = ByteReader.readBase64(this.view_, this.pos_, size, opt_end);
  this.pos_ += size;
  return rv;
};

/**
 * Read an image returning it as a data url.
 *
 * Adjusts the current position on success.  Throws an exception if the
 * read would go past the end of the buffer.
 */
ByteReader.prototype.readImage = function(size, opt_end) {
  var rv = ByteReader.readImage(this.view_, this.pos_, size, opt_end);
  this.pos_ += size;
  return rv;
};

/**
 * Seek to a give position relative to opt_seekStart.
 */
ByteReader.prototype.seek = function(pos, opt_seekStart, opt_end) {
  opt_end = opt_end || this.view_.byteLength;

  var newPos;
  if (opt_seekStart == ByteReader.SEEK_CUR) {
    newPos = this.pos_ + pos;
  } else if (opt_seekStart == ByteReader.SEEK_END) {
    newPos = opt_end + pos;
  } else {
    newPos = pos;
  }

  if (newPos < 0 || newPos > this.view_.byteLength)
    throw new Error('Seek outside of buffer: ' + (newPos - opt_end));

  this.pos_ = newPos;
};

/**
 * Seek to a given position relative to opt_seekStart, saving the current
 * position.
 *
 * Recover the current position with a call to seekPop.
 */
ByteReader.prototype.pushSeek = function(pos, opt_seekStart) {
  var oldPos = this.pos_;
  this.seek(pos, opt_seekStart);
  // Alter the seekStack_ after the call to seek(), in case it throws.
  this.seekStack_.push(oldPos);
};

/**
 * Undo a previous seekPush.
 */
ByteReader.prototype.popSeek = function() {
  this.seek(this.seekStack_.pop());
};

/**
 * Return the current read position.
 */
ByteReader.prototype.tell = function() {
  return this.pos_;
};
