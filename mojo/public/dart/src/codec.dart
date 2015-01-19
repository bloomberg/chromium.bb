// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of bindings;

int align(int size) => size + (kAlignment - (size % kAlignment)) % kAlignment;

const int kAlignment = 8;
const int kSerializedHandleSize = 4;
const int kPointerSize = 8;
const DataHeader kMapStructHeader = const DataHeader(24, 2);
const int kUnspecifiedArrayLength = -1;
const int kNothingNullable = 0;
const int kArrayNullable = (1 << 0);
const int kElementNullable = (1 << 1);

bool isArrayNullable(int nullability) => (nullability & kArrayNullable) > 0;
bool isElementNullable(int nullability) => (nullability & kElementNullable) > 0;

class _EncoderBuffer {
  ByteData buffer;
  List<core.MojoHandle> handles;
  int extent;

  static const int kInitialBufferSize = 1024;

  _EncoderBuffer([int size = -1]) :
      buffer = new ByteData(size > 0 ? size : kInitialBufferSize),
      handles = [],
      extent = 0;

  void _grow(int newSize) {
    Uint8List newBuffer = new Uint8List(newSize);
    newBuffer.setRange(0, buffer.lengthInBytes, buffer.buffer.asUint8List());
    buffer = newBuffer.buffer.asByteData();
  }

  void claimMemory(int claimSize) {
    extent += claimSize;
    if (extent > buffer.lengthInBytes) {
      int newSize = buffer.lengthInBytes + claimSize;
      newSize += newSize ~/ 2;
      _grow(newSize);
    }
  }

  ByteData get trimmed => new ByteData.view(buffer.buffer, 0, extent);
}

class Encoder {
  _EncoderBuffer _buffer;
  int _base;

  Encoder([int size = -1]) : _buffer = new _EncoderBuffer(size), _base = 0;

  Encoder._fromBuffer(_EncoderBuffer buffer) :
      _buffer = buffer,
      _base = buffer.extent; 

  Encoder getEncoderAtOffset(DataHeader dataHeader) {
    var result = new Encoder._fromBuffer(_buffer);
    result.encodeDataHeader(dataHeader);
    return result;
  }

  Message get message => new Message(_buffer.trimmed, _buffer.handles);

  void encodeDataHeader(DataHeader dataHeader) {
    _buffer.claimMemory(align(dataHeader.size));
    encodeUint32(dataHeader.size, DataHeader.kSizeOffset);
    encodeUint32(dataHeader.numFields, DataHeader.kNumFieldsOffset);
  }

  static const String kErrorUnsigned =
      'Passing negative value to unsigned encoder';

  void encodeBool(bool value, int offset, int bit) {
    if (value) {
      int encodedValue = _buffer.buffer.getUint8(_base + offset);
      encodedValue |= (1 << bit);
      _buffer.buffer.setUint8(_base + offset, encodedValue);
    }
  }

  void encodeInt8(int value, int offset) =>
      _buffer.buffer.setInt8(_base + offset, value);

  void encodeUint8(int value, int offset) {
    if (value < 0) {
      throw '$kErrorUnsigned: $val';
    }
    _buffer.buffer.setUint8(_base + offset, value);
  }

  void encodeInt16(int value, int offset) =>
      _buffer.buffer.setInt16(_base + offset, value, Endianness.LITTLE_ENDIAN);

  void encodeUint16(int value, int offset) {
    if (value < 0) {
      throw '$kErrorUnsigned: $val';
    }
    _buffer.buffer.setUint16(_base + offset, value, Endianness.LITTLE_ENDIAN);
  }

  void encodeInt32(int value, int offset) =>
      _buffer.buffer.setInt32(_base + offset, value, Endianness.LITTLE_ENDIAN);

  void encodeUint32(int value, int offset) {
    if (value < 0) {
      throw '$kErrorUnsigned: $val';
    }
    _buffer.buffer.setUint32(_base + offset, value, Endianness.LITTLE_ENDIAN);
  }

