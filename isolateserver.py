#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Archives a set of files to a server."""

__version__ = '0.2'

import binascii
import cStringIO
import hashlib
import itertools
import json
import logging
import os
import random
import re
import sys
import threading
import time
import urllib
import zlib

from third_party import colorama
from third_party.depot_tools import fix_encoding
from third_party.depot_tools import subcommand

from utils import net
from utils import threading_utils
from utils import tools


# Default server.
# TODO(maruel): Chromium-specific.
ISOLATE_SERVER = 'https://isolateserver-dev.appspot.com/'


# The minimum size of files to upload directly to the blobstore.
MIN_SIZE_FOR_DIRECT_BLOBSTORE = 20 * 1024

# The number of files to check the isolate server per /contains query.
# All files are sorted by likelihood of a change in the file content
# (currently file size is used to estimate this: larger the file -> larger the
# possibility it has changed). Then first ITEMS_PER_CONTAINS_QUERIES[0] files
# are taken and send to '/contains', then next ITEMS_PER_CONTAINS_QUERIES[1],
# and so on. Numbers here is a trade-off; the more per request, the lower the
# effect of HTTP round trip latency and TCP-level chattiness. On the other hand,
# larger values cause longer lookups, increasing the initial latency to start
# uploading, which is especially an issue for large files. This value is
# optimized for the "few thousands files to look up with minimal number of large
# files missing" case.
ITEMS_PER_CONTAINS_QUERIES = [20, 20, 50, 50, 50, 100]


# A list of already compressed extension types that should not receive any
# compression before being uploaded.
ALREADY_COMPRESSED_TYPES = [
    '7z', 'avi', 'cur', 'gif', 'h264', 'jar', 'jpeg', 'jpg', 'pdf', 'png',
    'wav', 'zip'
]


# The file size to be used when we don't know the correct file size,
# generally used for .isolated files.
UNKNOWN_FILE_SIZE = None


# The size of each chunk to read when downloading and unzipping files.
ZIPPED_FILE_CHUNK = 16 * 1024


# Chunk size to use when doing disk I/O.
DISK_FILE_CHUNK = 1024 * 1024


# Read timeout in seconds for downloads from isolate storage. If there's no
# response from the server within this timeout whole download will be aborted.
DOWNLOAD_READ_TIMEOUT = 60


# The delay (in seconds) to wait between logging statements when retrieving
# the required files. This is intended to let the user (or buildbot) know that
# the program is still running.
DELAY_BETWEEN_UPDATES_IN_SECS = 30


class ConfigError(ValueError):
  """Generic failure to load a .isolated file."""
  pass


class MappingError(OSError):
  """Failed to recreate the tree."""
  pass


def randomness():
  """Generates low-entropy randomness for MIME encoding.

  Exists so it can be mocked out in unit tests.
  """
  return str(time.time())


def encode_multipart_formdata(fields, files,
                              mime_mapper=lambda _: 'application/octet-stream'):
  """Encodes a Multipart form data object.

  Args:
    fields: a sequence (name, value) elements for
      regular form fields.
    files: a sequence of (name, filename, value) elements for data to be
      uploaded as files.
    mime_mapper: function to return the mime type from the filename.
  Returns:
    content_type: for httplib.HTTP instance
    body: for httplib.HTTP instance
  """
  boundary = hashlib.md5(randomness()).hexdigest()
  body_list = []
  for (key, value) in fields:
    if isinstance(key, unicode):
      value = key.encode('utf-8')
    if isinstance(value, unicode):
      value = value.encode('utf-8')
    body_list.append('--' + boundary)
    body_list.append('Content-Disposition: form-data; name="%s"' % key)
    body_list.append('')
    body_list.append(value)
    body_list.append('--' + boundary)
    body_list.append('')
  for (key, filename, value) in files:
    if isinstance(key, unicode):
      value = key.encode('utf-8')
    if isinstance(filename, unicode):
      value = filename.encode('utf-8')
    if isinstance(value, unicode):
      value = value.encode('utf-8')
    body_list.append('--' + boundary)
    body_list.append('Content-Disposition: form-data; name="%s"; '
                     'filename="%s"' % (key, filename))
    body_list.append('Content-Type: %s' % mime_mapper(filename))
    body_list.append('')
    body_list.append(value)
    body_list.append('--' + boundary)
    body_list.append('')
  if body_list:
    body_list[-2] += '--'
  body = '\r\n'.join(body_list)
  content_type = 'multipart/form-data; boundary=%s' % boundary
  return content_type, body


