// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var exif = {
  verbose: false,

  messageHandlers: {
    "init": function() {
      this.log('thumbnailer initialized');
    },

    "get-exif": function(fileURL) {
      this.processOneFile(fileURL, function callback(metadata) {
          postMessage({verb: 'give-exif',
                       arguments: [fileURL, metadata]});
      });
    },
  },

  processOneFile: function(fileURL, callback) {
    var self = this;
    var currentStep = -1;

    function nextStep(var_args) {
      self.vlog('nextStep: ' + steps[currentStep + 1].name);
      steps[++currentStep].apply(self, arguments);
    }

    function onError(err) {
      self.vlog('Error processing: ' + fileURL + ': step: ' +
                steps[currentStep].name + ": " + err);

      postMessage({verb: 'give-exif-error',
                   arguments: [fileURL, steps[currentStep].name, err]});
    }

    var steps =
    [ // Step one, turn the url into an entry.
      function getEntry() {
        webkitResolveLocalFileSystemURL(fileURL,
                                        function(entry) { nextStep(entry) },
                                        onError);
      },

      // Step two, turn the entry into a file.
      function getFile(entry) {
        entry.file(function(file) { nextStep(file) }, onError);
      },

      // Step three, read the file header into a byte array.
      function readHeader(file) {
        var reader = new FileReader(file.webkitSlice(0, 1024));
        reader.onerror = onError;
        reader.onload = function(event) { nextStep(file, reader.result) };
        reader.readAsArrayBuffer(file);
      },

      // Step four, find the exif marker and read all exif data.
      function findExif(file, buf) {
        var br = new exif.BufferReader(buf);
        var mark = br.readMark();
        if (mark != exif.MARK_SOI)
          return onError('Invalid file header: ' + mark.toString(16));

        while (true) {
          if (mark == exif.MARK_SOS || br.eof()) {
            return onError('Unable to find EXIF marker');
          }

          mark = br.readMark();
          if (mark == exif.MARK_EXIF) {
            var length = br.readMarkLength();

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

          br.skipMarkData();
        }
      },

      // Step five, parse the exif data.
      function parseExif(file, buf) {
        var br = new exif.BufferReader(buf);
        var order = br.readScalar(2);
        if (order == exif.ALIGN_LITTLE) {
          br.setByteOrder(exif.BufferReader.LITTLE_ENDIAN);
        } else if (order != exif.ALIGN_BIG) {
          return onError('Invalid alignment value: ' + order.toString(16));
        }

        var tag = br.readScalar(2);
        if (tag != exif.TAG_TIFF)
          return onError('Invalid TIFF tag: ' + tag.toString(16));

        var tags = {};
        var directoryOffset = br.readScalar(4);

        while (directoryOffset) {
          br.seek(directoryOffset);
          var entryCount = br.readScalar(2);
          for (var i = 0; i < entryCount; i++) {
            var tag = tags[br.readScalar(2)] = {};
            tag.format = br.readScalar(2);
            tag.componentCount = br.readScalar(4);
            tag.value = br.readScalar(4);
          };

          directoryOffset = br.readScalar(4);
        }

        var metadata = { rawTags: tags };

        if (exif.TAG_JPG_THUMB_OFFSET in tags &&
            exif.TAG_JPG_THUMB_LENGTH in tags) {
          br.seek(tags[exif.TAG_JPG_THUMB_OFFSET].value);
          var b64 = br.readBase64(tags[exif.TAG_JPG_THUMB_LENGTH].value);
          metadata.thumbnailURL = 'data:image/jpeg;base64,' + b64;
        } else {
          self.vlog('Image has EXIF data, but no JPG thumbnail.');
        }

        if (exif.TAG_EXIF_IMAGE_WIDTH in tags)
          metadata.exifImageWidth = tags[exif.TAG_IMAGE_WIDTH];

        if (exif.TAG_EXIF_IMAGE_HEIGHT in tags)
          metadata.exifImageHeight = tags[exif.TAG_IMAGE_HEIGHT];

        nextStep(metadata);
      },

      // Step six, we're done.
      callback
    ];

    nextStep();
  },

  onMessage: function(event) {
    var data = event.data;

    if (this.messageHandlers.hasOwnProperty(data.verb)) {
      //this.log('dispatching: ' + data.verb + ': ' + data.arguments);
      this.messageHandlers[data.verb].apply(this, data.arguments);
    } else {
      this.log('Unknown message from client: ' + data.verb, data);
    }
  },

  log: function(var_args) {
    var ary = Array.apply(null, arguments);
    postMessage({verb: 'log', arguments: ary});
  },

  vlog: function(var_args) {
    if (this.verbose)
      this.log.apply(this, arguments);
  }
};