  void encodeInt64(int value, int offset) =>
      _buffer.buffer.setInt64(_base + offset, value, Endianness.LITTLE_ENDIAN);

  void encodeUint64(int value, int offset) {
    if (value < 0) {
      throw '$kErrorUnsigned: $val';
    }
    _buffer.buffer.setUint64(_base + offset, value, Endianness.LITTLE_ENDIAN);
  }

  void encodeFloat(double value, int offset) =>
    _buffer.buffer.setFloat32(_base + offset, value, Endianness.LITTLE_ENDIAN);

  void encodeDouble(double value, int offset) =>
    _buffer.buffer.setFloat64(_base + offset, value, Endianness.LITTLE_ENDIAN);

  void encodeHandle(core.MojoHandle value, int offset, bool nullable) {
    if ((value == null) || !value.isValid) {
      encodeInvalideHandle(offset, nullable);
    } else {
      encodeUint32(_buffer.handles.length, offset);
      _buffer.handles.add(value);
    }
  }

  void encodeMessagePipeHandle(
      core.MojoMessagePipeEndpoint value, int offset, bool nullable) =>
      encodeHandle(value != null ? value.handle : null, offset, nullable);

  void encodeConsumerHandle(
      core.MojoDataPipeConsumer value, int offset, bool nullable) =>
      encodeHandle(value != null ? value.handle : null, offset, nullable);

  void encodeProducerHandle(
      core.MojoDataPipeProducer value, int offset, bool nullable) =>
      encodeHandle(value != null ? value.handle : null, offset, nullable);

  void encodeSharedBufferHandle(
      core.MojoSharedBuffer value, int offset, bool nullable) =>
      encodeHandle(value != null ? value.handle : null, offset, nullable);

  void encodeInterface(Interface interface, int offset, bool nullable) {
    if (interface == null) {
      encodeInvalideHandle(offset, nullable);
      return;
    }
    var pipe = new core.MojoMessagePipe();
    interface.bind(pipe.endpoints[0]);
    encodeMessagePipeHandle(pipe.endpoints[1], offset, nullable);
  }

  void encodeInterfaceRequest(Client client, int offset, bool nullable) {
    if (client == null) {
      encodeInvalideHandle(offset, nullable);
      return;
    }
    var pipe = new core.MojoMessagePipe();
    client.bind(pipe.endpoints[0]);
    encodeMessagePipeHandle(pipe.endpoints[1], offset, nullable);
  }

  void encodeNullPointer(int offset, bool nullable) {
    if (!nullable) {
      throw 'Trying to encode a null pointer for a non-nullable type';
    }
    _buffer.buffer.setUint64(_base + offset, 0, Endianness.LITTLE_ENDIAN);
  }

  void encodeInvalideHandle(int offset, bool nullable) {
    if (!nullable) {
      throw 'Trying to encode a null pointer for a non-nullable type';
    }
    _buffer.buffer.setInt32(_base + offset, -1, Endianness.LITTLE_ENDIAN);
  }

  void encodePointerToNextUnclaimed(int offset) =>
      encodeUint64(_buffer.extent - (_base + offset), offset);

  void encodeStruct(Struct value, int offset, bool nullable) {
    if (value == null) {
      encodeNullPointer(offset, nullable);
      return;
    }
    encodePointerToNextUnclaimed(offset);
    value.encode(this);
  }

  Encoder encodePointerArray(int length, int offset, int expectedLength) =>
      encoderForArray(kPointerSize, length, offset, expectedLength);

  Encoder encoderForArray(
      int elementSize, int length, int offset, int expectedLength) {
    if ((expectedLength != kUnspecifiedArrayLength) &&
        (expectedLength != length)) {
      throw 'Trying to encode a fixed array of incorrect length';
    }
    return encoderForArrayByTotalSize(length * elementSize, length, offset);
  }

