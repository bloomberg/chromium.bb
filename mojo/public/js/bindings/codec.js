// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/bindings/codec", function() {

  // Memory -------------------------------------------------------------------

  function store8(memory, pointer, val) {
    memory[pointer] = val;
  }

  function store16(memory, pointer, val) {
    memory[pointer + 0] = val >>  0;
    memory[pointer + 1] = val >>  8;
  }

  function store32(memory, pointer, val) {
    memory[pointer + 0] = val >>  0;
    memory[pointer + 1] = val >>  8;
    memory[pointer + 2] = val >> 16;
    memory[pointer + 3] = val >> 24;
  }

  function store64(memory, pointer, val) {
    store32(memory, pointer, val);
    var high = (val / 0x10000) | 0;
    store32(memory, pointer + 4, high);
  }

  function load8(memory, pointer) {
    return memory[pointer];
  }

  function load16(memory, pointer) {
    return (memory[pointer + 0] <<  0) +
           (memory[pointer + 1] <<  8);
  }

  function load32(memory, pointer) {
    return (memory[pointer + 0] <<  0) +
           (memory[pointer + 1] <<  8) +
           (memory[pointer + 2] << 16) +
           (memory[pointer + 3] << 24);
  }

  var kAlignment = 8;

  function align(size) {
    return size + (kAlignment - (size % kAlignment)) % kAlignment;
  }

  // Buffer -------------------------------------------------------------------

  function Buffer(size) {
    this.memory = new Uint8Array(size);
    this.next = 0;
  }

  Buffer.prototype.alloc = function(size) {
    var pointer = this.next;
    this.next += size;
    if (this.next > this.memory.length) {
      var newSize = (1.5 * (this.memory.length + size)) | 0;
      this.grow(newSize);
    }
    return pointer;
  };

  Buffer.prototype.grow = function(size) {
    var newMemory = new Uint8Array(size);
    var oldMemory = this.memory;
    for (var i = 0; i < oldMemory.length; ++i)
      newMemory[i] = oldMemory[i];
    this.memory = newMemory;
  };

  Buffer.prototype.createViewOfAllocatedMemory = function() {
    return new Uint8Array(this.memory.buffer, 0, this.next);
  };

  // Constants ----------------------------------------------------------------

  var kArrayHeaderSize = 8;
  var kStructHeaderSize = 8;
  var kMessageHeaderSize = 16;
  var kMessageWithRequestIDHeaderSize = 24;

  // Decoder ------------------------------------------------------------------

  function Decoder(memory, handles, base) {
    this.memory = memory;
    this.handles = handles;
    this.base = base;
    this.next = base;
    this.viewU32 = new Uint32Array(
        this.memory.buffer, 0,
        Math.floor(this.memory.length / Uint32Array.BYTES_PER_ELEMENT));
    this.viewFloat = new Float32Array(
        this.memory.buffer, 0,
        Math.floor(this.memory.length / Float32Array.BYTES_PER_ELEMENT));
  }

  Decoder.prototype.skip = function(offset) {
    this.next += offset;
  };

  Decoder.prototype.read8 = function() {
    var result = load8(this.memory, this.next);
    this.next += 1;
    return result;
  };

  Decoder.prototype.read16 = function() {
    var result = load16(this.memory, this.next);
    this.next += 2;
    return result;
  };

  Decoder.prototype.read32 = function() {
    var result = this.viewU32[this.next / this.viewU32.BYTES_PER_ELEMENT];
    this.next += this.viewU32.BYTES_PER_ELEMENT;
    return result;
  };

  Decoder.prototype.read64 = function() {
    var low = this.read32();
    var high = this.read32();
    return low + high * 0x100000000;
  };

  Decoder.prototype.decodeFloat = function() {
    var result = this.viewFloat[this.next / this.viewFloat.BYTES_PER_ELEMENT];
    this.next += this.viewFloat.BYTES_PER_ELEMENT;
    return result;
  };

  Decoder.prototype.decodePointer = function() {
    // TODO(abarth): To correctly decode a pointer, we need to know the real
    // base address of the array buffer.
    var offsetPointer = this.next;
    var offset = this.read64();
    if (!offset)
      return 0;
    return offsetPointer + offset;
  };

  Decoder.prototype.decodeAndCreateDecoder = function() {
    return new Decoder(this.memory, this.handles, this.decodePointer());
  };

  Decoder.prototype.decodeHandle = function() {
    return this.handles[this.read32()];
  };

  Decoder.prototype.decodeString = function() {
    // TODO(abarth): We should really support UTF-8. We might want to
    // jump out of the VM to decode the string directly from the array
    // buffer using v8::String::NewFromUtf8.
    var numberOfBytes = this.read32();
    var numberOfElements = this.read32();
    var val = new Array(numberOfElements);
    var memory = this.memory;
    var base = this.next;
    for (var i = 0; i < numberOfElements; ++i) {
      val[i] = String.fromCharCode(memory[base + i] & 0x7F);
    }
    this.next += numberOfElements;
    return val.join('');
  };

  Decoder.prototype.decodeArray = function(cls) {
    var numberOfBytes = this.read32();
    var numberOfElements = this.read32();
    var val = new Array(numberOfElements);
    for (var i = 0; i < numberOfElements; ++i) {
      val[i] = cls.decode(this);
    }
    return val;
  };

  Decoder.prototype.decodeStructPointer = function(cls) {
    return cls.decode(this.decodeAndCreateDecoder());
  };

  Decoder.prototype.decodeArrayPointer = function(cls) {
    return this.decodeAndCreateDecoder().decodeArray(cls);
  };

  Decoder.prototype.decodeStringPointer = function() {
    return this.decodeAndCreateDecoder().decodeString();
  };

  // Encoder ------------------------------------------------------------------

  function Encoder(buffer, handles, base) {
    this.buffer = buffer;
    this.handles = handles;
    this.base = base;
    this.next = base;
  }

  Encoder.prototype.skip = function(offset) {
    this.next += offset;
  };

  Encoder.prototype.write8 = function(val) {
    store8(this.buffer.memory, this.next, val);
    this.next += 1;
  };

  Encoder.prototype.write16 = function(val) {
    store16(this.buffer.memory, this.next, val);
    this.next += 2;
  };

  Encoder.prototype.write32 = function(val) {
    store32(this.buffer.memory, this.next, val);
    this.next += 4;
  };

  Encoder.prototype.write64 = function(val) {
    store64(this.buffer.memory, this.next, val);
    this.next += 8;
  };

  Encoder.prototype.encodeFloat = function(val) {
    var floatBuffer = new Float32Array(1);
    floatBuffer[0] = val;
    var buffer = new Uint8Array(floatBuffer.buffer, 0);
    for (var i = 0; i < buffer.length; ++i)
      this.buffer.memory[this.next++] = buffer[i];
  };

  Encoder.prototype.encodePointer = function(pointer) {
    if (!pointer)
      return this.write64(0);
    // TODO(abarth): To correctly encode a pointer, we need to know the real
    // base address of the array buffer.
    var offset = pointer - this.next;
    this.write64(offset);
  };

  Encoder.prototype.createAndEncodeEncoder = function(size) {
    var pointer = this.buffer.alloc(align(size));
    this.encodePointer(pointer);
    return new Encoder(this.buffer, this.handles, pointer);
  };

  Encoder.prototype.encodeHandle = function(handle) {
    this.handles.push(handle);
    this.write32(this.handles.length - 1);
  };

  Encoder.prototype.encodeString = function(val) {
    var numberOfElements = val.length;
    var numberOfBytes = kArrayHeaderSize + numberOfElements;
    this.write32(numberOfBytes);
    this.write32(numberOfElements);
    // TODO(abarth): We should really support UTF-8. We might want to
    // jump out of the VM to encode the string directly from the array
    // buffer using v8::String::WriteUtf8.
    var memory = this.buffer.memory;
    var base = this.next;
    var len = val.length;
    for (var i = 0; i < len; ++i) {
      memory[base + i] = val.charCodeAt(i) & 0x7F;
    }
    this.next += len;
  };

  Encoder.prototype.encodeArray = function(cls, val) {
    var numberOfElements = val.length;
    var numberOfBytes = kArrayHeaderSize + cls.encodedSize * numberOfElements;
    this.write32(numberOfBytes);
    this.write32(numberOfElements);
    for (var i = 0; i < numberOfElements; ++i) {
      cls.encode(this, val[i]);
    }
  };

  Encoder.prototype.encodeStructPointer = function(cls, val) {
    var encoder = this.createAndEncodeEncoder(cls.encodedSize);
    cls.encode(encoder, val);
  };

  Encoder.prototype.encodeArrayPointer = function(cls, val) {
    var encodedSize = kArrayHeaderSize + cls.encodedSize * val.length;
    var encoder = this.createAndEncodeEncoder(encodedSize);
    encoder.encodeArray(cls, val);
  };

  Encoder.prototype.encodeStringPointer = function(val) {
    // TODO(abarth): This won't be right once we support UTF-8.
    var encodedSize = kArrayHeaderSize + val.length;
    var encoder = this.createAndEncodeEncoder(encodedSize);
    encoder.encodeString(val);
  };

  // Message ------------------------------------------------------------------

  var kMessageExpectsResponse = 1 << 0;
  var kMessageIsResponse      = 1 << 1;

  function Message(memory, handles) {
    this.memory = memory;
    this.handles = handles;
  }

  Message.prototype.setRequestID = function(requestID) {
    // TODO(darin): Verify that space was reserved for this field!
    store64(this.memory, 4 + 4 + 4 + 4, requestID);
  };

  Message.prototype.getFlags = function() {
    // Skip over num_bytes, num_fields, and message_name.
    return load32(this.memory, 4 + 4 + 4);
  };

  // MessageBuilder -----------------------------------------------------------

  function MessageBuilder(messageName, payloadSize) {
    // Currently, we don't compute the payload size correctly ahead of time.
    // Instead, we resize the buffer at the end.
    var numberOfBytes = kMessageHeaderSize + payloadSize;
    this.buffer = new Buffer(numberOfBytes);
    this.handles = [];
    var encoder = this.createEncoder(kMessageHeaderSize);
    encoder.write32(kMessageHeaderSize);
    encoder.write32(2);  // num_fields.
    encoder.write32(messageName);
    encoder.write32(0);  // flags.
  }

  MessageBuilder.prototype.createEncoder = function(size) {
    var pointer = this.buffer.alloc(size);
    return new Encoder(this.buffer, this.handles, pointer);
  };

  MessageBuilder.prototype.encodeStruct = function(cls, val) {
    cls.encode(this.createEncoder(cls.encodedSize), val);
  };

  MessageBuilder.prototype.finish = function() {
    // TODO(abarth): Rather than resizing the buffer at the end, we could
    // compute the size we need ahead of time, like we do in C++.
    var memory = this.buffer.createViewOfAllocatedMemory();
    var message = new Message(memory, this.handles);
    this.buffer = null;
    this.handles = null;
    this.encoder = null;
    return message;
  };

  // MessageWithRequestIDBuilder -----------------------------------------------

  function MessageWithRequestIDBuilder(messageName, payloadSize, flags,
                                       requestID) {
    // Currently, we don't compute the payload size correctly ahead of time.
    // Instead, we resize the buffer at the end.
    var numberOfBytes = kMessageWithRequestIDHeaderSize + payloadSize;
    this.buffer = new Buffer(numberOfBytes);
    this.handles = [];
    var encoder = this.createEncoder(kMessageWithRequestIDHeaderSize);
    encoder.write32(kMessageWithRequestIDHeaderSize);
    encoder.write32(3);  // num_fields.
    encoder.write32(messageName);
    encoder.write32(flags);
    encoder.write64(requestID);
  }

  MessageWithRequestIDBuilder.prototype =
      Object.create(MessageBuilder.prototype);
  MessageWithRequestIDBuilder.prototype.constructor =
      MessageWithRequestIDBuilder;

  // MessageReader ------------------------------------------------------------

  function MessageReader(message) {
    this.decoder = new Decoder(message.memory, message.handles, 0);
    var messageHeaderSize = this.decoder.read32();
    this.payloadSize = message.memory.length - messageHeaderSize;
    var numFields = this.decoder.read32();
    this.messageName = this.decoder.read32();
    this.flags = this.decoder.read32();
    if (numFields >= 3)
      this.requestID = this.decoder.read64();
    this.decoder.skip(messageHeaderSize - this.decoder.next);
  }

  MessageReader.prototype.decodeStruct = function(cls) {
    return cls.decode(this.decoder);
  };

  // Built-in types -----------------------------------------------------------

  function Uint8() {
  }

  Uint8.encodedSize = 1;

  Uint8.decode = function(decoder) {
    return decoder.read8();
  };

  Uint8.encode = function(encoder, val) {
    encoder.write8(val);
  };

  function Uint16() {
  }

  Uint16.encodedSize = 2;

  Uint16.decode = function(decoder) {
    return decoder.read16();
  };

  Uint16.encode = function(encoder, val) {
    encoder.write16(val);
  };

  function Uint32() {
  }

  Uint32.encodedSize = 4;

  Uint32.decode = function(decoder) {
    return decoder.read32();
  };

  Uint32.encode = function(encoder, val) {
    encoder.write32(val);
  };

  function Uint64() {
  };

  Uint64.encodedSize = 8;

  Uint64.decode = function(decoder) {
    return decoder.read64();
  };

  Uint64.encode = function(encoder, val) {
    encoder.write64(val);
  };

  function PointerTo(cls) {
    this.cls = cls;
  };

  // TODO(abarth): Add missing types:
  // * String
  // * Float
  // * Double
  // * Signed integers

  PointerTo.prototype.encodedSize = 8;

  PointerTo.prototype.decode = function(decoder) {
    return this.cls.decode(decoder.decodeAndCreateDecoder());
  };

  PointerTo.prototype.encode = function(encoder, val) {
    var objectEncoder = encoder.createAndEncodeEncoder(this.cls.encodedSize);
    this.cls.encode(objectEncoder, val);
  };

  function ArrayOf(cls) {
    this.cls = cls;
  };

  ArrayOf.prototype.encodedSize = 8;

  ArrayOf.prototype.decode = function(decoder) {
    return decoder.decodeArrayPointer(self.cls);
  };

  ArrayOf.prototype.encode = function(encoder, val) {
    encoder.encodeArrayPointer(self.cls, val);
  };

  function Handle() {
  }

  Handle.encodedSize = 4;

  Handle.decode = function(decoder) {
    return decoder.decodeHandle();
  };

  Handle.encode = function(encoder, val) {
    encoder.encodeHandle(val);
  };

  var exports = {};
  exports.align = align;
  exports.Message = Message;
  exports.MessageBuilder = MessageBuilder;
  exports.MessageWithRequestIDBuilder = MessageWithRequestIDBuilder;
  exports.MessageReader = MessageReader;
  exports.kArrayHeaderSize = kArrayHeaderSize;
  exports.kStructHeaderSize = kStructHeaderSize;
  exports.kMessageHeaderSize = kMessageHeaderSize;
  exports.kMessageExpectsResponse = kMessageExpectsResponse;
  exports.kMessageIsResponse = kMessageIsResponse;
  exports.Uint8 = Uint8;
  exports.Uint16 = Uint16;
  exports.Uint32 = Uint32;
  exports.Uint64 = Uint64;
  exports.PointerTo = PointerTo;
  exports.ArrayOf = ArrayOf;
  exports.Handle = Handle;
  return exports;
});
