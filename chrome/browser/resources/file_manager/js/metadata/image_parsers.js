// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/* Base class for image metadata parsers that only need to look at a short
  fragment at the start of the file */
function SimpleImageParser(parent, type, urlFilter, headerSize) {
  ImageParser.call(this, parent, type, urlFilter);
  this.headerSize = headerSize;
}

SimpleImageParser.prototype = {__proto__: ImageParser.prototype};

/**
 * @param {File} file  // TODO(JSDOC).
 * @param {Object} metadata  // TODO(JSDOC).
 * @param {function(Object)} callback  // TODO(JSDOC).
 * @param {function(string)} errorCallback  // TODO(JSDOC).
 */
SimpleImageParser.prototype.parse = function(
    file, metadata, callback, errorCallback) {
  var self = this;
  util.readFileBytes(file, 0, this.headerSize,
    function(file, br) {
      try {
        self.parseHeader(metadata, br);
        callback(metadata);
      } catch (e) {
        errorCallback(e.toString());
      }
    },
    errorCallback);
};


function PngParser(parent) {
  SimpleImageParser.call(this, parent, 'png', /\.png$/i, 24);
}

PngParser.prototype = {__proto__: SimpleImageParser.prototype};

/**
 * @param {Object} metadata  // TODO(JSDOC).
 * @param {ByteReader} br  // TODO(JSDOC).
 */
PngParser.prototype.parseHeader = function(metadata, br) {
  br.setByteOrder(ByteReader.BIG_ENDIAN);

  var signature = br.readString(8);
  if (signature != '\x89PNG\x0D\x0A\x1A\x0A')
    throw new Error('Invalid PNG signature: ' + signature);

  br.seek(12);
  var ihdr = br.readString(4);
  if (ihdr != 'IHDR')
    throw new Error('Missing IHDR chunk');

  metadata.width = br.readScalar(4);
  metadata.height = br.readScalar(4);
};

MetadataDispatcher.registerParserClass(PngParser);


function BmpParser(parent) {
  SimpleImageParser.call(this, parent, 'bmp', /\.bmp$/i, 28);
}

BmpParser.prototype = {__proto__: SimpleImageParser.prototype};

/**
 * @param {Object} metadata  // TODO(JSDOC).
 * @param {ByteReader} br  // TODO(JSDOC).
 */
BmpParser.prototype.parseHeader = function(metadata, br) {
  br.setByteOrder(ByteReader.LITTLE_ENDIAN);

  var signature = br.readString(2);
  if (signature != 'BM')
    throw new Error('Invalid BMP signature: ' + signature);

  br.seek(18);
  metadata.width = br.readScalar(4);
  metadata.height = br.readScalar(4);
};

MetadataDispatcher.registerParserClass(BmpParser);


function GifParser(parent) {
  SimpleImageParser.call(this, parent, 'gif', /\.Gif$/i, 10);
}

GifParser.prototype = {__proto__: SimpleImageParser.prototype};

/**
 * @param {Object} metadata  // TODO(JSDOC).
 * @param {ByteReader} br  // TODO(JSDOC).
 */
GifParser.prototype.parseHeader = function(metadata, br) {
  br.setByteOrder(ByteReader.LITTLE_ENDIAN);

  var signature = br.readString(6);
  if (!signature.match(/GIF8(7|9)a/))
    throw new Error('Invalid GIF signature: ' + signature);

  metadata.width = br.readScalar(2);
  metadata.height = br.readScalar(2);
};

MetadataDispatcher.registerParserClass(GifParser);


function WebpParser(parent) {
  SimpleImageParser.call(this, parent, 'webp', /\.webp$/i, 30);
}

WebpParser.prototype = {__proto__: SimpleImageParser.prototype};

/**
 * @param {Object} metadata  // TODO(JSDOC).
 * @param {ByteReader} br  // TODO(JSDOC).
 */
WebpParser.prototype.parseHeader = function(metadata, br) {
  br.setByteOrder(ByteReader.LITTLE_ENDIAN);

  var riffSignature = br.readString(4);
  if (riffSignature != 'RIFF')
    throw new Error('Invalid RIFF signature: ' + riffSignature);

  br.seek(8);
  var webpSignature = br.readString(4);
  if (webpSignature != 'WEBP')
    throw new Error('Invalid WEBP signature: ' + webpSignature);

  var chunkFormat = br.readString(4);
  if (chunkFormat != 'VP8 ' && chunkFormat != 'VP8L')
    throw new Error('Invalid chunk format: ' + chunkFormat);

  if (chunkFormat == 'VP8 ') {
    // VP8 lossy bitstream format.
    br.seek(23);
    var lossySignature = br.readScalar(2) | (br.readScalar(1) << 16);
    if (lossySignature != 0x2a019d)
      throw new Error('Invalid VP8 lossy bitstream signature: ' +
        lossySignature);

    var dimensionBits = br.readScalar(4);
    metadata.width = dimensionBits & 0x3fff;
    metadata.height = (dimensionBits >> 16) & 0x3fff;
  } else {
    // VP8 lossless bitstream format.
    br.seek(20);
    var losslessSignature = br.readScalar(1);
    if (losslessSignature != 0x2f)
      throw new Error('Invalid VP8 lossless bitstream signature: ' +
        losslessSignature);

    var dimensionBits = br.readScalar(4);
    metadata.width = (dimensionBits & 0x3fff) + 1;
    metadata.height = ((dimensionBits >> 14) & 0x3fff) + 1;
  }
};

MetadataDispatcher.registerParserClass(WebpParser);