  Encoder encoderForArrayByTotalSize(int size, int length, int offset) {
    encodePointerToNextUnclaimed(offset);
    return getEncoderAtOffset(
        new DataHeader(DataHeader.kHeaderSize + size, length));
  }

  void encodeBoolArray(
      List<bool> value, int offset, int nullability, int expectedLength) {
    if (value == null) {
      encodeNullPointer(offset, isArrayNullable(nullability));
      return;
    }
    if ((expectedLength != kUnspecifiedArrayLength) &&
        (expectedLength != value.length)) {
      throw 'Trying to encode a fixed array of incorrect size.';
    }
    var bytes = new Uint8List((value.length + 7) ~/ kAlignment);
    for (int i = 0; i < bytes.length; ++i) {
      for (int j = 0; j < kAlignment; ++j) {
        int boolIndex = kAlignment * i + j;
        if ((boolIndex < value.length) && value[boolIndex]) {
          bytes[i] |= (1 << j);
        }
      }
    }
    var encoder = encoderForArrayByTotalSize(
        bytes.length, value.length, offset);
    encoder.appendUint8Array(bytes);
  }

  void encodeArray(Function arrayAppend,
                   int elementBytes,
                   List<int> value,
                   int offset,
                   int nullability,
                   int expectedLength) {
    if (value == null) {
      encodeNullPointer(offset, isArrayNullable(nullability));
      return;
    }
    var encoder = encoderForArray(
        elementBytes, value.length, offset, expectedLength);
    arrayAppend(encoder, value);
  }

  void encodeInt8Array(
      List<int> value, int offset, int nullability, int expectedLength) =>
      encodeArray((e, v) => e.appendInt8Array(v),
                  1, value, offset, nullability, expectedLength);

  void encodeUint8Array(
      List<int> value, int offset, int nullability, int expectedLength) =>
      encodeArray((e, v) => e.appendUint8Array(v),
                  1, value, offset, nullability, expectedLength);

  void encodeInt16Array(
      List<int> value, int offset, int nullability, int expectedLength) =>
      encodeArray((e, v) => e.appendInt16Array(v),
                  2, value, offset, nullability, expectedLength);

  void encodeUint16Array(
      List<int> value, int offset, int nullability, int expectedLength) =>
      encodeArray((e, v) => e.appendUint16Array(v),
                  2, value, offset, nullability, expectedLength);

  void encodeInt32Array(
      List<int> value, int offset, int nullability, int expectedLength) =>
      encodeArray((e, v) => e.appendInt32Array(v),
                  4, value, offset, nullability, expectedLength);

  void encodeUint32Array(
      List<int> value, int offset, int nullability, int expectedLength) =>
      encodeArray((e, v) => e.appendUint32Array(v),
                  4, value, offset, nullability, expectedLength);

  void encodeInt64Array(
      List<int> value, int offset, int nullability, int expectedLength) =>
      encodeArray((e, v) => e.appendInt64Array(v),
                  8, value, offset, nullability, expectedLength);

  void encodeUint64Array(
      List<int> value, int offset, int nullability, int expectedLength) =>
      encodeArray((e, v) => e.appendUint64Array(v),
                  8, value, offset, nullability, expectedLength);

  void encodeFloatArray(
      List<int> value, int offset, int nullability, int expectedLength) =>
      encodeArray((e, v) => e.appendFloatArray(v),
                  4, value, offset, nullability, expectedLength);

  void encodeDoubleArray(
      List<int> value, int offset, int nullability, int expectedLength) =>
      encodeArray((e, v) => e.appendDoubleArray(v),
                  8, value, offset, nullability, expectedLength);

  void _handleArrayEncodeHelper(Function elementEncoder,
                                List value,
                                int offset,
                                int nullability,
                                int expectedLength) {
    if (value == null) {
      encodeNullPointer(offset, isArrayNullable(nullability));
      return;
    }
    var encoder = encoderForArray(
        kSerializedHandleSize, value.length, offset, expectedLength);
    for (int i = 0; i < value.length; ++i) {
      int handleOffset = DataHeader.kHeaderSize + kSerializedHandleSize * i;
      elementEncoder(
          encoder, value[i], handleOffset, isElementNullable(nullability));
    }
  }

