# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Understands .isolated files and can do local operations on them."""

import hashlib
import re


# Version stored and expected in .isolated files.
ISOLATED_FILE_VERSION = '1.4'


# Chunk size to use when doing disk I/O.
DISK_FILE_CHUNK = 1024 * 1024


# The file size to be used when we don't know the correct file size,
# generally used for .isolated files.
UNKNOWN_FILE_SIZE = None


# Sadly, hashlib uses 'sha1' instead of the standard 'sha-1' so explicitly
# specify the names here.
SUPPORTED_ALGOS = {
  'md5': hashlib.md5,
  'sha-1': hashlib.sha1,
  'sha-512': hashlib.sha512,
}


# Used for serialization.
SUPPORTED_ALGOS_REVERSE = dict((v, k) for k, v in SUPPORTED_ALGOS.iteritems())


class IsolatedError(ValueError):
  """Generic failure to load a .isolated file."""
  pass


class MappingError(OSError):
  """Failed to recreate the tree."""
  pass


def is_valid_hash(value, algo):
  """Returns if the value is a valid hash for the corresponding algorithm."""
  size = 2 * algo().digest_size
  return bool(re.match(r'^[a-fA-F0-9]{%d}$' % size, value))


def get_hash_algo(_namespace):
  """Return hash algorithm class to use when uploading to given |namespace|."""
  # TODO(vadimsh): Implement this at some point.
  return hashlib.sha1


def hash_file(filepath, algo):
  """Calculates the hash of a file without reading it all in memory at once.

  |algo| should be one of hashlib hashing algorithm.
  """
  digest = algo()
  with open(filepath, 'rb') as f:
    while True:
      chunk = f.read(DISK_FILE_CHUNK)
      if not chunk:
        break
      digest.update(chunk)
  return digest.hexdigest()
