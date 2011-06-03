// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mpeg = {
  urlFilter: /\.(mp4|m4v|m4a|mpe?g4?)$/i,

  parse: function (file, callback, onError, logger) {
    const HEADER_SIZE = 8;

    var metadata = {};

    // MPEG-4 format parsing primitives.

    function checkBuffer(bytes, pos, end) {
      if (!end) end = bytes.length;
      if (pos > end)
        throw "Reading past the buffer end: pos=" + pos + ", end=" + end;
    }

    function parseFourCC(bytes, pos, end) {
      checkBuffer(bytes, pos + 4, end);
      return parseUtil.parseString(bytes, pos, pos + 4);
    }

    function parseUint16(bytes, pos, end) {
      checkBuffer(bytes, pos + 2, end)
      return parseUtil.parseBig(bytes, pos, 2);
    }

    function parseUint32(bytes, pos, end) {
      checkBuffer(bytes, pos + 4, end)
      return parseUtil.parseBig(bytes, pos, 4);
    }

    function parseAtomSize(bytes, pos, end) {
      var size = parseUint32(bytes, pos, end);

      if (size < HEADER_SIZE)
        throw "atom too short (" + size + ") @" + pos;

      if (end && (end < pos + size))
        throw "atom too long (" + size + ") @" + pos;

      return size;
    }

    function parseAtomName(bytes, pos, end) {
      return parseFourCC(bytes, pos, end).toLowerCase();
    }

    function findParentAtom(atom, name) {
      for (;;) {
        atom = atom.parent;
        if (!atom) return null;
        if (atom.name == name) return atom;
      }
    }

    // Specific atom parsers.
    function parseFtyp(bytes, atom) {
      metadata.brand = parseFourCC(bytes, atom.start, atom.end);
    }

    function parseMvhd(bytes, atom) {
      var version = parseUint32(bytes, atom.start, atom.end);
      var offset = (version == 0) ? 12 : 20;
      var timescale = parseUint32(bytes, atom.start + offset, atom.end)
      var duration = parseUint32(bytes, atom.start + offset + 4, atom.end)
      metadata.duration =  duration / timescale;
    }

    function parseHdlr(bytes, atom) {
      findParentAtom(atom, 'trak').trackType =
          parseFourCC(bytes, atom.start + 8, atom.end);
    }

    function parseStsd(bytes, atom) {
      var track = findParentAtom(atom, 'trak');
      if (track && track.trackType == 'vide') {
        metadata.width = parseUint16(bytes, atom.start + 40, atom.end);
        metadata.height = parseUint16(bytes, atom.start + 42, atom.end);
      }
    }

    function parseDataString(name, bytes, atom) {
      metadata[name] = parseUtil.parseString(bytes, atom.start + 8, atom.end);
    }

    function parseCovr(bytes, atom) {
      metadata.thumbnailURL =
          parseUtil.createImageDataUrl(bytes, atom.start + 8, atom.end);
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

    function applyParser(parser, bytes, atom, file_pos) {
      if (logger.verbose) {
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
        logger.vlog(path,
                  '@' + (file_pos + start) + ':' + (atom.end - start),
                  action);
      }

      if (parser) {
        if (parser instanceof Function) {
          parser(bytes, atom);
        } else {
          if (parser.versioned) {
            atom.start += 4;
          }
          parseMpegAtomsInRange(parser, bytes, atom, file_pos);
        }
      }
    }

    function parseMpegAtomsInRange(parser, bytes, parentAtom, file_pos) {
      var count = 0;
      for (var offset = parentAtom.start; offset != parentAtom.end;) {
        if (count++ > 100) // Most likely we are looping through a corrupt file.
          throw "too many child atoms in " + parentAtom.name + " @" + offset;

        var size = parseAtomSize(bytes, offset, parentAtom.end);
        var name = parseAtomName(bytes, offset + 4, parentAtom.end);

        applyParser(
            parser[name],
            bytes,
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
      logger.vlog("reading @" + file_pos + ":" + size);
      reader.readAsArrayBuffer(file.webkitSlice(file_pos, file_pos + size));
    }

    function processTopLevelAtom(file, buf, file_pos, size, name) {
      try {
        var bytes = new Uint8Array(buf);

        var atom_end = size - HEADER_SIZE; // the header has already been read.

        // Check the available data size. It should be either exactly
        // what we requested or HEADER_SIZE bytes less (for the last atom).
        if (bytes.length != atom_end && bytes.length != size) {
          throw "Read failure @" + file_pos + ", " +
              "requested " + size + ", read " + bytes.length;
        }

        // Process the top level atom.
        if (name) { // name is undefined only the first time.
          applyParser(
              parseRoot[name],
              bytes,
              {start: 0, end: atom_end, name: name},
              file_pos
          );
        }

        file_pos += bytes.length;
        if (bytes.length == size) {
          // The previous read returned everything we asked for, including
          // the next atom header at the end of the buffer.
          // Parse this header and schedule the next read.
          var next_size = parseAtomSize(bytes, bytes.length - HEADER_SIZE);
          var next_name = parseAtomName(bytes, bytes.length - HEADER_SIZE + 4);

          // If we do not have a parser for the next atom, skip the content and
          // read only the header (the one after the next).
          if (!parseRoot[next_name]) {
            file_pos += next_size - HEADER_SIZE;
            next_size = HEADER_SIZE;
          }

          requestRead(file, file_pos, next_size, next_name);
        } else {
          // The previous read did not return the next atom header, EOF reached.
          logger.vlog("EOF @" + file_pos);
          metadata.fileSize = file_pos;
          callback(metadata);
        }
      } catch(e) {
        return onError(e);
      }
    }

    // Kick off the processing by reading the first atom's header.
    requestRead(file, 0, HEADER_SIZE);
  }
};

if (metadataReader) metadataReader.registerParser(mpeg);