def is_valid_hash(value, algo):
  """Returns if the value is a valid hash for the corresponding algorithm."""
  size = 2 * algo().digest_size
  return bool(re.match(r'^[a-fA-F0-9]{%d}$' % size, value))


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


def file_read(filepath):
  """Yields file content."""
  with open(filepath, 'rb') as f:
    while True:
      data = f.read(DISK_FILE_CHUNK)
      if not data:
        break
      yield data


def file_write(filepath, content_generator):
  """Writes file content as generated by content_generator.

  Creates the intermediary directory as needed.

  Returns the number of bytes written.

  Meant to be mocked out in unit tests.
  """
  filedir = os.path.dirname(filepath)
  if not os.path.isdir(filedir):
    os.makedirs(filedir)
  total = 0
  with open(filepath, 'wb') as f:
    for d in content_generator:
      total += len(d)
      f.write(d)
  return total


def create_directories(base_directory, files):
  """Creates the directory structure needed by the given list of files."""
  logging.debug('create_directories(%s, %d)', base_directory, len(files))
  # Creates the tree of directories to create.
  directories = set(os.path.dirname(f) for f in files)
  for item in list(directories):
    while item:
      directories.add(item)
      item = os.path.dirname(item)
  for d in sorted(directories):
    if d:
      os.mkdir(os.path.join(base_directory, d))


def create_links(base_directory, files):
  """Creates any links needed by the given set of files."""
  for filepath, properties in files:
    if 'l' not in properties:
      continue
    if sys.platform == 'win32':
      # TODO(maruel): Create junctions or empty text files similar to what
      # cygwin do?
      logging.warning('Ignoring symlink %s', filepath)
      continue
    outfile = os.path.join(base_directory, filepath)
    # symlink doesn't exist on Windows. So the 'link' property should
    # never be specified for windows .isolated file.
    os.symlink(properties['l'], outfile)  # pylint: disable=E1101
    if 'm' in properties:
      lchmod = getattr(os, 'lchmod', None)
      if lchmod:
        lchmod(outfile, properties['m'])


def setup_commands(base_directory, cwd, cmd):
  """Correctly adjusts and then returns the required working directory
  and command needed to run the test.
  """
  assert not os.path.isabs(cwd), 'The cwd must be a relative path, got %s' % cwd
  cwd = os.path.join(base_directory, cwd)
  if not os.path.isdir(cwd):
    os.makedirs(cwd)

  # Ensure paths are correctly separated on windows.
  cmd[0] = cmd[0].replace('/', os.path.sep)
  cmd = tools.fix_python_path(cmd)

  return cwd, cmd


def generate_remaining_files(files):
  """Generates a dictionary of all the remaining files to be downloaded."""
  remaining = {}
  for filepath, props in files:
    if 'h' in props:
      remaining.setdefault(props['h'], []).append((filepath, props))

  return remaining


def is_valid_file(filepath, size):
  """Determines if the given files appears valid.

  Currently it just checks the file's size.
  """
  if size == UNKNOWN_FILE_SIZE:
    return True
  actual_size = os.stat(filepath).st_size
  if size != actual_size:
    logging.warning(
        'Found invalid item %s; %d != %d',
        os.path.basename(filepath), actual_size, size)
    return False
  return True


def try_remove(filepath):
  """Removes a file without crashing even if it doesn't exist."""
  try:
    os.remove(filepath)
  except OSError:
    pass


def url_read(url, **kwargs):
  result = net.url_read(url, **kwargs)
  if result is None:
    # If we get no response from the server, assume it is down and raise an
    # exception.
    raise MappingError('Unable to connect to server %s' % url)
  return result