exif.MARK_SOI = 0xffd8;  // Start of image data.
exif.MARK_SOS = 0xffda;  // Start of "stream" (the actual image data).
exif.MARK_EXIF = 0xffe1;  // Start of exif block.

exif.ALIGN_LITTLE = 0x4949;  // Indicates little endian alignment of exif data.
exif.ALIGN_BIG = 0x4d4d;  // Indicates big endian alignment of exif data.

exif.TAG_TIFF = 0x002a;  // First tag in the exif data.
exif.TAG_JPG_THUMB_OFFSET = 0x0201;
exif.TAG_JPG_THUMB_LENGTH = 0x0202;
exif.TAG_EXIF_IMAGE_WIDTH = 0xa002;
exif.TAG_EXIF_IMAGE_HEIGHT = 0xa003;

exif.BufferReader = function(buf) {
  this.buf_ = buf;
  this.ary_ = new Uint8Array(buf);
  this.pos_ = 0;
  this.setByteOrder(exif.BufferReader.BIG_ENDIAN);
};

exif.BufferReader.LITTLE_ENDIAN = 0;  // Intel, 0x1234 is [0x34, 0x12]
exif.BufferReader.BIG_ENDIAN = 1;  // Motorola, 0x002a is [0x12, 0x34]

exif.BufferReader.prototype = {
  setByteOrder: function(order) {
    this.order_ = order;
    if (order == exif.BufferReader.LITTLE_ENDIAN) {
      this.readScalar = this.readLittle;
    } else {
      this.readScalar = this.readBig;
    }
  },

  eof: function() {
    return this.pos_ >= this.ary_.length;
  },

  readScalar: null,  // Either readLittle or readBig, according to byte order.

  /**
   * Big endian read.  Most significant bytes come first.
   */
  readBig: function(width) {
    var rv = 0;
    switch(width) {
      case 4:
        rv = this.ary_[this.pos_++] << 24;
      case 3:
        rv |= this.ary_[this.pos_++] << 16;
      case 2:
        rv |= this.ary_[this.pos_++] << 8;
      case 1:
        rv |= this.ary_[this.pos_++];
    }

    return rv;
  },

  /**
   * Little endian read.  Least significant bytes come first.
   */
  readLittle: function(width) {
    var rv = 0;
    switch(width) {
      case 4:
        rv = this.ary_[this.pos_ + 3] << 24;
      case 3:
        rv |= this.ary_[this.pos_ + 2] << 16;
      case 2:
        rv |= this.ary_[this.pos_+ 1] << 8;
      case 1:
        rv |= this.ary_[this.pos_];
    }

    this.pos_ += width;
    return rv;
  },

  readString: function(length) {
    var chars = [];
    for (var i = 0; i < length; i++) {
      chars[i] = String.fromCharCode(this.ary_[this.pos_++]);
    }

    return chars.join('');
  },

  base64Alphabet_: ('ABCDEFGHIJKLMNOPQRSTUVWXYZ' +
                    'abcdefghijklmnopqrstuvwxyz' +
                    '0123456789+/').split(''),

  readBase64: function(length) {
    var rv = [];
    var chars = [];
    var padding = 0;

    for (var i = 0; i < length; /* incremented inside */) {
      var bits = this.ary_[this.pos_ + i++] << 16;

      if (i < length) {
        bits |= this.ary_[this.pos_ + i++] << 8;

        if (i < length) {
          bits |= this.ary_[this.pos_ + i++];
        } else {
          padding = 1;
        }
      } else {
        padding = 2;
      }

      chars[3] = this.base64Alphabet_[bits & 63];
      chars[2] = this.base64Alphabet_[(bits >> 6) & 63];
      chars[1] = this.base64Alphabet_[(bits >> 12) & 63];
      chars[0] = this.base64Alphabet_[(bits >> 18) & 63];

      rv.push.apply(rv, chars);
    }

    this.pos_ += i;

    if (padding > 0)
      chars[chars.length - 1] = '=';
    if (padding > 1)
      chars[chars.length - 2] = '=';

    return rv.join('');
  },

  readMark: function() {
    return this.readScalar(2);
  },

  readMarkLength: function() {
    // Length includes the 2 bytes used to store the length.
    return this.readScalar(2) - 2;
  },

  readMarkData: function(opt_arrayConstructor) {
    var arrayConstructor = opt_arrayConstructor || Uint8Array;

    var length = this.readMarkLength();
    var slice = new arrayConstructor(this.buf_, this.pos_, length);
    this.pos_ += length;

    return slice;
  },

  skipMarkData: function() {
    this.skip(this.readMarkLength());
  },

  seek: function(pos) {
    this.pos_ = pos;
  },

  skip: function(count) {
    this.pos_ += count;
  },

  tell: function() {
    return this.pos_;
  }
};

var onmessage = exif.onMessage.bind(exif);
