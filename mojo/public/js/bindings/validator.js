// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/bindings/validator", [
  "mojo/public/js/bindings/codec",
  ], function(codec) {

  var validationError = {
    NONE: 'VALIDATION_ERROR_NONE',
    MISALIGNED_OBJECT: 'VALIDATION_ERROR_MISALIGNED_OBJECT',
    ILLEGAL_MEMORY_RANGE: 'VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE',
    UNEXPECTED_STRUCT_HEADER: 'VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER',
    UNEXPECTED_ARRAY_HEADER: 'VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER',
    ILLEGAL_HANDLE: 'VALIDATION_ERROR_ILLEGAL_HANDLE',
    ILLEGAL_POINTER: 'VALIDATION_ERROR_ILLEGAL_POINTER',
    MESSAGE_HEADER_INVALID_FLAG_COMBINATION:
        'VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAG_COMBINATION',
    MESSAGE_HEADER_MISSING_REQUEST_ID:
        'VALIDATION_ERROR_MESSAGE_HEADER_MISSING_REQUEST_ID'
  };

  function Validator(message) {
    this.message = message;
    this.offset = 0;
  }

  Object.defineProperty(Validator.prototype, "offsetLimit", {
    get: function() { return this.message.buffer.byteLength; }
  });

  // True if we can safely allocate a block of bytes from start to
  // to start + numBytes.
  Validator.prototype.isValidRange = function(start, numBytes) {
    // Only positive JavaScript integers that are less than 2^53
    // (Number.MAX_SAFE_INTEGER) can be represented exactly.
    if (start < this.offset || numBytes <= 0 ||
        !Number.isSafeInteger(start) ||
        !Number.isSafeInteger(numBytes))
      return false;

    var newOffset = start + numBytes;
    if (!Number.isSafeInteger(newOffset) || newOffset > this.offsetLimit)
      return false;

    return true;
  }

  Validator.prototype.claimRange = function(start, numBytes) {
    if (this.isValidRange(start, numBytes)) {
      this.offset = start + numBytes;
      return true;
    }
    return false;
  }

  Validator.prototype.validateStructHeader =
      function(offset, minNumBytes, minNumFields) {
    if (!codec.isAligned(offset))
      return validationError.MISALIGNED_OBJECT;

    if (!this.isValidRange(offset, codec.kStructHeaderSize))
      return validationError.ILLEGAL_MEMORY_RANGE;

    var numBytes = this.message.buffer.getUint32(offset);
    var numFields = this.message.buffer.getUint32(offset + 4);

    if (numBytes < minNumBytes || numFields < minNumFields)
      return validationError.UNEXPECTED_STRUCT_HEADER;

    if (!this.claimRange(offset, numBytes))
      return validationError.ILLEGAL_MEMORY_RANGE;

    return validationError.NONE;
  }

  Validator.prototype.validateMessageHeader = function() {
    var numBytes = this.message.getHeaderNumBytes();
    var numFields = this.message.getHeaderNumFields();

    var validNumFieldsAndNumBytes =
        (numFields == 2 && numBytes == codec.kMessageHeaderSize) ||
        (numFields == 3 &&
         numBytes == codec.kMessageWithRequestIDHeaderSize) ||
        (numFields > 3 &&
         numBytes >= codec.kMessageWithRequestIDHeaderSize);
    if (!validNumFieldsAndNumBytes)
      return validationError.UNEXPECTED_STRUCT_HEADER;

    var expectsResponse = this.message.expectsResponse();
    var isResponse = this.message.isResponse();

    if (numFields == 2 && (expectsResponse || isResponse))
      return validationError.MESSAGE_HEADER_MISSING_REQUEST_ID;

    if (isResponse && expectsResponse)
      return validationError.MESSAGE_HEADER_INVALID_FLAG_COMBINATION;

    return validationError.NONE;
  }

  Validator.prototype.validateMessage = function() {
    var err = this.validateStructHeader(0, codec.kStructHeaderSize, 2);
    if (err != validationError.NONE)
      return err;

    return this.validateMessageHeader();
  }

  var exports = {};
  exports.validationError = validationError;
  exports.Validator = Validator;
  return exports;
});