class IsolateServer(object):
  """Client class to download or upload to Isolate Server."""
  def __init__(self, base_url, namespace):
    assert base_url.startswith('http'), base_url
    self.content_url = base_url.rstrip('/') + '/content/'
    self.namespace = namespace
    self._token = None
    self._lock = threading.Lock()

  @property
  def token(self):
    # TODO(maruel): Make this request much earlier asynchronously while the
    # files are being enumerated.
    with self._lock:
      if not self._token:
        self._token = urllib.quote(url_read(self.content_url + 'get_token'))
      return self._token

  def fetch(self, item, expected_size):
    """Fetches an object and yields its content."""
    assert isinstance(item, basestring)
    assert (
        isinstance(expected_size, (int, long)) or
        expected_size == UNKNOWN_FILE_SIZE)
    zipped_url = '%sretrieve/%s/%s' % (self.content_url, self.namespace, item)
    logging.debug('download_file(%s)', zipped_url)

    # Because the app engine DB is only eventually consistent, retry 404 errors
    # because the file might just not be visible yet (even though it has been
    # uploaded).
    connection = net.url_open(
        zipped_url, retry_404=True, read_timeout=DOWNLOAD_READ_TIMEOUT)
    if not connection:
      raise IOError('Unable to open connection to %s' % zipped_url)

    # TODO(maruel): Must only decompress when needed.
    decompressor = zlib.decompressobj()
    try:
      compressed_size = 0
      decompressed_size = 0
      while True:
        chunk = connection.read(ZIPPED_FILE_CHUNK)
        if not chunk:
          break
        compressed_size += len(chunk)
        decompressed = decompressor.decompress(chunk)
        decompressed_size += len(decompressed)
        yield decompressed

      # Ensure that all the data was properly decompressed.
      uncompressed_data = decompressor.flush()
      if uncompressed_data:
        raise IOError('Decompression failed')
      if (expected_size != UNKNOWN_FILE_SIZE and
          decompressed_size != expected_size):
        raise IOError('File incorrect size after download of %s. Got %s and '
                      'expected %s' % (item, decompressed_size, expected_size))
    except zlib.error as e:
      msg = 'Corrupted zlib for item %s. Processed %d of %s bytes.\n%s' % (
          item, compressed_size, connection.content_length, e)
      logging.warning(msg)

      # Testing seems to show that if a few machines are trying to download
      # the same blob, they can cause each other to fail. So if we hit a zip
      # error, this is the most likely cause (it only downloads some of the
      # data). Randomly sleep for between 5 and 25 seconds to try and spread
      # out the downloads.
      sleep_duration = (random.random() * 20) + 5
      time.sleep(sleep_duration)
      raise IOError(msg)

  def push(self, item, expected_size, content_generator):
    """Uploads content generated by |content_generator| as |item| to the remote
    isolate server.
    """
    assert isinstance(item, basestring)
    assert isinstance(expected_size, int) or expected_size == UNKNOWN_FILE_SIZE
    item = str(item)
    # TODO(maruel): Support large files. This would require streaming support.
    content = ''.join(content_generator)
    if len(content) > MIN_SIZE_FOR_DIRECT_BLOBSTORE:
      return self._upload_hash_content_to_blobstore(item, content)

    url = '%sstore/%s/%s?token=%s' % (
        self.content_url, self.namespace, item, self.token)
    return url_read(
        url, data=content, content_type='application/octet-stream')

  def _upload_hash_content_to_blobstore(self, item, content):
    """Uploads the content directly to the blobstore via a generated url."""
    # TODO(maruel): Support large files. This would require streaming support.
    gen_url = '%sgenerate_blobstore_url/%s/%s' % (
        self.content_url, self.namespace, item)
    # Token is guaranteed to be already quoted but it is unnecessary here, and
    # only here.
    data = [('token', urllib.unquote(self.token))]
    content_type, body = encode_multipart_formdata(
        data, [('content', item, content)])
    last_url = gen_url
    for _ in net.retry_loop(max_attempts=net.URL_OPEN_MAX_ATTEMPTS):
      # Retry HTTP 50x here but not 404.
      upload_url = net.url_read(gen_url, data=data)
      if not upload_url:
        raise MappingError('Unable to connect to server %s' % gen_url)
      last_url = upload_url

      # Do not retry this request on HTTP 50x. Regenerate an upload url each
      # time since uploading "consumes" the upload url.
      result = net.url_read(
          upload_url, data=body, content_type=content_type, retry_50x=False)
      if result is not None:
        return result
    raise MappingError('Unable to connect to server %s' % last_url)


