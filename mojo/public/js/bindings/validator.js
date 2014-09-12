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
    UNEXPECTED_INVALID_HANDLE: 'VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE',
    ILLEGAL_POINTER: 'VALIDATION_ERROR_ILLEGAL_POINTER',
    UNEXPECTED_NULL_POINTER: 'VALIDATION_ERROR_UNEXPECTED_NULL_POINTER',
    MESSAGE_HEADER_INVALID_FLAG_COMBINATION:
        'VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAG_COMBINATION',
    MESSAGE_HEADER_MISSING_REQUEST_ID:
        'VALIDATION_ERROR_MESSAGE_HEADER_MISSING_REQUEST_ID'
  };

  var NULL_MOJO_POINTER = "NULL_MOJO_POINTER";

  function isStringClass(cls) {
    return cls === codec.String || cls === codec.NullableString;
  }

  function isHandleClass(cls) {
    return cls === codec.Handle || cls === codec.NullableHandle;
  }

  function isNullable(type) {
    return type === codec.NullableString || type === codec.NullableHandle ||
        type instanceof codec.NullableArrayOf ||
        type instanceof codec.NullablePointerTo;
  }

  function Validator(message) {
    this.message = message;
    this.offset = 0;
    this.handleIndex = 0;
  }

  Object.defineProperty(Validator.prototype, "offsetLimit", {
    get: function() { return this.message.buffer.byteLength; }
  });

  Object.defineProperty(Validator.prototype, "handleIndexLimit", {
    get: function() { return this.message.handles.length; }
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

  Validator.prototype.claimHandle = function(index) {
    if (index === codec.kEncodedInvalidHandleValue)
      return true;

    if (index < this.handleIndex || index >= this.handleIndexLimit)
      return false;

    // This is safe because handle indices are uint32.
    this.handleIndex = index + 1;
    return true;
  }

  Validator.prototype.validateHandle = function(offset, nullable) {
    var index = this.message.buffer.getUint32(offset);

    if (index === codec.kEncodedInvalidHandleValue)
      return nullable ?
          validationError.NONE : validationError.UNEXPECTED_INVALID_HANDLE;

    if (!this.claimHandle(index))
      return validationError.ILLEGAL_HANDLE;
    return validationError.NONE;
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
    var err = this.validateStructHeader(0, codec.kMessageHeaderSize, 2);
    if (err != validationError.NONE)
      return err;

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

  // Returns the message.buffer relative offset this pointer "points to",
  // NULL_MOJO_POINTER if the pointer represents a null, or JS null if the
  // pointer's value is not valid.
  Validator.prototype.decodePointer = function(offset) {
    var pointerValue = this.message.buffer.getUint64(offset);
    if (pointerValue === 0)
      return NULL_MOJO_POINTER;
    var bufferOffset = offset + pointerValue;
    return Number.isSafeInteger(bufferOffset) ? bufferOffset : null;
  }

  Validator.prototype.validateArrayPointer = function(
      offset, elementSize, expectedElementCount, elementType, nullable) {
    var arrayOffset = this.decodePointer(offset);
    if (arrayOffset === null)
      return validationError.ILLEGAL_POINTER;

    if (arrayOffset === NULL_MOJO_POINTER)
      return nullable ?
          validationError.NONE : validationError.UNEXPECTED_NULL_POINTER;

    return this.validateArray(
        arrayOffset, elementSize, expectedElementCount, elementType);
  }

  Validator.prototype.validateStructPointer = function(
        offset, structClass, nullable) {
    var structOffset = this.decodePointer(offset);
    if (structOffset === null)
      return validationError.ILLEGAL_POINTER;

    if (structOffset === NULL_MOJO_POINTER)
      return nullable ?
          validationError.NONE : validationError.UNEXPECTED_NULL_POINTER;

    return structClass.validate(this, structOffset);
  }

  Validator.prototype.validateStringPointer = function(offset, nullable) {
    return this.validateArrayPointer(
        offset, codec.Uint8.encodedSize, 0, codec.Uint8, nullable);
  }

  // Similar to Array_Data<T>::Validate()
  // mojo/public/cpp/bindings/lib/array_internal.h

  Validator.prototype.validateArray =
      function (offset, elementSize, expectedElementCount, elementType) {
    if (!codec.isAligned(offset))
      return validationError.MISALIGNED_OBJECT;

    if (!this.isValidRange(offset, codec.kArrayHeaderSize))
      return validationError.ILLEGAL_MEMORY_RANGE;

    var numBytes = this.message.buffer.getUint32(offset);
    var numElements = this.message.buffer.getUint32(offset + 4);

    // Note: this computation is "safe" because elementSize <= 8 and
    // numElements is a uint32.
    var elementsTotalSize = (elementType === codec.PackedBool) ?
        Math.ceil(numElements / 8) : (elementSize * numElements);

    if (numBytes < codec.kArrayHeaderSize + elementsTotalSize)
      return validationError.UNEXPECTED_ARRAY_HEADER;

    if (expectedElementCount != 0 && numElements != expectedElementCount)
      return validationError.UNEXPECTED_ARRAY_HEADER;

    if (!this.claimRange(offset, numBytes))
      return validationError.ILLEGAL_MEMORY_RANGE;

    // Validate the array's elements if they are pointers or handles.

    var elementsOffset = offset + codec.kArrayHeaderSize;
    var nullable = isNullable(elementType);

    if (isHandleClass(elementType))
      return this.validateHandleElements(elementsOffset, numElements, nullable);
    if (isStringClass(elementType))
      return this.validateArrayElements(
          elementsOffset, numElements, codec.Uint8, nullable)
    if (elementType instanceof codec.PointerTo)
      return this.validateStructElements(
          elementsOffset, numElements, elementType.cls, nullable);
    if (elementType instanceof codec.ArrayOf)
      return this.validateArrayElements(
          elementsOffset, numElements, elementType.cls, nullable);

    return validationError.NONE;
  }

  // Note: the |offset + i * elementSize| computation in the validateFooElements
  // methods below is "safe" because elementSize <= 8, offset and
  // numElements are uint32, and 0 <= i < numElements.

  Validator.prototype.validateHandleElements =
      function(offset, numElements, nullable) {
    var elementSize = codec.Handle.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateHandle(elementOffset, nullable);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  }

  // The elementClass parameter is the element type of the element arrays.
  Validator.prototype.validateArrayElements =
      function(offset, numElements, elementClass, nullable) {
    var elementSize = codec.PointerTo.prototype.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateArrayPointer(
          elementOffset, elementClass.encodedSize, 0, elementClass, nullable);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  }

  Validator.prototype.validateStructElements =
      function(offset, numElements, structClass, nullable) {
    var elementSize = codec.PointerTo.prototype.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err =
          this.validateStructPointer(elementOffset, structClass, nullable);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  }

  var exports = {};
  exports.validationError = validationError;
  exports.Validator = Validator;
  return exports;
});
