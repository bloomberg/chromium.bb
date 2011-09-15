// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const EXIF_MARK_SOI = 0xffd8;  // Start of image data.
const EXIF_MARK_SOS = 0xffda;  // Start of "stream" (the actual image data).
const EXIF_MARK_EXIF = 0xffe1;  // Start of exif block.

const EXIF_ALIGN_LITTLE = 0x4949;  // Indicates little endian exif data.
const EXIF_ALIGN_BIG = 0x4d4d;  // Indicates big endian exif data.

const EXIF_TAG_TIFF = 0x002a;  // First directory containing TIFF data.
const EXIF_TAG_GPSDATA = 0x8825;  // Pointer from TIFF to the GPS directory.
const EXIF_TAG_EXIFDATA = 0x8769;  // Pointer from TIFF to the EXIF IFD.
const EXIF_TAG_SUBIFD = 0x014a;  // Pointer from TIFF to "Extra" IFDs.

const EXIF_TAG_JPG_THUMB_OFFSET = 0x0201;  // Pointer from TIFF to thumbnail.
const EXIF_TAG_JPG_THUMB_LENGTH = 0x0202;  // Length of thumbnail data.

const EXIF_TAG_ORIENTATION = 0x0112;

function ExifParser(parent) {
  MetadataParser.apply(this, [parent]);
  this.verbose = false;
}

ExifParser.parserType = 'exif';

ExifParser.prototype = {__proto__: MetadataParser.prototype};

ExifParser.prototype.urlFilter = /\.jpe?g$/i;

ExifParser.prototype.parse = function(file, callback, errorCallback) {
  var self = this;
  var currentStep = -1;

  function nextStep(var_args) {
    self.vlog('exif nextStep: ' + steps[currentStep + 1].name);
    steps[++currentStep].apply(null, arguments);
  }

  function onError(err) {
    errorCallback(err, steps[currentStep].name);
  }

  var steps =
  [ // Step one, read the file header into a byte array.
    function readHeader(file) {
      var reader = new FileReader();
      reader.onerror = onError;
      reader.onload = function(event) { nextStep(file, reader.result) };
      reader.readAsArrayBuffer(file.webkitSlice(0, 1024));
    },

    // Step two, find the exif marker and read all exif data.
    function findExif(file, buf) {
      var br = new ByteReader(buf);
      var mark = self.readMark(br);
      if (mark != EXIF_MARK_SOI)
        return onError('Invalid file header: ' + mark.toString(16));

      while (true) {
        if (mark == EXIF_MARK_SOS || br.eof()) {
          return onError('Unable to find EXIF marker');
        }

        mark = self.readMark(br);
        if (mark == EXIF_MARK_EXIF) {
          var length = self.readMarkLength(br);

          // Offsets inside the EXIF block are based after this bit of
          // magic, so we verify and discard it here, before exif parsing,
          // to make offset calculations simpler.
          var magic = br.readString(6);
          if (magic != 'Exif\0\0')
            return onError('Invalid EXIF magic: ' + magic.toString(16));

          var pos = br.tell();
          var reader = new FileReader();
          reader.onerror = onError;
          reader.onload = function(event) { nextStep(file, reader.result) };
          reader.readAsArrayBuffer(file.webkitSlice(pos, pos + length - 6));
          return;
        }

        self.skipMarkData(br);
      }
    },

    // Step three, parse the exif data.
    function readDirectories(file, buf) {
      var br = new ByteReader(buf);
      var order = br.readScalar(2);
      if (order == EXIF_ALIGN_LITTLE) {
        br.setByteOrder(ByteReader.LITTLE_ENDIAN);
      } else if (order != EXIF_ALIGN_BIG) {
        return onError('Invalid alignment value: ' + order.toString(16));
      }

      var tag = br.readScalar(2);
      if (tag != EXIF_TAG_TIFF)
        return onError('Invalid TIFF tag: ' + tag.toString(16));

      var metadata = {
        metadataType: 'exif',
        mimeType: 'image/jpeg',
        littleEndian: (order == EXIF_ALIGN_LITTLE),
        ifd: {
          image: {},
          thumbnail: {},
          exif: {},
          gps: {}
        }
      };

      var directoryOffset = br.readScalar(4);

      // Image directory.
      self.vlog('Read image directory.');
      br.seek(directoryOffset);
      directoryOffset = self.readDirectory(br, metadata.ifd.image);
      metadata.imageTransform = self.parseOrientation(metadata.ifd.image);

      // Thumbnail Directory chained from the end of the image directory.
      if (directoryOffset) {
        self.vlog('Read thumbnail directory.');
        br.seek(directoryOffset);
        self.readDirectory(br, metadata.ifd.thumbnail);
        metadata.thumbnailTransform =
            self.parseOrientation(metadata.ifd.thumbnail);
      }

      // EXIF Directory may be specified as a tag in the image directory.
      if (EXIF_TAG_EXIFDATA in metadata.ifd.image) {
        self.vlog('Read EXIF directory.');
        directoryOffset = metadata.ifd.image[EXIF_TAG_EXIFDATA].value;
        br.seek(directoryOffset);
        self.readDirectory(br, metadata.ifd.exif);
      }

      // GPS Directory may also be linked from the image directory.
      if (EXIF_TAG_GPSDATA in metadata.ifd.image) {
        self.vlog('Read GPS directory.');
        directoryOffset = metadata.ifd.image[EXIF_TAG_GPSDATA].value;
        br.seek(directoryOffset);
        self.readDirectory(br, metadata.ifd.gps);
      }

      // Thumbnail may be linked from the image directory.
      if (EXIF_TAG_JPG_THUMB_OFFSET in metadata.ifd.thumbnail &&
          EXIF_TAG_JPG_THUMB_LENGTH in metadata.ifd.thumbnail) {
        self.vlog('Read thumbnail image.');
        br.seek(metadata.ifd.thumbnail[EXIF_TAG_JPG_THUMB_OFFSET].value);
        metadata.thumbnailURL = br.readImage(
            metadata.ifd.thumbnail[EXIF_TAG_JPG_THUMB_LENGTH].value);
      } else {
        self.vlog('Image has EXIF data, but no JPG thumbnail.');
      }

      nextStep(metadata);
    },

    // Step four, we're done.
    callback
  ];

  nextStep(file);
};