def check_files_exist_on_server(query_url, queries):
  """Queries the server to see which files from this batch already exist there.

  Arguments:
    queries: The hash files to potential upload to the server.
  Returns:
    missing_files: list of files that are missing on the server.
  """
  # TODO(maruel): Move inside IsolateServer.
  logging.info('Checking existence of %d files...', len(queries))
  body = ''.join(
      (binascii.unhexlify(meta_data['h']) for (_, meta_data) in queries))
  assert (len(body) % 20) == 0, repr(body)

  response = url_read(
      query_url, data=body, content_type='application/octet-stream')
  if len(queries) != len(response):
    raise MappingError(
        'Got an incorrect number of responses from the server. Expected %d, '
        'but got %d' % (len(queries), len(response)))

  missing_files = [
    queries[i] for i, flag in enumerate(response) if flag == chr(0)
  ]
  logging.info('Queried %d files, %d cache hit',
               len(queries), len(queries) - len(missing_files))
  return missing_files


class FileSystem(object):
  """Fetches data from the file system.

  The common use case is a NFS/CIFS file server that is mounted locally that is
  used to fetch the file on a local partition.
  """
  def __init__(self, base_path):
    self.base_path = base_path

  def fetch(self, item, expected_size):
    assert isinstance(item, basestring)
    assert isinstance(expected_size, int) or expected_size == UNKNOWN_FILE_SIZE
    source = os.path.join(self.base_path, item)
    if (expected_size != UNKNOWN_FILE_SIZE and
        not is_valid_file(source, expected_size)):
      raise IOError('Invalid file %s' % item)
    return file_read(source)

  def push(self, item, expected_size, content_generator):
    assert isinstance(item, basestring)
    assert isinstance(expected_size, int) or expected_size == UNKNOWN_FILE_SIZE
    dest = os.path.join(self.base_path, item)
    total = file_write(dest, content_generator)
    if expected_size != UNKNOWN_FILE_SIZE and total != expected_size:
      os.remove(dest)
      raise IOError(
          'Invalid file %s, %d != %d' % (item, total, expected_size))


def get_storage_api(file_or_url, namespace):
  """Returns an object that implements .fetch() and .push()."""
  if re.match(r'^https?://.+$', file_or_url):
    return IsolateServer(file_or_url, namespace)
  else:
    return FileSystem(file_or_url)


class WorkerPool(threading_utils.AutoRetryThreadPool):
  """Thread pool that automatically retries on IOError and runs a preconfigured
  function.
  """
  # Initial and maximum number of worker threads.
  INITIAL_WORKERS = 2
  MAX_WORKERS = 16
  RETRIES = 5

  def __init__(self):
    super(WorkerPool, self).__init__(
        [IOError],
        self.RETRIES,
        self.INITIAL_WORKERS,
        self.MAX_WORKERS,
        0,
        'remote')


def compression_level(filename):
  """Given a filename calculates the ideal compression level to use."""
  file_ext = os.path.splitext(filename)[1].lower()
  # TODO(csharp): Profile to find what compression level works best.
  return 0 if file_ext in ALREADY_COMPRESSED_TYPES else 7


def read_and_compress(filepath, level):
  """Reads a file and returns its content gzip compressed."""
  compressor = zlib.compressobj(level)
  compressed_data = cStringIO.StringIO()
  with open(filepath, 'rb') as f:
    while True:
      chunk = f.read(ZIPPED_FILE_CHUNK)
      if not chunk:
        break
      compressed_data.write(compressor.compress(chunk))
  compressed_data.write(compressor.flush(zlib.Z_FINISH))
  value = compressed_data.getvalue()
  compressed_data.close()
  return value


def zip_and_trigger_upload(infile, metadata, add_item):
  # TODO(csharp): Fix crbug.com/150823 and enable the touched logic again.
  # if not metadata['T']:
  # TODO(maruel): Use a generator?
  compressed_data = read_and_compress(infile, compression_level(infile))
  priority = (
      WorkerPool.HIGH if metadata.get('priority', '1') == '0'
      else WorkerPool.MED)
  return add_item(priority, metadata['h'], [compressed_data])