  void encodeHandleArray(
      List<core.MojoHandle> value,
      int offset,
      int nullability,
      int expectedLength) =>
      _handleArrayEncodeHelper(
          (e, v, o, n) => e.encodeHandle(v, o, n),
          value, offset, nullability, expectedLength);

  void encodeMessagePipeHandleArray(
      List<core.MojoMessagePipeEndpoint> value,
      int offset,
      int nullability,
      int expectedLength) =>
      _handleArrayEncodeHelper(
          (e, v, o, n) => e.encodeMessagePipeHandle(v, o, n),
          value, offset, nullability, expectedLength);

  void encodeConsumerHandleArray(
      List<core.MojoDataPipeConsumer> value,
      int offset,
      int nullability,
      int expectedLength) =>
      _handleArrayEncodeHelper(
          (e, v, o, n) => e.encodeConsumerHandle(v, o, n),
          value, offset, nullability, expectedLength);

  void encodeProducerHandleArray(
      List<core.MojoDataPipeProducer> value,
      int offset,
      int nullability,
      int expectedLength) =>
      _handleArrayEncodeHelper(
          (e, v, o, n) => e.encodeProducerHandle(v, o, n),
          value, offset, nullability, expectedLength);

  void encodeSharedBufferHandleArray(
      List<core.MojoSharedBuffer> value,
      int offset,
      int nullability,
      int expectedLength) =>
      _handleArrayEncodeHelper(
          (e, v, o, n) => e.encodeSharedBufferHandle(v, o, n),
          value, offset, nullability, expectedLength);

  void encodeInterfaceRequestArray(
      List<Client> value,
      int offset,
      int nullability,
      int expectedLength) =>
      _handleArrayEncodeHelper(
          (e, v, o, n) => e.encodeInterfaceRequest(v, o, n),
          value, offset, nullability, expectedLength);

  void encodeInterfaceArray(
      List<Interface> value,
      int offset,
      int nullability,
      int expectedLength) =>
      _handleArrayEncodeHelper(
          (e, v, o, n) => e.encodeInterface(v, o, n),
          value, offset, nullability, expectedLength);

  static Uint8List _utf8OfString(String s) =>
      (new Uint8List.fromList((const Utf8Encoder()).convert(s)));

  void encodeString(String value, int offset, bool nullable) {
    if (value == null) {
      encodeNullPointer(offset, nullable);
      return;
    }
    int nullability = nullable ? kArrayNullable : kNothingNullable;
    encodeUint8Array(_utf8OfString(value),
                     offset,
                     nullability,
                     kUnspecifiedArrayLength);
  }

  void appendBytes(Uint8List value) {
    _buffer.buffer.buffer.asUint8List().setRange(
        _base + DataHeader.kHeaderSize,
        _base + DataHeader.kHeaderSize + value.lengthInBytes,
        value);
  }

  void appendInt8Array(List<int> value) =>
      appendBytes(new Uint8List.view(new Int8List.fromList(value)));

  void appendUint8Array(List<int> value) =>
      appendBytes(new Uint8List.fromList(value));

  void appendInt16Array(List<int> value) =>
      appendBytes(new Uint8List.view(new Int16List.fromList(value)));

  void appendUint16Array(List<int> value) =>
      appendBytes(new Uint8List.view(new Uint16List.fromList(value)));

  void appendInt32Array(List<int> value) =>
      appendBytes(new Uint8List.view(new Int32List.fromList(value)));

  void appendUint32Array(List<int> value) =>
      appendBytes(new Uint8List.view(new Uint32List.fromList(value)));

  void appendInt64Array(List<int> value) =>
      appendBytes(new Uint8List.view(new Int64List.fromList(value)));

  void appendUint64Array(List<int> value) =>
      appendBytes(new Uint8List.view(new Uint64List.fromList(value)));

