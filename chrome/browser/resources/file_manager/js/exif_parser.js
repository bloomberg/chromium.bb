// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const EXIF_MARK_SOI = 0xffd8;  // Start of image data.
const EXIF_MARK_SOS = 0xffda;  // Start of "stream" (the actual image data).
const EXIF_MARK_SOF = 0xffc0;  // Start of "frame"
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
const EXIF_TAG_X_DIMENSION = 0xA002;
const EXIF_TAG_Y_DIMENSION = 0xA003;

function ExifParser(parent) {
  ImageParser.call(this, parent, 'jpeg', /\.jpe?g$/i);
}

ExifParser.prototype = {__proto__: ImageParser.prototype};

ExifParser.prototype.parse = function(file, metadata, callback, errorCallback) {
  this.requestSlice(file, callback, errorCallback, metadata, 0);
};

ExifParser.prototype.requestSlice = function (
    file, callback, errorCallback, metadata, filePos, opt_length) {
  // Read at least 1Kb so that we do not issue too many read requests.
  opt_length = Math.max(1024, opt_length || 0);

  var self = this;
  var reader = new FileReader();
  reader.onerror = errorCallback;
  reader.onload = function() { self.parseSlice(
      file, callback, errorCallback, metadata, filePos, reader.result);
  };
  reader.readAsArrayBuffer(file.webkitSlice(filePos, filePos + opt_length));
};

ExifParser.prototype.parseSlice = function(
    file, callback, errorCallback, metadata, filePos, buf) {
  try {
    var br = new ByteReader(buf);

    if (!br.canRead(4)) {
      // We never ask for less than 4 bytes. This can only mean we reached EOF.
      throw new Error('Unexpected EOF @' + (filePos + buf.byteLength));
    }

    if (filePos == 0) {
      // First slice, check for the SOI mark.
      var firstMark = this.readMark(br);
      if (firstMark != EXIF_MARK_SOI)
        throw new Error('Invalid file header: ' + firstMark.toString(16));
    }

    var self = this;
    function reread(opt_offset, opt_bytes) {
      self.requestSlice(file, callback, errorCallback, metadata,
          filePos + br.tell() + (opt_offset || 0), opt_bytes);
    }

    while (true) {
      if (!br.canRead(4)) {
        // Cannot read the mark and the length, request a minimum-size slice.
        reread();
        return;
      }

      var mark = this.readMark(br);
      if (mark == EXIF_MARK_SOS)
        throw new Error('SOS marker found before SOF');

      var markLength = this.readMarkLength(br);

      var nextSectionStart = br.tell() + markLength;
      if (!br.canRead(markLength)) {
        // Get the entire section.
        reread(-4, markLength + 4);
        return;
      }

      if (mark == EXIF_MARK_EXIF) {
        this.parseExifSection(metadata, buf, br);
      } else if (mark == EXIF_MARK_SOF) {
        // The most reliable size information is encoded in the SOF section.
        br.seek(1, ByteReader.SEEK_CUR); // Skip the precision byte.
        var height = br.readScalar(2);
        var width = br.readScalar(2);
        ExifParser.setImageSize(metadata, width, height);
        callback(metadata);  // We are done!
        return;
      }

      br.seek(nextSectionStart, ByteReader.SEEK_BEG);
    }
  } catch (e) {
    errorCallback(e.toString());
  }
};

ExifParser.prototype.parseExifSection = function(metadata, buf, br) {
  var magic = br.readString(6);
  if (magic != 'Exif\0\0') {
    // Some JPEG files may have sections marked with EXIF_MARK_EXIF
    // but containing something else (e.g. XML text). Ignore such sections.
    this.vlog('Invalid EXIF magic: ' + magic + br.readString(100));
    return;
  }

  // Offsets inside the EXIF block are based after the magic string.
  // Create a new ByteReader based on the current position to make offset
  // calculations simpler.
  br = new ByteReader(buf, br.tell());

  var order = br.readScalar(2);
  if (order == EXIF_ALIGN_LITTLE) {
    br.setByteOrder(ByteReader.LITTLE_ENDIAN);
  } else if (order != EXIF_ALIGN_BIG) {
    this.log('Invalid alignment value: ' + order.toString(16));
    return;
  }

  var tag = br.readScalar(2);
  if (tag != EXIF_TAG_TIFF) {
    this.log('Invalid TIFF tag: ' + tag.toString(16));
    return;
  }

  metadata.littleEndian = (order == EXIF_ALIGN_LITTLE);
  metadata.ifd = {
    image: {},
    thumbnail: {}
  };
  var directoryOffset = br.readScalar(4);

  // Image directory.
  this.vlog('Read image directory.');
  br.seek(directoryOffset);
  directoryOffset = this.readDirectory(br, metadata.ifd.image);
  metadata.imageTransform = this.parseOrientation(metadata.ifd.image);

  // Thumbnail Directory chained from the end of the image directory.
  if (directoryOffset) {
    this.vlog('Read thumbnail directory.');
    br.seek(directoryOffset);
    this.readDirectory(br, metadata.ifd.thumbnail);
    // If no thumbnail orientation is encoded, assume same orientation as
    // the primary image.
    metadata.thumbnailTransform =
        this.parseOrientation(metadata.ifd.thumbnail) ||
        metadata.imageTransform;
  }

  // EXIF Directory may be specified as a tag in the image directory.
  if (EXIF_TAG_EXIFDATA in metadata.ifd.image) {
    this.vlog('Read EXIF directory.');
    directoryOffset = metadata.ifd.image[EXIF_TAG_EXIFDATA].value;
    br.seek(directoryOffset);
    metadata.ifd.exif = {};
    this.readDirectory(br, metadata.ifd.exif);
  }

  // GPS Directory may also be linked from the image directory.
  if (EXIF_TAG_GPSDATA in metadata.ifd.image) {
    this.vlog('Read GPS directory.');
    directoryOffset = metadata.ifd.image[EXIF_TAG_GPSDATA].value;
    br.seek(directoryOffset);
    metadata.ifd.gps = {};
    this.readDirectory(br, metadata.ifd.gps);
  }

  // Thumbnail may be linked from the image directory.
  if (EXIF_TAG_JPG_THUMB_OFFSET in metadata.ifd.thumbnail &&
      EXIF_TAG_JPG_THUMB_LENGTH in metadata.ifd.thumbnail) {
    this.vlog('Read thumbnail image.');
    br.seek(metadata.ifd.thumbnail[EXIF_TAG_JPG_THUMB_OFFSET].value);
    metadata.thumbnailURL = br.readImage(
        metadata.ifd.thumbnail[EXIF_TAG_JPG_THUMB_LENGTH].value);
  } else {
    this.vlog('Image has EXIF data, but no JPG thumbnail.');
  }
};

ExifParser.setImageSize = function(metadata, width, height) {
  if (metadata.imageTransform && metadata.imageTransform.rotate90) {
    metadata.width = height;
    metadata.height = width;
  } else {
    metadata.width = width;
    metadata.height = height;
  }
};

ExifParser.prototype.readMark = function(br) {
  return br.readScalar(2);
};

ExifParser.prototype.readMarkLength = function(br) {
  // Length includes the 2 bytes used to store the length.
  return br.readScalar(2) - 2;
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
    if (totalSize < 1) {
      // This is probably invalid exif data, skip it.
      tag.componentCount = 1;
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
      if (tag.componentCount == 0) {
        tag.value = '';
      } else if (tag.componentCount == 1) {
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