def batch_files_for_check(infiles):
  """Splits list of files to check for existence on the server into batches.

  Each batch corresponds to a single 'exists?' query to the server.

  Yields:
    batches: list of batches, each batch is a list of files.
  """
  batch_count = 0
  batch_size_limit = ITEMS_PER_CONTAINS_QUERIES[0]
  next_queries = []
  items = ((k, v) for k, v in infiles.iteritems() if 's' in v)
  for relfile, metadata in sorted(items, key=lambda x: -x[1]['s']):
    next_queries.append((relfile, metadata))
    if len(next_queries) == batch_size_limit:
      yield next_queries
      next_queries = []
      batch_count += 1
      batch_size_limit = ITEMS_PER_CONTAINS_QUERIES[
          min(batch_count, len(ITEMS_PER_CONTAINS_QUERIES) - 1)]
  if next_queries:
    yield next_queries


def get_files_to_upload(contains_hash_url, infiles):
  """Yields files that are missing on the server."""
  with threading_utils.ThreadPool(1, 16, 0, prefix='get_files_to_upload') as tp:
    for files in batch_files_for_check(infiles):
      tp.add_task(0, check_files_exist_on_server, contains_hash_url, files)
    for missing_file in itertools.chain.from_iterable(tp.iter_results()):
      yield missing_file


def upload_tree(base_url, indir, infiles, namespace):
  """Uploads the given tree to the given url.

  Arguments:
    base_url:  The base url, it is assume that |base_url|/has/ can be used to
               query if an element was already uploaded, and |base_url|/store/
               can be used to upload a new element.
    indir:     Root directory the infiles are based in.
    infiles:   dict of files to upload files from |indir| to |base_url|.
    namespace: The namespace to use on the server.
  """
  logging.info('upload tree(base_url=%s, indir=%s, files=%d)' %
               (base_url, indir, len(infiles)))

  # Create a pool of workers to zip and upload any files missing from
  # the server.
  num_threads = threading_utils.num_processors()
  remote = get_storage_api(base_url, namespace)
  # TODO(maruel): There's three separate thread pools here, it is not very
  # efficient. remote_uploader and get_files_to_upload() should share the same
  # pool and control priorities accordingly.
  uploaded = []
  with WorkerPool() as remote_uploader:
    # Starts the zip and upload process for files that are missing
    # from the server.
    # TODO(maruel): Move .contains() to the API.
    contains_hash_url = '%scontains/%s?token=%s' % (
        remote.content_url, namespace, remote.token)

    def add_item(priority, item, content_generator):
      remote_uploader.add_task(
          priority, remote.push, item, UNKNOWN_FILE_SIZE, content_generator)

    with threading_utils.ThreadPool(
        min(2, num_threads), num_threads, 0, 'zip') as zipping_pool:
      for relfile, metadata in get_files_to_upload(contains_hash_url, infiles):
        infile = os.path.join(indir, relfile)
        zipping_pool.add_task(
            0, zip_and_trigger_upload, infile, metadata, add_item)
        uploaded.append((relfile, metadata))

      logging.info('Waiting for all files to finish zipping')
      zipping_pool.join()
    logging.info('All files zipped.')
    remote_uploader.join()
  logging.info('All files are uploaded')

  total = len(infiles)
  total_size = sum(metadata.get('s', 0) for metadata in infiles.itervalues())
  logging.info(
      'Total:      %6d, %9.1fkb',
      total,
      sum(m.get('s', 0) for m in infiles.itervalues()) / 1024.)
  cache_hit = set(infiles.iterkeys()) - set(x[0] for x in uploaded)
  cache_hit_size = sum(infiles[i].get('s', 0) for i in cache_hit)
  logging.info(
      'cache hit:  %6d, %9.1fkb, %6.2f%% files, %6.2f%% size',
      len(cache_hit),
      cache_hit_size / 1024.,
      len(cache_hit) * 100. / total,
      cache_hit_size * 100. / total_size if total_size else 0)
  cache_miss = uploaded
  cache_miss_size = sum(infiles[i[0]].get('s', 0) for i in cache_miss)
  logging.info(
      'cache miss: %6d, %9.1fkb, %6.2f%% files, %6.2f%% size',
      len(cache_miss),
      cache_miss_size / 1024.,
      len(cache_miss) * 100. / total,
      cache_miss_size * 100. / total_size if total_size else 0)
  return 0