  void appendFloatArray(List<int> value) =>
      appendBytes(new Uint8List.view(new Float32List.fromList(value)));

  void appendDoubleArray(List<int> value) =>
      appendBytes(new Uint8List.view(new Float64List.fromList(value)));

  Encoder encoderForMap(int offset) {
    encodePointerToNextUnclaimed(offset);
    return getEncoderAtOffset(kMapStructHeader);
  }
}


class Decoder {
  Message _message;
  int _base = 0;

  Decoder(this._message, [this._base = 0]);

  Decoder getDecoderAtPosition(int offset) => new Decoder(_message, offset);

  factory Decoder.atOffset(Decoder d, int offset) =>
      new Decoder(d._message, offset);

  ByteData get _buffer => _message.buffer;
  List<core.MojoHandle> get _handles => _message.handles;

  int decodeInt8(int offset) => _buffer.getInt8(_base + offset);
  int decodeUint8(int offset) => _buffer.getUint8(_base + offset);
  int decodeInt16(int offset) =>
      _buffer.getInt16(_base + offset, Endianness.LITTLE_ENDIAN);
  int decodeUint16(int offset) =>
      _buffer.getUint16(_base + offset, Endianness.LITTLE_ENDIAN);
  int decodeInt32(int offset) =>
      _buffer.getInt32(_base + offset, Endianness.LITTLE_ENDIAN);
  int decodeUint32(int offset) =>
      _buffer.getUint32(_base + offset, Endianness.LITTLE_ENDIAN);
  int decodeInt64(int offset) =>
      _buffer.getInt64(_base + offset, Endianness.LITTLE_ENDIAN);
  int decodeUint64(int offset) =>
      _buffer.getUint64(_base + offset,Endianness.LITTLE_ENDIAN);
  double decodeFloat(int offset) =>
      _buffer.getFloat32(_base + offset, Endianness.LITTLE_ENDIAN);
  double decodeDouble(int offset) =>
      _buffer.getFloat64(_base + offset, Endianness.LITTLE_ENDIAN);

  bool decodeBool(int offset, int bit) =>
      (decodeUint8(offset) & (1 << bit)) != 0;

  core.MojoHandle decodeHandle(int offset, bool nullable) {
    int index = decodeInt32(offset);
    if (index == -1) {
      if (!nullable) {
        throw 'Trying to decode an invalid handle from a non-nullable type.';
      }
      return new core.MojoHandle(core.MojoHandle.INVALID);
    }
    return _handles[index];
  }

  core.MojoMessagePipeEndpoint decodeMessagePipeHandle(
      int offset, bool nullable) =>
      new core.MojoMessagePipeEndpoint(decodeHandle(offset, nullable));

  core.MojoDataPipeConsumer decodeConsumerHandle(int offset, bool nullable) =>
      new core.MojoDataPipeConsumer(decodeHandle(offset, nullable));

  core.MojoDataPipeProducer decodeProducerHandle(int offset, bool nullable) =>
      new core.MojoDataPipeProducer(decodeHandle(offset, nullable));

  core.MojoSharedBuffer decodeSharedBufferHandle(int offset, bool nullable) =>
      new core.MojoSharedBuffer(decodeHandle(offset, nullable));

  Client decodeServiceInterface(
      int offset, bool nullable, Function clientFactory) {
    var endpoint = decodeMessagePipeHandle(offset, nullable);
    return endpoint.handle.isValid ? clientFactory(endpoint) : null;
  }

  Interface decodeInterfaceRequest(
      int offset, bool nullable, Function interfaceFactory) {
    var endpoint = decodeMessagePipeHandle(offset, nullable);
    return endpoint.handle.isValid ? interfaceFactory(endpoint) : null;
  }

  Decoder decodePointer(int offset, bool nullable) {
    int basePosition = _base + offset;
    int pointerOffset = decodeUint64(offset);
    if (pointerOffset == 0) {
      if (!nullable) {
        throw 'Trying to decode a null pointer for a non-nullable type';
      }
      return null;
    }
    int newPosition = (basePosition + pointerOffset);
    return new Decoder.atOffset(this, newPosition);
  }

