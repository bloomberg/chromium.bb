// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(zra): Rewrite MojoDecoder and MojoEncoder using Dart idioms, and make
// corresponding changes to the bindings generation script.

part of bindings;

const int kAlignment = 8;
const int kArrayHeaderSize = 8;
const int kStructHeaderSize = 8;
const int kMessageHeaderSize = 16;
const int kMessageWithRequestIDHeaderSize = 24;
const int kMapStructPayloadSize = 16;
const int kStructHeaderNumBytesOffset = 0;
const int kStructHeaderNumFieldsOffset = 4;
const int kEncodedInvalidHandleValue = 0xffffffff;
const String kErrorUnsigned = "Passing negative value to unsigned encoder";


int align(int size) => size + (kAlignment - (size % kAlignment)) % kAlignment;
bool isAligned(offset) => (offset >= 0) && ((offset % kAlignment) == 0);


Uint8List utf8OfString(String s) =>
  (new Uint8List.fromList((const Utf8Encoder()).convert(s)));


String stringOfUtf8(Uint8List bytes) =>
    (const Utf8Decoder()).convert(bytes.toList());


// Given an argument that is either a Type or an instance:
// Invoke the static method "decode" of the Type, or the instance method
// "decode" of the instance, on the MojoDecoder.
Object _callDecode(Object typeOrInstance, MojoDecoder decoder) {
  if (typeOrInstance is Type) {
    return reflectClass(typeOrInstance).invoke(#decode, [decoder]).reflectee;
  } else {
    return typeOrInstance.decode(decoder);
  }
}


// Given an argument that is either a Type or an instance:
// Invoke the static method "encode" of the Type, or the instance method
// "encode" of the instance, on the MojoEncoder and value to be encoded.
void _callEncode(Object typeOrInstance, MojoEncoder encoder, Object val) {
  if (typeOrInstance is Type) {
    reflectClass(typeOrInstance).invoke(#encode, [encoder, val]);
  } else {
    typeOrInstance.encode(encoder, val);
  }
}


// Given an argument that is either a Type or an instance:
// Invoke the static getter "encodedSize" of the Type, or the instance getter
// "encodedSize" of the instance, and return the result.
int getEncodedSize(Object typeOrInstance) {
  if (typeOrInstance is Type) {
    return reflectClass(typeOrInstance).getField(#encodedSize).reflectee;
  } else {
    return typeOrInstance.encodedSize;
  }
}


class MojoDecoder {
  ByteData buffer;
  List<core.RawMojoHandle> handles;
  int base;
  int next;

  MojoDecoder(this.buffer, this.handles, this.base) {
    next = base;
  }

  void skip(int offset) {
    next += offset;
  }

  int readInt8() {
    int result = buffer.getInt8(next);
    next += 1;
    return result;
  }

  int readUint8() {
    int result = buffer.getUint8(next);
    next += 1;
    return result;
  }

  int readInt16() {
    int result = buffer.getInt16(next, Endianness.LITTLE_ENDIAN);
    next += 2;
    return result;
  }

  int readUint16() {
    int result = buffer.getUint16(next, Endianness.LITTLE_ENDIAN);
    next += 2;
    return result;
  }

  int readInt32() {
    int result = buffer.getInt32(next, Endianness.LITTLE_ENDIAN);
    next += 4;
    return result;
  }

  int readUint32() {
    int result = buffer.getUint32(next, Endianness.LITTLE_ENDIAN);
    next += 4;
    return result;
  }

  int readInt64() {
    int result = buffer.getInt64(next, Endianness.LITTLE_ENDIAN);
    next += 8;
    return result;
  }

  int readUint64() {
    int result = buffer.getUint64(next, Endianness.LITTLE_ENDIAN);
    next += 8;
    return result;
  }

  double readFloat() {
    double result = buffer.getFloat32(next,Endianness.LITTLE_ENDIAN);
    next += 4;
    return result;
  }

  double readDouble() {
    double result = buffer.getFloat64(next, Endianness.LITTLE_ENDIAN);
    next += 8;
    return result;
  }

  int decodePointer() {
    int offsetPointer = next;
    int offset = readUint64();
    if (offset == 0) {
      return 0;
    }
    return offsetPointer + offset;
  }

  MojoDecoder decodeAndCreateDecoder(int offset) {
    return new MojoDecoder(buffer, handles, offset);
  }

  core.RawMojoHandle decodeHandle() {
    return handles[readUint32()];
  }

  String decodeString() {
    int numBytes = readUint32();
    int numElements = readUint32();
    int base = next;
    next += numElements;
    return stringOfUtf8(buffer.buffer.asUint8List(base, numElements));
  }

  List decodeArray(Object type) {
    int numBytes = readUint32();
    int numElements = readUint32();
    if (type == PackedBool) {
      int b;
      List<bool> result = new List<bool>(numElements);
      for (int i = 0; i < numElements; i++) {
        if ((i % 8) == 0) {
          b = readUint8();
        }
        result[i] = ((b & (1 << (i % 8)) != 0) ? true : false);
      }
      return result;
    } else {
      List result = new List(numElements);
      for (int i = 0; i < numElements; i++) {
        result[i] = _callDecode(type, this);
      }
      return result;
    }
  }

  Object decodeStruct(Object t) {
    return _callDecode(t, this);
  }

  Object decodeStructPointer(Object t) {
    int pointer = decodePointer();
    if (pointer == 0) {
      return null;
    }
    return _callDecode(t, decodeAndCreateDecoder(pointer));
  }

  List decodeArrayPointer(Object type) {
    int pointer = decodePointer();
    if (pointer == 0) {
      return null;
    }
    return decodeAndCreateDecoder(pointer).decodeArray(type);
  }

  String decodeStringPointer() {
    int pointer = decodePointer();
    if (pointer == 0) {
      return null;
    }
    return decodeAndCreateDecoder(pointer).decodeString();
  }

  Map decodeMap(Object keyType, Object valType) {
    skip(4);  // number of bytes.
    skip(4);  // number of fields.
    List keys = decodeArrayPointer(keyType);
    List values = decodeArrayPointer(valType);
    return new Map.fromIterables(keys, values);
  }

  Map decodeMapPointer(Object keyType, Object valType) {
    int pointer = this.decodePointer();
    if (pointer == 0) {
      return null;
    }
    MojoDecoder decoder = decodeAndCreateDecoder(pointer);
    return decoder.decodeMap(keyType, valType);
  }
}


class MojoEncoder {
  ByteData buffer;
  List<core.RawMojoHandle> handles;
  int base;
  int next;
  int extent;

  MojoEncoder(this.buffer, this.handles, this.base, this.extent) {
    next = base;
  }

  void skip(int offset) {
    next += offset;
  }

  void writeInt8(int val) {
    buffer.setInt8(next, val);
    next += 1;
  }

  void writeUint8(int val) {
    if (val < 0) {
      throw new ArgumentError("$kErrorUnsigned: $val");
    }
    buffer.setUint8(next, val);
    next += 1;
  }

  void writeInt16(int val) {
    buffer.setInt16(next, val, Endianness.LITTLE_ENDIAN);
    next += 2;
  }

  void writeUint16(int val) {
    if (val < 0) {
      throw new ArgumentError("$kErrorUnsigned: $val");
    }
    buffer.setUint16(next, val, Endianness.LITTLE_ENDIAN);
    next += 2;
  }

  void writeInt32(int val) {
    buffer.setInt32(next, val, Endianness.LITTLE_ENDIAN);
    next += 4;
  }

  void writeUint32(int val) {
    if (val < 0) {
      throw new ArgumentError("$kErrorUnsigned: $val");
    }
    buffer.setUint32(next, val, Endianness.LITTLE_ENDIAN);
    next += 4;
  }

  void writeInt64(int val) {
    buffer.setInt64(next, val, Endianness.LITTLE_ENDIAN);
    next += 8;
  }

  void writeUint64(int val) {
    if (val < 0) {
      throw new ArgumentError("$kErrorUnsigned: $val");
    }
    buffer.setUint64(next, val, Endianness.LITTLE_ENDIAN);
    next += 8;
  }

  void writeFloat(double val) {
    buffer.setFloat32(next, val, Endianness.LITTLE_ENDIAN);
    next += 4;
  }

  void writeDouble(double val) {
    buffer.setFloat64(next, val, Endianness.LITTLE_ENDIAN);
    next += 8;
  }

  void encodePointer(int pointer) {
    if (pointer == null) {
      writeUint64(0);
      return;
    }
    int offset = pointer - next;
    writeUint64(offset);
  }

  void grow(int new_size) {
    Uint8List new_buffer = new Uint8List(new_size);
    new_buffer.setRange(0, next, buffer.buffer.asUint8List());
    buffer = new_buffer.buffer.asByteData();
  }

  int alloc(int size_request) {
    int pointer = extent;
    extent += size_request;
    if (extent > buffer.lengthInBytes) {
      int new_size = buffer.lengthInBytes + size_request;
      new_size += new_size ~/ 2;
      grow(new_size);
    }
    return pointer;
  }

  MojoEncoder createAndEncodeEncoder(int size) {
    int pointer = alloc(align(size));
    encodePointer(pointer);
    return new MojoEncoder(buffer, handles, pointer, extent);
  }

  void encodeHandle(core.RawMojoHandle handle) {
    handles.add(handle);
    writeUint32(handles.length - 1);
  }

  void encodeString(String val) {
    Uint8List utf8string = utf8OfString(val);
    int numElements = utf8string.lengthInBytes;
    int numBytes = kArrayHeaderSize + numElements;
    writeUint32(numBytes);
    writeUint32(numElements);
    buffer.buffer.asUint8List().setRange(next, next + numElements, utf8string);
    next += numElements;
  }

  void encodeArray(Object t, List val, [int numElements, int encodedSize]) {
    if (numElements == null) {
      numElements = val.length;
    }
    if (encodedSize == null) {
      encodedSize = kArrayHeaderSize + (getEncodedSize(t) * numElements);
    }

    writeUint32(encodedSize);
    writeUint32(numElements);

    if (t == PackedBool) {
      int b = 0;
      for (int i = 0; i < numElements; i++) {
        if (val[i]) {
          b |= (1 << (i % 8));
        }
        if (((i % 8) == 7) || (i == (numElements - 1))) {
          Uint8.encode(this, b);
        }
      }
    } else {
      for (int i = 0; i < numElements; i++) {
        _callEncode(t, this, val[i]);
      }
    }
  }

  void encodeStruct(Object t, Object val) {
    _callEncode(t, this, val);
  }

  void encodeStructPointer(Object t, Object val) {
    if (val == null) {
      encodePointer(val);
      return;
    }
    MojoEncoder encoder = createAndEncodeEncoder(getEncodedSize(t));
    _callEncode(t, encoder, val);
    extent = encoder.extent;
    buffer = encoder.buffer;
  }

  void encodeArrayPointer(Object t, List val) {
    if (val == null) {
      encodePointer(val);
      return;
    }
    int numElements = val.length;
    int encodedSize = kArrayHeaderSize + ((t == PackedBool) ?
        (numElements / 8).ceil() : (getEncodedSize(t) * numElements));
    MojoEncoder encoder = createAndEncodeEncoder(encodedSize);
    encoder.encodeArray(t, val, numElements, encodedSize);
    extent = encoder.extent;
    buffer = encoder.buffer;
  }

  void encodeStringPointer(String val) {
    if (val == null) {
      encodePointer(val);
      return;
    }
    int encodedSize = kArrayHeaderSize + utf8OfString(val).lengthInBytes;
    MojoEncoder encoder = createAndEncodeEncoder(encodedSize);
    encoder.encodeString(val);
    extent = encoder.extent;
    buffer = encoder.buffer;
  }

  void encodeMap(Object keyType, Object valType, Map val) {
    List keys = val.keys;
    List vals = val.values;
    writeUint32(kStructHeaderSize + kMapStructPayloadSize);
    writeUint32(2);
    encodeArrayPointer(keyType, keys);
    encodeArrayPointer(valType, vals);
  }

  void encodeMapPointer(Object keyTYpe, Object valType, Map val) {
    if (val == null) {
      encodePointer(val);
      return;
    }
    int encodedSize = kStructHeaderSize + kMapStructPayloadSize;
    MojoEncoder encoder = createAndEncodeEncoder(encodedSize);
    encoder.encodeMap(keyType, valType, val);
    extent = encoder.extent;
    buffer = encoder.buffer;
  }
}


const int kMessageNameOffset = kStructHeaderSize;
const int kMessageFlagsOffset = kMessageNameOffset + 4;
const int kMessageRequestIDOffset = kMessageFlagsOffset + 4;
const int kMessageExpectsResponse = 1 << 0;
const int kMessageIsResponse = 1 << 1;

class Message {
  ByteData buffer;
  List<core.RawMojoHandle> handles;

  Message(this.buffer, this.handles);

  int getHeaderNumBytes() => buffer.getUint32(kStructHeaderNumBytesOffset);
  int getHeaderNumFields() => buffer.getUint32(kStructHeaderNumFieldsOffset);
  int getName() => buffer.getUint32(kMessageNameOffset);
  int getFlags() => buffer.getUint32(kMessageFlagsOffset);
  bool isResponse() => (getFlags() & kMessageIsResponse) != 0;
  bool expectsResponse() => (getFlags() & kMessageExpectsResponse) != 0;

  void setRequestID(int id) {
    buffer.setUint64(kMessageRequestIDOffset, id);
  }
}


class MessageBuilder {
  MojoEncoder encoder;
  List<core.RawMojoHandle> handles;

  MessageBuilder._();

  MessageBuilder(int name, int payloadSize) {
    int numBytes = kMessageHeaderSize + payloadSize;
    var buffer = new ByteData(numBytes);
    handles = <core.RawMojoHandle>[];

    encoder = new MojoEncoder(buffer, handles, 0, kMessageHeaderSize);
    encoder.writeUint32(kMessageHeaderSize);
    encoder.writeUint32(2);  // num_fields;
    encoder.writeUint32(name);
    encoder.writeUint32(0);  // flags.
  }

  MojoEncoder createEncoder(int size) {
    encoder = new MojoEncoder(encoder.buffer,
                              handles,
                              encoder.next,
                              encoder.next + size);
    return encoder;
  }

  void encodeStruct(Object t, Object val) {
    encoder = createEncoder(getEncodedSize(t));
    _callEncode(t, encoder, val);
  }

  ByteData _trimBuffer() {
    return new ByteData.view(encoder.buffer.buffer, 0, encoder.extent);
  }

  Message finish() {
    Message message = new Message(_trimBuffer(), handles);
    encoder = null;
    handles = null;
    return message;
  }
}


class MessageWithRequestIDBuilder extends MessageBuilder {
  MessageWithRequestIDBuilder(
      int name, int payloadSize, int requestID, [int flags = 0])
      : super._() {
    int numBytes = kMessageWithRequestIDHeaderSize + payloadSize;
    var buffer = new ByteData(numBytes);
    handles = <core.RawMojoHandle>[];

    encoder = new MojoEncoder(
        buffer, handles, 0, kMessageWithRequestIDHeaderSize);
    encoder.writeUint32(kMessageWithRequestIDHeaderSize);
    encoder.writeUint32(3);  // num_fields.
    encoder.writeUint32(name);
    encoder.writeUint32(flags);
    encoder.writeUint64(requestID);
  }
}


class MessageReader {
  MojoDecoder decoder;
  int payloadSize;
  int name;
  int flags;
  int requestID;

  MessageReader(Message message) {
    decoder = new MojoDecoder(message.buffer, message.handles, 0);

    int messageHeaderSize = decoder.readUint32();
    payloadSize = message.buffer.lengthInBytes - messageHeaderSize;

    int num_fields = decoder.readUint32();
    name = decoder.readUint32();
    flags = decoder.readUint32();

    if (num_fields >= 3) {
      requestID = decoder.readUint64();
    }
    decoder.skip(messageHeaderSize - decoder.next);
  }

  Object decodeStruct(Object t) => _callDecode(t, decoder);
}


class PackedBool {}


class Int8 {
  static const int encodedSize = 1;
  static int decode(MojoDecoder decoder) => decoder.readInt8();
  static void encode(MojoEncoder encoder, int val) {
    encoder.writeInt8(val);
  }
}


class Uint8 {
  static const int encodedSize = 1;
  static int decode(MojoDecoder decoder) => decoder.readUint8();
  static void encode(MojoEncoder encoder, int val) {
    encoder.writeUint8(val);
  }
}


class Int16 {
  static const int encodedSize = 2;
  static int decode(MojoDecoder decoder) => decoder.readInt16();
  static void encode(MojoEncoder encoder, int val) {
    encoder.writeInt16(val);
  }
}


class Uint16 {
  static const int encodedSize = 2;
  static int decode(MojoDecoder decoder) => decoder.readUint16();
  static void encode(MojoEncoder encoder, int val) {
    encoder.writeUint16(val);
  }
}


class Int32 {
  static const int encodedSize = 4;
  static int decode(MojoDecoder decoder) => decoder.readInt32();
  static void encode(MojoEncoder encoder, int val) {
    encoder.writeInt32(val);
  }
}


class Uint32 {
  static const int encodedSize = 4;
  static int decode(MojoDecoder decoder) => decoder.readUint32();
  static void encode(MojoEncoder encoder, int val) {
    encoder.writeUint32(val);
  }
}


class Int64 {
  static const int encodedSize = 8;
  static int decode(MojoDecoder decoder) => decoder.readInt64();
  static void encode(MojoEncoder encoder, int val) {
    encoder.writeInt64(val);
  }
}


class Uint64 {
  static const int encodedSize = 8;
  static int decode(MojoDecoder decoder) => decoder.readUint64();
  static void encode(MojoEncoder encoder, int val) {
    encoder.writeUint64(val);
  }
}


class MojoString {
  static const int encodedSize = 8;
  static String decode(MojoDecoder decoder) => decoder.decodeStringPointer();
  static void encode(MojoEncoder encoder, String val) {
    encoder.encodeStringPointer(val);
  }
}


class NullableMojoString {
  static const int encodedSize = MojoString.encodedSize;
  static var decode = MojoString.decode;
  static var encode = MojoString.encode;
}


class Float {
  static const int encodedSize = 4;
  static double decode(MojoDecoder decoder) => decoder.readFloat();
  static void encode(MojoEncoder encoder, double val) {
    encoder.writeFloat(val);
  }
}


class Double {
  static const int encodedSize = 8;
  static double decode(MojoDecoder decoder) => decoder.readDouble();
  static void encode(MojoEncoder encoder, double val) {
    encoder.writeDouble(val);
  }
}


class PointerTo {
  Object val;

  PointerTo(this.val);

  int encodedSize = 8;
  Object decode(MojoDecoder decoder) {
    int pointer = decoder.decodePointer();
    if (pointer == 0) {
      return null;
    }
    return _callDecode(val, decoder.decodeAndCreateDecoder(pointer));
  }
  void encode(MojoEncoder encoder, Object val) {
    if (val == null) {
      encoder.encodePointer(val);
      return;
    }
    MojoEncoder obj_encoder =
        encoder.createAndEncodeEncoder(getEncodedSize(this.val));
    _callEncode(this.val, obj_encoder, val);
  }
}


class NullablePointerTo extends PointerTo {
  NullablePointerTo(Object val) : super(val);
}


class ArrayOf {
  Object val;
  int length;

  ArrayOf(this.val, [this.length = 0]);

  int dimensions() => [length].addAll((val is ArrayOf) ? val.dimensions() : []);

  int encodedSize = 8;
  List decode(MojoDecoder decoder) => decoder.decodeArrayPointer(val);
  void encode(MojoEncoder encoder, List val) {
    encoder.encodeArrayPointer(this.val, val);
  }
}


class NullableArrayOf extends ArrayOf {
  NullableArrayOf(Object val, [int length = 0]) : super(val, length);
}


class Handle {
  static const int encodedSize = 4;
  static core.RawMojoHandle decode(MojoDecoder decoder) =>
      decoder.decodeHandle();
  static void encode(MojoEncoder encoder, core.RawMojoHandle val) {
    encoder.encodeHandle(val);
  }
}


class NullableHandle {
  static const int encodedSize = Handle.encodedSize;
  static const decode = Handle.decode;
  static const encode = Handle.encode;
}