class MemoryCache(object):
  """This class is intended to be usable everywhere the Cache class is.

  Instead of downloading to a cache, all files are kept in memory to be stored
  in the target directory directly.
  """

  def __init__(self, target_directory, pool, remote):
    self.target_directory = target_directory
    self.pool = pool
    self.remote = remote
    self._lock = threading.Lock()
    self._contents = {}

  def retrieve(self, priority, item, size):
    """Gets the requested file."""
    self.pool.add_task(priority, self._store, item, size)

  def wait_for(self, items):
    """Starts a loop that waits for at least one of |items| to be retrieved.

    Returns the first item retrieved.
    """
    with self._lock:
      # Flush items already present.
      for item in items:
        if item in self._contents:
          return item

    while True:
      downloaded = self.pool.get_one_result()
      if downloaded in items:
        return downloaded

  def path(self, item):
    return os.path.join(self.target_directory, item)

  def read(self, item):
    return self._contents[item]

  def _store(self, item, size):
    data = ''.join(self.remote.fetch(item, size))
    with self._lock:
      self._contents[item] = data
    return item

  def __enter__(self):
    return self

  def __exit__(self, _exc_type, _exec_value, _traceback):
    return False


def load_isolated(content, os_flavor, algo):
  """Verifies the .isolated file is valid and loads this object with the json
  data.
  """
  try:
    data = json.loads(content)
  except ValueError:
    raise ConfigError('Failed to parse: %s...' % content[:100])

  if not isinstance(data, dict):
    raise ConfigError('Expected dict, got %r' % data)

  for key, value in data.iteritems():
    if key == 'command':
      if not isinstance(value, list):
        raise ConfigError('Expected list, got %r' % value)
      if not value:
        raise ConfigError('Expected non-empty command')
      for subvalue in value:
        if not isinstance(subvalue, basestring):
          raise ConfigError('Expected string, got %r' % subvalue)

    elif key == 'files':
      if not isinstance(value, dict):
        raise ConfigError('Expected dict, got %r' % value)
      for subkey, subvalue in value.iteritems():
        if not isinstance(subkey, basestring):
          raise ConfigError('Expected string, got %r' % subkey)
        if not isinstance(subvalue, dict):
          raise ConfigError('Expected dict, got %r' % subvalue)
        for subsubkey, subsubvalue in subvalue.iteritems():
          if subsubkey == 'l':
            if not isinstance(subsubvalue, basestring):
              raise ConfigError('Expected string, got %r' % subsubvalue)
          elif subsubkey == 'm':
            if not isinstance(subsubvalue, int):
              raise ConfigError('Expected int, got %r' % subsubvalue)
          elif subsubkey == 'h':
            if not is_valid_hash(subsubvalue, algo):
              raise ConfigError('Expected sha-1, got %r' % subsubvalue)
          elif subsubkey == 's':
            if not isinstance(subsubvalue, int):
              raise ConfigError('Expected int, got %r' % subsubvalue)
          else:
            raise ConfigError('Unknown subsubkey %s' % subsubkey)
        if bool('h' in subvalue) and bool('l' in subvalue):
          raise ConfigError(
              'Did not expect both \'h\' (sha-1) and \'l\' (link), got: %r' %
              subvalue)

    elif key == 'includes':
      if not isinstance(value, list):
        raise ConfigError('Expected list, got %r' % value)
      if not value:
        raise ConfigError('Expected non-empty includes list')
      for subvalue in value:
        if not is_valid_hash(subvalue, algo):
          raise ConfigError('Expected sha-1, got %r' % subvalue)

    elif key == 'read_only':
      if not isinstance(value, bool):
        raise ConfigError('Expected bool, got %r' % value)

    elif key == 'relative_cwd':
      if not isinstance(value, basestring):
        raise ConfigError('Expected string, got %r' % value)

    elif key == 'os':
      if os_flavor and value != os_flavor:
        raise ConfigError(
            'Expected \'os\' to be \'%s\' but got \'%s\'' %
            (os_flavor, value))

    else:
      raise ConfigError('Unknown key %s' % key)

  return data