  DataHeader decodeDataHeader() {
    int size = decodeUint32(DataHeader.kSizeOffset);
    int numFields = decodeUint32(DataHeader.kNumFieldsOffset);
    return new DataHeader(size, numFields);
  }

  // Decode arrays.
  DataHeader decodeDataHeaderForBoolArray(int expectedLength) {
    var header = decodeDataHeader();
    if (header.size < DataHeader.kHeaderSize + (header.numFields + 7) ~/ 8) {
      throw 'Array header is incorrect';
    }
    if ((expectedLength != kUnspecifiedArrayLength) &&
        (header.numFields != expectedLength)) {
      throw 'Incorrect array length';
    }
    return header;
  }

  List<bool> decodeBoolArray(int offset, int nullability, int expectedLength) {
    Decoder d = decodePointer(offset, isArrayNullable(nullability));
    if (d == null) {
      return null;
    }
    var header = d.decodeDataHeaderForBoolArray(expectedLength);
    var bytes = new Uint8List.view(
        d._buffer.buffer,
        d._buffer.offsetInBytes + d._base + DataHeader.kHeaderSize,
        (header.numFields + 7) ~/ kAlignment);
    var result = new List<bool>(header.numFields);
    for (int i = 0; i < bytes.lengthInBytes; ++i) {
      for (int j = 0; j < kAlignment; ++j) {
        int boolIndex = i * kAlignment + j;
        if (boolIndex < result.length) {
          result[boolIndex] = (bytes[i] & (1 << j)) != 0;
        }
      }
    }
    return result;
  }

  DataHeader decodeDataHeaderForArray(int elementSize, int expectedLength) {
    var header = decodeDataHeader();
    if (header.size < DataHeader.kHeaderSize + header.numFields * elementSize) {
      throw 'Array header is incorrect: $header, elementSize = $elementSize';
    }
    if ((expectedLength != kUnspecifiedArrayLength) &&
        (header.numFields != expectedLength)) {
      throw 'Incorrect array length.';
    }
    return header;
  }

  DataHeader decodeDataHeaderForPointerArray(int expectedLength) =>
      decodeDataHeaderForArray(kPointerSize, expectedLength);

  List<int> decodeArray(Function arrayViewer,
                        int elementSize,
                        int offset,
                        int nullability,
                        int expectedLength) {
    Decoder d = decodePointer(offset, isArrayNullable(nullability));
    if (d == null) {
      return null;
    }
    var header = d.decodeDataHeaderForArray(elementSize, expectedLength);
    return arrayViewer(
        d._buffer.buffer,
        d._buffer.offsetInBytes + d._base + DataHeader.kHeaderSize,
        header.numFields);
  }

  List<int> decodeInt8Array(
      int offset, int nullability, int expectedLength) =>
      decodeArray((b, s, l) => new Int8List.view(b, s, l),
                  1, offset, nullability, expectedLength);

  List<int> decodeUint8Array(
      int offset, int nullability, int expectedLength) =>
      decodeArray((b, s, l) => new Uint8List.view(b, s, l),
                  1, offset, nullability, expectedLength);

  List<int> decodeInt16Array(
      int offset, int nullability, int expectedLength) =>
      decodeArray((b, s, l) => new Int16List.view(b, s, l),
                  2, offset, nullability, expectedLength);

  List<int> decodeUint16Array(
      int offset, int nullability, int expectedLength) =>
      decodeArray((b, s, l) => new Uint16List.view(b, s, l),
                  2, offset, nullability, expectedLength);

  List<int> decodeInt32Array(
      int offset, int nullability, int expectedLength) =>
      decodeArray((b, s, l) => new Int32List.view(b, s, l),
                  4, offset, nullability, expectedLength);