ExifParser.prototype.readMark = function(br) {
  return br.readScalar(2);
};

ExifParser.prototype.readMarkLength = function(br) {
  // Length includes the 2 bytes used to store the length.
  return br.readScalar(2) - 2;
};

ExifParser.prototype.readMarkData = function(br) {
  var length = this.readMarkLength(br);
  return br.readSlice(length);
};

ExifParser.prototype.skipMarkData = function(br) {
  br.seek(this.readMarkLength(br), ByteReader.SEEK_CUR);
};

ExifParser.prototype.readDirectory = function(br, tags) {
  var entryCount = br.readScalar(2);
  for (var i = 0; i < entryCount; i++) {
    var tagId = br.readScalar(2);
    var tag = tags[tagId] = {id: tagId};
    tag.format = br.readScalar(2);
    tag.componentCount = br.readScalar(4);
    this.readTagValue(br, tag);
  }

  return br.readScalar(4);
};

ExifParser.prototype.readTagValue = function(br, tag) {
  var self = this;

  function safeRead(size, readFunction, signed) {
    try {
      unsafeRead(size, readFunction, signed);
    } catch (ex) {
      self.log('error reading tag 0x' + tag.id.toString(16) + '/' +
               tag.format + ', size ' + tag.componentCount + '*' + size + ' ' +
               (ex.stack || '<no stack>') + ': ' + ex);
      tag.value = null;
    }
  }

  function unsafeRead(size, readFunction, signed) {
    if (!readFunction)
      readFunction = function(size) { return br.readScalar(size, signed) };

    var totalSize = tag.componentCount * size;
    if (totalSize > 100 || totalSize < 1) {
      // This is probably invalid exif data.
      tag.value = br.readScalar(4);
      return;
    }

    if (totalSize > 4) {
      // If the total size is > 4, the next 4 bytes will be a pointer to the
      // actual data.
      br.pushSeek(br.readScalar(4));
    }

    if (tag.componentCount == 1) {
      tag.value = readFunction(size);
    } else {
      // Read multiple components into an array.
      tag.value = [];
      for (var i = 0; i < tag.componentCount; i++)
        tag.value[i] = readFunction(size);
    }

    if (totalSize > 4) {
      // Go back to the previous position if we had to jump to the data.
      br.popSeek();
    } else if (totalSize < 4) {
      // Otherwise, if the value wasn't exactly 4 bytes, skip over the
      // unread data.
      br.seek(4 - totalSize, ByteReader.SEEK_CUR);
    }
  }

  switch (tag.format) {
    case 1: // Byte
    case 7: // Undefined
      safeRead(1);
      break;

    case 2: // String
      safeRead(1);
      if (tag.componentCount == 1) {
        tag.value = String.fromCharCode(tag.value);
      } else {
        tag.value = String.fromCharCode.apply(null, tag.value);
      }
      break;

    case 3: // Short
      safeRead(2);
      break;

    case 4: // Long
      safeRead(4);
      break;

    case 9: // Signed Long
      safeRead(4, null, true);
      break;

    case 5: // Rational
      safeRead(8, function() {
        return [ br.readScalar(4), br.readScalar(4) ];
      });
      break;

    case 10: // Signed Rational
      safeRead(8, function() {
        return [ br.readScalar(4, true), br.readScalar(4, true) ];
      });
      break;

    default: // ???
      this.vlog('Unknown tag format 0x' + Number(tag.id).toString(16) +
                ': ' + tag.format);
      safeRead(4);
      break;
  }

  this.vlog('Read tag: 0x' + tag.id.toString(16) + '/' + tag.format + ': ' +
            tag.value);
};

ExifParser.SCALEX   = [1, -1, -1,  1,  1,  1, -1, -1];
ExifParser.SCALEY   = [1,  1, -1, -1, -1,  1,  1, -1];
ExifParser.ROTATE90 = [0,  0,  0,  0,  1,  1,  1,  1];

/**
 * Transform exif-encoded orientation into a set of parameters compatible with
 * CSS and canvas transforms (scaleX, scaleY, rotation).
 *
 * @param {Object} ifd exif property dictionary (image or thumbnail)
 */
ExifParser.prototype.parseOrientation = function(ifd) {
  if (ifd[EXIF_TAG_ORIENTATION]) {
    var index = (ifd[EXIF_TAG_ORIENTATION].value || 1) - 1;
    return {
      scaleX: ExifParser.SCALEX[index],
      scaleY: ExifParser.SCALEY[index],
      rotate90: ExifParser.ROTATE90[index]
    }
  }
  return null;
};

MetadataDispatcher.registerParserClass(ExifParser);