class IsolatedFile(object):
  """Represents a single parsed .isolated file."""
  def __init__(self, obj_hash, algo):
    """|obj_hash| is really the sha-1 of the file."""
    logging.debug('IsolatedFile(%s)' % obj_hash)
    self.obj_hash = obj_hash
    self.algo = algo
    # Set once all the left-side of the tree is parsed. 'Tree' here means the
    # .isolate and all the .isolated files recursively included by it with
    # 'includes' key. The order of each sha-1 in 'includes', each representing a
    # .isolated file in the hash table, is important, as the later ones are not
    # processed until the firsts are retrieved and read.
    self.can_fetch = False

    # Raw data.
    self.data = {}
    # A IsolatedFile instance, one per object in self.includes.
    self.children = []

    # Set once the .isolated file is loaded.
    self._is_parsed = False
    # Set once the files are fetched.
    self.files_fetched = False

  def load(self, content):
    """Verifies the .isolated file is valid and loads this object with the json
    data.
    """
    logging.debug('IsolatedFile.load(%s)' % self.obj_hash)
    assert not self._is_parsed
    self.data = load_isolated(content, None, self.algo)
    self.children = [
        IsolatedFile(i, self.algo) for i in self.data.get('includes', [])
    ]
    self._is_parsed = True

  def fetch_files(self, cache, files):
    """Adds files in this .isolated file not present in |files| dictionary.

    Preemptively request files.

    Note that |files| is modified by this function.
    """
    assert self.can_fetch
    if not self._is_parsed or self.files_fetched:
      return
    logging.debug('fetch_files(%s)' % self.obj_hash)
    for filepath, properties in self.data.get('files', {}).iteritems():
      # Root isolated has priority on the files being mapped. In particular,
      # overriden files must not be fetched.
      if filepath not in files:
        files[filepath] = properties
        if 'h' in properties:
          # Preemptively request files.
          logging.debug('fetching %s' % filepath)
          cache.retrieve(
              WorkerPool.MED,
              properties['h'],
              properties['s'])
    self.files_fetched = True


class Settings(object):
  """Results of a completely parsed .isolated file."""
  def __init__(self):
    self.command = []
    self.files = {}
    self.read_only = None
    self.relative_cwd = None
    # The main .isolated file, a IsolatedFile instance.
    self.root = None

  def load(self, cache, root_isolated_hash, algo):
    """Loads the .isolated and all the included .isolated asynchronously.

    It enables support for "included" .isolated files. They are processed in
    strict order but fetched asynchronously from the cache. This is important so
    that a file in an included .isolated file that is overridden by an embedding
    .isolated file is not fetched needlessly. The includes are fetched in one
    pass and the files are fetched as soon as all the ones on the left-side
    of the tree were fetched.

    The prioritization is very important here for nested .isolated files.
    'includes' have the highest priority and the algorithm is optimized for both
    deep and wide trees. A deep one is a long link of .isolated files referenced
    one at a time by one item in 'includes'. A wide one has a large number of
    'includes' in a single .isolated file. 'left' is defined as an included
    .isolated file earlier in the 'includes' list. So the order of the elements
    in 'includes' is important.
    """
    self.root = IsolatedFile(root_isolated_hash, algo)

    # Isolated files being retrieved now: hash -> IsolatedFile instance.
    pending = {}
    # Set of hashes of already retrieved items to refuse recursive includes.
    seen = set()

    def retrieve(isolated_file):
      h = isolated_file.obj_hash
      if h in seen:
        raise ConfigError('IsolatedFile %s is retrieved recursively' % h)
      assert h not in pending
      seen.add(h)
      pending[h] = isolated_file
      cache.retrieve(WorkerPool.HIGH, h, UNKNOWN_FILE_SIZE)

    retrieve(self.root)

    while pending:
      item_hash = cache.wait_for(pending)
      item = pending.pop(item_hash)
      item.load(cache.read(item_hash))
      if item_hash == root_isolated_hash:
        # It's the root item.
        item.can_fetch = True

      for new_child in item.children:
        retrieve(new_child)

      # Traverse the whole tree to see if files can now be fetched.
      self._traverse_tree(cache, self.root)

    def check(n):
      return all(check(x) for x in n.children) and n.files_fetched
    assert check(self.root)

    self.relative_cwd = self.relative_cwd or ''
    self.read_only = self.read_only or False

  def _traverse_tree(self, cache, node):
    if node.can_fetch:
      if not node.files_fetched:
        self._update_self(cache, node)
      will_break = False
      for i in node.children:
        if not i.can_fetch:
          if will_break:
            break
          # Automatically mark the first one as fetcheable.
          i.can_fetch = True
          will_break = True
        self._traverse_tree(cache, i)

  def _update_self(self, cache, node):
    node.fetch_files(cache, self.files)
    # Grabs properties.
    if not self.command and node.data.get('command'):
      self.command = node.data['command']
    if self.read_only is None and node.data.get('read_only') is not None:
      self.read_only = node.data['read_only']
    if (self.relative_cwd is None and
        node.data.get('relative_cwd') is not None):
      self.relative_cwd = node.data['relative_cwd']