  List<int> decodeUint32Array(
      int offset, int nullability, int expectedLength) =>
      decodeArray((b, s, l) => new Uint32List.view(b, s, l),
                  4, offset, nullability, expectedLength);

  List<int> decodeInt64Array(
      int offset, int nullability, int expectedLength) =>
      decodeArray((b, s, l) => new Int64List.view(b, s, l),
                  8, offset, nullability, expectedLength);

  List<int> decodeUint64Array(
      int offset, int nullability, int expectedLength) =>
      decodeArray((b, s, l) => new Uint64List.view(b, s, l),
                  8, offset, nullability, expectedLength);

  List<double> decodeFloatArray(
      int offset, int nullability, int expectedLength) =>
      decodeArray((b, s, l) => new Float32List.view(b, s, l),
                  4, offset, nullability, expectedLength);

  List<double> decodeDoubleArray(
      int offset, int nullability, int expectedLength) =>
      decodeArray((b, s, l) => new Float64List.view(b, s, l),
                  8, offset, nullability, expectedLength);

  List _handleArrayDecodeHelper(Function elementDecoder,
                                int offset,
                                int nullability,
                                int expectedLength) {
    Decoder d = decodePointer(offset, isArrayNullable(nullability));
    if (d == null) {
      return null;
    }
    var header = d.decodeDataHeaderForArray(4, expectedLength);
    var result = new List(header.numFields);
    for (int i = 0; i < result.length; ++i) {
      result[i] = elementDecoder(
          d,
          DataHeader.kHeaderSize + kSerializedHandleSize * i,
          isElementNullable(nullability));
    }
    return result;

  }

  List<core.MojoHandle> decodeHandleArray(
      int offset, int nullability, int expectedLength) =>
      _handleArrayDecodeHelper((d, o, n) => d.decodeHandle(o, n),
                               offset, nullability, expectedLength);

  List<core.MojoDataPipeConsumer> decodeConsumerHandleArray(
      int offset, int nullability, int expectedLength) =>
      _handleArrayDecodeHelper((d, o, n) => d.decodeConsumerHandle(o, n),
                               offset, nullability, expectedLength);

  List<core.MojoDataPipeProducer> decodeProducerHandleArray(
      int offset, int nullability, int expectedLength) =>
      _handleArrayDecodeHelper((d, o, n) => d.decodeProducerHandle(o, n),
                               offset, nullability, expectedLength);

  List<core.MojoMessagePipeEndpoint> decodeMessagePipeHandleArray(
      int offset, int nullability, int expectedLength) =>
      _handleArrayDecodeHelper((d, o, n) => d.decodeMessagePipeHandle(o, n),
                               offset, nullability, expectedLength);

  List<core.MojoSharedBuffer> decodeSharedBufferHandleArray(
      int offset, int nullability, int expectedLength) =>
      _handleArrayDecodeHelper((d, o, n) => d.decodeSharedBufferHandle(o, n),
                               offset, nullability, expectedLength);

  List<Interface> decodeInterfaceRequestArray(
      int offset,
      int nullability,
      int expectedLength,
      Function interfaceFactory) =>
      _handleArrayDecodeHelper(
          (d, o, n) => d.decodeInterfaceRequest(o, n, interfaceFactory),
          offset, nullability, expectedLength);

  List<Client> decodeServiceInterfaceArray(
      int offset,
      int nullability,
      int expectedLength,
      Function clientFactory) =>
      _handleArrayDecodeHelper(
          (d, o, n) => d.decodeServiceInterface(o, n, clientFactory),
          offset, nullability, expectedLength);

  static String _stringOfUtf8(Uint8List bytes) =>
      (const Utf8Decoder()).convert(bytes.toList());

  String decodeString(int offset, bool nullable) {
    int nullability = nullable ? kArrayNullable : 0;
    var bytes = decodeUint8Array(offset, nullability, kUnspecifiedArrayLength);
    if (bytes == null) {
      return null;
    }
    return _stringOfUtf8(bytes);
  }
}
