// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function MpegParser(parent) {
  MetadataParser.apply(this, [parent]);
  this.verbose = true;
}

MpegParser.parserType = 'mpeg';

MpegParser.prototype = {__proto__: MetadataParser.prototype};

MpegParser.prototype.urlFilter = /\.(mp4|m4v|m4a|mpe?g4?)$/i;

MpegParser.prototype.parse = function (file, callback, onError) {
  // TODO(rginda): This function should be broken up into methods on
  // MpegParser.prototype.  It may also benefit from a ByteReader instance,
  // rather than tracking the current position on its own.
  // TODO(rginda): Fix variables_with_underscores.

  const HEADER_SIZE = 8;

  var self = this;
  var metadata = {metadataType: 'mpeg'};

  // MPEG-4 format parsing primitives.

  function parseFourCC(view, pos, opt_end) {
     return ByteReader.readString(view, pos, 4, opt_end || view.byteLength);
  }

  function parseUint16(view, pos, opt_end) {
    ByteReader.validateRead(pos, 2, opt_end || view.byteLength);
    return view.getUint16(pos);
  }

  function parseUint32(view, pos, opt_end) {
    ByteReader.validateRead(pos, 4, opt_end || view.byteLength);
    return view.getUint32(pos);
  }

  // TODO(rginda): opt_end is used to validate the current read, as well as the
  // size that gets read.  If the view doesn't contain the entire atom, then
  // passing opt_end could result in a false error.
  function parseAtomSize(view, pos, opt_end) {
    var size = parseUint32(view, pos, opt_end || view.byteLength);
    self.log('atom size: ' + size);
    if (size < HEADER_SIZE)
      throw "atom too short (" + size + ") @" + pos;

    if (opt_end && (opt_end < pos + size))
      throw "atom too long (" + size + ") @" + pos;

    return size;
  }

  function parseAtomName(view, pos, opt_end) {
    return parseFourCC(view, pos, opt_end || view.byteLength).toLowerCase();
  }

  function findParentAtom(atom, name) {
    for (;;) {
      atom = atom.parent;
      if (!atom) return null;
      if (atom.name == name) return atom;
    }
  }

  // Specific atom parsers.
  function parseFtyp(view, atom) {
    metadata.brand = parseFourCC(view, atom.start, atom.end);
  }

  function parseMvhd(view, atom) {
    var version = parseUint32(view, atom.start, atom.end);
    var offset = (version == 0) ? 12 : 20;
    var timescale = parseUint32(view, atom.start + offset, atom.end)
    var duration = parseUint32(view, atom.start + offset + 4, atom.end)
    metadata.duration =  duration / timescale;
  }

  function parseHdlr(view, atom) {
    findParentAtom(atom, 'trak').trackType =
        parseFourCC(view, atom.start + 8, atom.end);
  }

  function parseStsd(view, atom) {
    var track = findParentAtom(atom, 'trak');
    if (track && track.trackType == 'vide') {
      metadata.width = parseUint16(view, atom.start + 40, atom.end);
      metadata.height = parseUint16(view, atom.start + 42, atom.end);
    }
  }

  function parseDataString(name, view, atom) {
    metadata[name] = parseUtil.parseString(view, atom.start + 8, atom.end);
  }

  function parseCovr(view, atom) {
    metadata.thumbnailURL =
        parseUtil.createImageDataUrl(view, atom.start + 8, atom.end);
  }

  // 'meta' atom can occur at one of the several places in the file structure.
  var parseMeta = {
    ilst: {
      "©nam": { data : parseDataString.bind(null, "title") },
      "©alb": { data : parseDataString.bind(null, "album") },
      "©art": { data : parseDataString.bind(null, "artist") },
      "covr": { data : parseCovr }
    },
    versioned: true
  };

  // main parser for the entire file structure.
  var parseRoot = {
    ftyp:  parseFtyp,
    moov: {
      mvhd : parseMvhd,
      trak: {
        mdia: {
          hdlr: parseHdlr,
          minf: {
            stbl: {
              stsd: parseStsd
            }
          }
        },
        meta: parseMeta
      },
      udta: {
        meta: parseMeta
      },
      meta: parseMeta
    },
    meta: parseMeta
  };

  function applyParser(parser, view, atom, file_pos) {
    if (self.verbose) {
      var path = atom.name;
      for (var p = atom.parent; p && p.name; p = p.parent) {
        path = p.name + '.' + path;
      }

      var action;
      if (!parser) {
        action = 'skipping ';
      } else if (parser instanceof Function) {
        action = 'parsing  ';
      } else {
        action = 'recursing';
      }

      var start = atom.start - HEADER_SIZE;
      self.vlog(path + ': ' +
                '@' + (file_pos + start) + ':' + (atom.end - start),
                action);
    }

    if (parser) {
      if (parser instanceof Function) {
        parser(view, atom);
      } else {
        if (parser.versioned) {
          atom.start += 4;
        }
        parseMpegAtomsInRange(parser, view, atom, file_pos);
      }
    }
  }

  function parseMpegAtomsInRange(parser, view, parentAtom, file_pos) {
    var count = 0;
    for (var offset = parentAtom.start; offset != parentAtom.end;) {
      if (count++ > 100) // Most likely we are looping through a corrupt file.
        throw "too many child atoms in " + parentAtom.name + " @" + offset;

      var size = parseAtomSize(view, offset, parentAtom.end);
      var name = parseAtomName(view, offset + 4, parentAtom.end);

      applyParser(
          parser[name],
          view,
          { start: offset + HEADER_SIZE,
            end: offset + size,
            name: name,
            parent: parentAtom
          },
          file_pos
      );

      offset += size;
    }
  }

  function requestRead(file, file_pos, size, name) {
    var reader = new FileReader();
    reader.onerror = onError;
    reader.onload = function(event) {
      processTopLevelAtom(file, reader.result, file_pos, size, name);
    };
    self.vlog("reading @" + file_pos + ":" + size);
    reader.readAsArrayBuffer(file.webkitSlice(file_pos, file_pos + size));
  }

  function processTopLevelAtom(file, buf, file_pos, size, name) {
    try {
      var view = new DataView(buf);

      var atom_end = size - HEADER_SIZE; // the header has already been read.

      var viewLength = view.byteLength;

      // Check the available data size. It should be either exactly
      // what we requested or HEADER_SIZE bytes less (for the last atom).
      if (viewLength != atom_end && viewLength != size) {
        throw "Read failure @" + file_pos + ", " +
            "requested " + size + ", read " + viewLength;
      }

      // Process the top level atom.
      if (name) { // name is undefined only the first time.
        applyParser(
            parseRoot[name],
            view,
            {start: 0, end: atom_end, name: name},
            file_pos
        );
      }

      file_pos += viewLength;
      if (viewLength == size) {
        // The previous read returned everything we asked for, including
        // the next atom header at the end of the buffer.
        // Parse this header and schedule the next read.
        var next_size = parseAtomSize(view, viewLength - HEADER_SIZE);
        var next_name = parseAtomName(view, viewLength - HEADER_SIZE + 4);

        // If we do not have a parser for the next atom, skip the content and
        // read only the header (the one after the next).
        if (!parseRoot[next_name]) {
          file_pos += next_size - HEADER_SIZE;
          next_size = HEADER_SIZE;
        }

        requestRead(file, file_pos, next_size, next_name);
      } else {
        // The previous read did not return the next atom header, EOF reached.
        self.vlog("EOF @" + file_pos);
        metadata.fileSize = file_pos;
        callback(metadata);
      }
    } catch(e) {
      return onError(e.toString());
    }
  }

  // Kick off the processing by reading the first atom's header.
  requestRead(file, 0, HEADER_SIZE);
};

MetadataDispatcher.registerParserClass(MpegParser);