@subcommand.usage('<file1..fileN> or - to read from stdin')
def CMDarchive(parser, args):
  """Archives data to the server."""
  options, files = parser.parse_args(args)

  if files == ['-']:
    files = sys.stdin.readlines()

  if not files:
    parser.error('Nothing to upload')
  if not options.isolate_server:
    parser.error('Nowhere to send. Please specify --isolate-server')

  # Load the necessary metadata. This is going to be rewritten eventually to be
  # more efficient.
  algo = hashlib.sha1
  infiles = dict(
      (
        f,
        {
          's': os.stat(f).st_size,
          'h': hash_file(f, algo),
        }
      )
      for f in files)

  with tools.Profiler('Archive'):
    ret = upload_tree(
        base_url=options.isolate_server,
        indir=os.getcwd(),
        infiles=infiles,
        namespace=options.namespace)
  if not ret:
    print '\n'.join('%s  %s' % (infiles[f]['h'], f) for f in sorted(infiles))
  return ret


def CMDdownload(parser, args):
  """Download data from the server.

  It can download individual files.
  """
  parser.add_option(
      '-f', '--file', metavar='HASH DEST', default=[], action='append', nargs=2,
      help='hash and destination of a file, can be used multiple times')
  parser.add_option(
      '-t', '--target', metavar='DIR', default=os.getcwd(),
      help='destination directory')
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported arguments: %s' % args)
  if not options.file:
    parser.error('Use one of --file is required.')

  options.target = os.path.abspath(options.target)
  remote = get_storage_api(options.isolate_server, options.namespace)
  for h, dest in options.file:
    logging.info('%s: %s', h, dest)
    file_write(
        os.path.join(options.target, dest),
        remote.fetch(h, UNKNOWN_FILE_SIZE))
  return 0


class OptionParserIsolateServer(tools.OptionParserWithLogging):
  def __init__(self, **kwargs):
    tools.OptionParserWithLogging.__init__(self, **kwargs)
    self.add_option(
        '-I', '--isolate-server',
        default=ISOLATE_SERVER,
        metavar='URL',
        help='Isolate server where data is stored. default: %default')
    self.add_option(
        '--namespace', default='default-gzip',
        help='The namespace to use on the server, default: %default')

  def parse_args(self, *args, **kwargs):
    options, args = tools.OptionParserWithLogging.parse_args(
        self, *args, **kwargs)
    options.isolate_server = options.isolate_server.rstrip('/')
    if not options.isolate_server:
      self.error('--isolate-server is required.')
    return options, args


def main(args):
  dispatcher = subcommand.CommandDispatcher(__name__)
  try:
    return dispatcher.execute(
        OptionParserIsolateServer(version=__version__), args)
  except (ConfigError, MappingError) as e:
    sys.stderr.write('\nError: ')
    sys.stderr.write(str(e))
    sys.stderr.write('\n')
    return 1


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  tools.disable_buffering()
  colorama.init()
  sys.exit(main(sys.argv[1:]))
