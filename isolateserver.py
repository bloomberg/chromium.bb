#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Archives a set of files to a server."""

__version__ = '0.2'

import binascii
import hashlib
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

# Maximum expected delay (in seconds) between successive file fetches
# in run_tha_test. If it takes longer than that, a deadlock might be happening
# and all stack frames for all threads are dumped to log.
DEADLOCK_TIMEOUT = 5 * 60


# The delay (in seconds) to wait between logging statements when retrieving
# the required files. This is intended to let the user (or buildbot) know that
# the program is still running.
DELAY_BETWEEN_UPDATES_IN_SECS = 30


# Sadly, hashlib uses 'sha1' instead of the standard 'sha-1' so explicitly
# specify the names here.
SUPPORTED_ALGOS = {
  'md5': hashlib.md5,
  'sha-1': hashlib.sha1,
  'sha-512': hashlib.sha512,
}


# Used for serialization.
SUPPORTED_ALGOS_REVERSE = dict((v, k) for k, v in SUPPORTED_ALGOS.iteritems())


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


def file_read(filepath, chunk_size=DISK_FILE_CHUNK):
  """Yields file content in chunks of given |chunk_size|."""
  with open(filepath, 'rb') as f:
    while True:
      data = f.read(chunk_size)
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


def zip_compress(content_generator, level=7):
  """Reads chunks from |content_generator| and yields zip compressed chunks."""
  compressor = zlib.compressobj(level)
  for chunk in content_generator:
    compressed = compressor.compress(chunk)
    if compressed:
      yield compressed
  tail = compressor.flush(zlib.Z_FINISH)
  if tail:
    yield tail


def get_zip_compression_level(filename):
  """Given a filename calculates the ideal zip compression level to use."""
  file_ext = os.path.splitext(filename)[1].lower()
  # TODO(csharp): Profile to find what compression level works best.
  return 0 if file_ext in ALREADY_COMPRESSED_TYPES else 7


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


class Storage(object):
  """Efficiently downloads or uploads large set of files via StorageApi."""

  def __init__(self, storage_api, use_zip):
    self.use_zip = use_zip
    self._storage_api = storage_api
    self._cpu_thread_pool = None
    self._net_thread_pool = None

  @property
  def cpu_thread_pool(self):
    """ThreadPool for CPU-bound tasks like zipping."""
    if self._cpu_thread_pool is None:
      self._cpu_thread_pool = threading_utils.ThreadPool(
          2, max(threading_utils.num_processors(), 2), 0, 'zip')
    return self._cpu_thread_pool

  @property
  def net_thread_pool(self):
    """AutoRetryThreadPool for IO-bound tasks, retries IOError."""
    if self._net_thread_pool is None:
      self._net_thread_pool = WorkerPool()
    return self._net_thread_pool

  def close(self):
    """Waits for all pending tasks to finish."""
    if self._cpu_thread_pool:
      self._cpu_thread_pool.join()
      self._cpu_thread_pool.close()
      self._cpu_thread_pool = None
    if self._net_thread_pool:
      self._net_thread_pool.join()
      self._net_thread_pool.close()
      self._net_thread_pool = None

  def __enter__(self):
    """Context manager interface."""
    return self

  def __exit__(self, _exc_type, _exc_value, _traceback):
    """Context manager interface."""
    self.close()
    return False

  def upload_tree(self, indir, infiles):
    """Uploads the given tree to the isolate server.

    Arguments:
      indir: root directory the infiles are based in.
      infiles: dict of files to upload from |indir|.
    """
    logging.info('upload tree(indir=%s, files=%d)', indir, len(infiles))

    # Enqueue all upload tasks.
    channel = threading_utils.TaskChannel()
    missing = []
    for filename, metadata, push_urls in self.get_missing_files(infiles):
      missing.append((filename, metadata))
      path = os.path.join(indir, filename)
      if metadata.get('priority', '1') == '0':
        priority = WorkerPool.HIGH
      else:
        priority = WorkerPool.MED
      compression_level = get_zip_compression_level(path)
      chunk_size = ZIPPED_FILE_CHUNK if self.use_zip else DISK_FILE_CHUNK
      content = file_read(path, chunk_size)
      self.async_push(channel, priority, metadata['h'], metadata['s'],
          content, compression_level, push_urls)

    # No need to spawn deadlock detector thread if there's nothing to upload.
    if missing:
      with threading_utils.DeadlockDetector(DEADLOCK_TIMEOUT) as detector:
        # Wait for all started uploads to finish.
        uploaded = 0
        while uploaded != len(missing):
          detector.ping()
          item = channel.pull()
          uploaded += 1
          logging.debug('Uploaded %d / %d: %s', uploaded, len(missing), item)
    logging.info('All files are uploaded')

    # Print stats.
    total = len(infiles)
    total_size = sum(metadata.get('s', 0) for metadata in infiles.itervalues())
    logging.info(
        'Total:      %6d, %9.1fkb',
        total,
        sum(m.get('s', 0) for m in infiles.itervalues()) / 1024.)
    cache_hit = set(infiles.iterkeys()) - set(x[0] for x in missing)
    cache_hit_size = sum(infiles[i].get('s', 0) for i in cache_hit)
    logging.info(
        'cache hit:  %6d, %9.1fkb, %6.2f%% files, %6.2f%% size',
        len(cache_hit),
        cache_hit_size / 1024.,
        len(cache_hit) * 100. / total,
        cache_hit_size * 100. / total_size if total_size else 0)
    cache_miss = missing
    cache_miss_size = sum(infiles[i[0]].get('s', 0) for i in cache_miss)
    logging.info(
        'cache miss: %6d, %9.1fkb, %6.2f%% files, %6.2f%% size',
        len(cache_miss),
        cache_miss_size / 1024.,
        len(cache_miss) * 100. / total,
        cache_miss_size * 100. / total_size if total_size else 0)

  def async_push(self, channel, priority, item, expected_size,
                 content_generator, compression_level, push_urls=None):
    """Starts asynchronous push to the server in a parallel thread."""
    def push(content, size):
      """Pushes an item and returns its id, to pass as a result to |channel|."""
      self._storage_api.push(item, size, content, push_urls)
      return item

    # If zipping is not required, just start a push task.
    if not self.use_zip:
      self.net_thread_pool.add_task_with_channel(channel, priority, push,
          content_generator, expected_size)
      return

    # If zipping is enabled, zip in a separate thread.
    def zip_and_push():
      # TODO(vadimsh): Implement streaming uploads. Before it's done, assemble
      # content right here. It will block until all file is zipped.
      try:
        stream = zip_compress(content_generator, compression_level)
        data = ''.join(stream)
      except Exception as exc:
        logging.error('Failed to zip \'%s\': %s', item, exc)
        channel.send_exception(exc)
        return
      self.net_thread_pool.add_task_with_channel(channel, priority, push,
          [data], UNKNOWN_FILE_SIZE)
    self.cpu_thread_pool.add_task(0, zip_and_push)

  def get_missing_files(self, files):
    """Yields files that are missing from the server.

    Issues multiple parallel queries via StorageApi's 'contains' method.

    Arguments:
      files: a dictionary file name -> metadata dict.

    Yields:
      Triplets (file name, metadata dict, push_urls object to pass to push).
    """
    channel = threading_utils.TaskChannel()
    pending = 0
    # Enqueue all requests.
    for batch in self.batch_files_for_check(files):
      self.net_thread_pool.add_task_with_channel(channel, WorkerPool.HIGH,
          self._storage_api.contains, batch)
      pending += 1
    # Yield results as they come in.
    for _ in xrange(pending):
      for missing in channel.pull():
        yield missing

  @staticmethod
  def batch_files_for_check(files):
    """Splits list of files to check for existence on the server into batches.

    Each batch corresponds to a single 'exists?' query to the server via a call
    to StorageApi's 'contains' method.

    Arguments:
      files: a dictionary file name -> metadata dict.

    Yields:
      Batches of files to query for existence in a single operation,
      each batch is a list of pairs: (file name, metadata dict).
    """
    batch_count = 0
    batch_size_limit = ITEMS_PER_CONTAINS_QUERIES[0]
    next_queries = []
    items = ((k, v) for k, v in files.iteritems() if 's' in v)
    for filename, metadata in sorted(items, key=lambda x: -x[1]['s']):
      next_queries.append((filename, metadata))
      if len(next_queries) == batch_size_limit:
        yield next_queries
        next_queries = []
        batch_count += 1
        batch_size_limit = ITEMS_PER_CONTAINS_QUERIES[
            min(batch_count, len(ITEMS_PER_CONTAINS_QUERIES) - 1)]
    if next_queries:
      yield next_queries


class StorageApi(object):
  """Interface for classes that implement low-level storage operations."""

  def fetch(self, item, expected_size):
    """Fetches an object and yields its content.

    Arguments:
      item: hash digest of item to download.
      expected_size: expected size of the item, to validate it.

    Yields:
      Chunks of downloaded item (as str objects).
    """
    raise NotImplementedError()

  def push(self, item, expected_size, content_generator, push_urls=None):
    """Uploads content generated by |content_generator| as |item|.

    Arguments:
      item: hash digest of item to upload.
      expected_size: total length of the content yielded by |content_generator|.
      content_generator: generator that produces chunks to push.
      push_urls: optional URLs returned by 'contains' call for this item.

    Returns:
      None.
    """
    raise NotImplementedError()

  def contains(self, files):
    """Checks for existence of given |files| on the server.

    Arguments:
      files: list of pairs (file name, metadata dict).

    Returns:
      A list of files missing on server as a list of triplets
        (file name, metadata dict, push_urls object to pass to push).
    """
    raise NotImplementedError()


class IsolateServer(StorageApi):
  """StorageApi implementation that downloads and uploads to Isolate Server."""
  def __init__(self, base_url, namespace):
    super(IsolateServer, self).__init__()
    assert base_url.startswith('http'), base_url
    self.content_url = base_url.rstrip('/') + '/content/'
    self.namespace = namespace
    self.algo = get_hash_algo(namespace)
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

  def push(self, item, expected_size, content_generator, push_urls=None):
    assert isinstance(item, basestring)
    assert isinstance(expected_size, int) or expected_size == UNKNOWN_FILE_SIZE
    item = str(item)

    # TODO(maruel): Support large files. This would require streaming support.

    # A cheese way to avoid memcpy of (possibly huge) file, until streaming
    # upload support is implemented.
    if isinstance(content_generator, list) and len(content_generator) == 1:
      content = content_generator[0]
    else:
      content = ''.join(content_generator)

    if len(content) > MIN_SIZE_FOR_DIRECT_BLOBSTORE:
      return self._upload_hash_content_to_blobstore(item, content)

    url = '%sstore/%s/%s?token=%s' % (
        self.content_url, self.namespace, item, self.token)
    return url_read(url, data=content, content_type='application/octet-stream')

  def contains(self, files):
    logging.info('Checking existence of %d files...', len(files))

    body = ''.join(
        (binascii.unhexlify(metadata['h']) for (_, metadata) in files))
    assert (len(body) % self.algo().digest_size) == 0, repr(body)

    query_url = '%scontains/%s?token=%s' % (
        self.content_url, self.namespace, self.token)
    response = url_read(
        query_url, data=body, content_type='application/octet-stream')
    if len(files) != len(response):
      raise MappingError(
          'Got an incorrect number of responses from the server. Expected %d, '
          'but got %d' % (len(files), len(response)))

    # This implementation of IsolateServer doesn't use push_urls field,
    # set it to None.
    missing_files = [
        files[i] + (None,) for i, flag in enumerate(response) if flag == '\x00'
    ]
    logging.info('Queried %d files, %d cache hit',
        len(files), len(files) - len(missing_files))
    return missing_files

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


class FileSystem(StorageApi):
  """StorageApi implementation that fetches data from the file system.

  The common use case is a NFS/CIFS file server that is mounted locally that is
  used to fetch the file on a local partition.
  """
  def __init__(self, base_path):
    super(FileSystem, self).__init__()
    self.base_path = base_path

  def fetch(self, item, expected_size):
    assert isinstance(item, basestring)
    assert isinstance(expected_size, int) or expected_size == UNKNOWN_FILE_SIZE
    source = os.path.join(self.base_path, item)
    if (expected_size != UNKNOWN_FILE_SIZE and
        not is_valid_file(source, expected_size)):
      raise IOError('Invalid file %s' % item)
    return file_read(source)

  def push(self, item, expected_size, content_generator, push_urls=None):
    assert isinstance(item, basestring)
    assert isinstance(expected_size, int) or expected_size == UNKNOWN_FILE_SIZE
    dest = os.path.join(self.base_path, item)
    total = file_write(dest, content_generator)
    if expected_size != UNKNOWN_FILE_SIZE and total != expected_size:
      os.remove(dest)
      raise IOError(
          'Invalid file %s, %d != %d' % (item, total, expected_size))

  def contains(self, files):
    return [
        (filename, metadata, None)
        for filename, metadata in files
        if not os.path.exists(os.path.join(self.base_path, metadata['h']))
    ]


def get_hash_algo(_namespace):
  """Return hash algorithm class to use when uploading to given |namespace|."""
  # TODO(vadimsh): Implement this at some point.
  return hashlib.sha1


def is_namespace_with_compression(namespace):
  """Returns True if given |namespace| stores compressed objects."""
  return namespace.endswith(('-gzip', '-deflate'))


def get_storage_api(file_or_url, namespace):
  """Returns an object that implements StorageApi interface."""
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
  remote = get_storage_api(base_url, namespace)
  with Storage(remote, is_namespace_with_compression(namespace)) as storage:
    storage.upload_tree(indir, infiles)
  return 0


class MemoryCache(object):
  """This class is intended to be usable everywhere the Cache class is.

  Instead of downloading to a cache, all files are kept in memory to be stored
  in the target directory directly.
  """

  def __init__(self, remote):
    self.remote = remote
    self._pool = None
    self._lock = threading.Lock()
    self._contents = {}

  def set_pool(self, pool):
    self._pool = pool

  def retrieve(self, priority, item, size):
    """Gets the requested file."""
    self._pool.add_task(priority, self._on_content, item, size)

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
      downloaded = self._pool.get_one_result()
      if downloaded in items:
        return downloaded

  def add(self, filepath, item):
    with self._lock:
      with open(filepath, 'rb') as f:
        self._contents[item] = f.read()

  def read(self, item):
    return self._contents[item]

  def store_to(self, item, dest):
    file_write(dest, [self._contents[item]])

  def _on_content(self, item, size):
    data = ''.join(self.remote.fetch(item, size))
    with self._lock:
      self._contents[item] = data
    return item

  def __enter__(self):
    return self

  def __exit__(self, _exc_type, _exec_value, _traceback):
    with self._lock:
      self._contents = {}
    return False


def load_isolated(content, os_flavor, algo):
  """Verifies the .isolated file is valid and loads this object with the json
  data.

  Arguments:
  - content: raw serialized content to load.
  - os_flavor: OS to load this file on. Optional.
  - algo: hashlib algorithm class. Used to confirm the algorithm matches the
          algorithm used on the Isolate Server.
  """
  try:
    data = json.loads(content)
  except ValueError:
    raise ConfigError('Failed to parse: %s...' % content[:100])

  if not isinstance(data, dict):
    raise ConfigError('Expected dict, got %r' % data)

  # Check 'version' first, since it could modify the parsing after.
  value = data.get('version', '1.0')
  if not isinstance(value, basestring):
    raise ConfigError('Expected string, got %r' % value)
  if not re.match(r'^(\d+)\.(\d+)$', value):
    raise ConfigError('Expected a compatible version, got %r' % value)
  if value.split('.', 1)[0] != '1':
    raise ConfigError('Expected compatible \'1.x\' version, got %r' % value)

  if algo is None:
    # Default the algorithm used in the .isolated file itself, falls back to
    # 'sha-1' if unspecified.
    algo = SUPPORTED_ALGOS_REVERSE[data.get('algo', 'sha-1')]

  for key, value in data.iteritems():
    if key == 'algo':
      if not isinstance(value, basestring):
        raise ConfigError('Expected string, got %r' % value)
      if value not in SUPPORTED_ALGOS:
        raise ConfigError(
            'Expected one of \'%s\', got %r' %
            (', '.join(sorted(SUPPORTED_ALGOS)), value))
      if value != SUPPORTED_ALGOS_REVERSE[algo]:
        raise ConfigError(
            'Expected \'%s\', got %r' % (SUPPORTED_ALGOS_REVERSE[algo], value))

    elif key == 'command':
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
        if bool('h' in subvalue) == bool('l' in subvalue):
          raise ConfigError(
              'Need only one of \'h\' (sha-1) or \'l\' (link), got: %r' %
              subvalue)
        if bool('h' in subvalue) != bool('s' in subvalue):
          raise ConfigError(
              'Both \'h\' (sha-1) and \'s\' (size) should be set, got: %r' %
              subvalue)
        if bool('s' in subvalue) == bool('l' in subvalue):
          raise ConfigError(
              'Need only one of \'s\' (size) or \'l\' (link), got: %r' %
              subvalue)
        if bool('l' in subvalue) and bool('m' in subvalue):
          raise ConfigError(
              'Cannot use \'m\' (mode) and \'l\' (link), got: %r' %
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

    elif key == 'version':
      # Already checked above.
      pass

    else:
      raise ConfigError('Unknown key %r' % key)

  # Automatically fix os.path.sep if necessary. While .isolated files are always
  # in the the native path format, someone could want to download an .isolated
  # tree from another OS.
  wrong_path_sep = '/' if os.path.sep == '\\' else '\\'
  if 'files' in data:
    data['files'] = dict(
        (k.replace(wrong_path_sep, os.path.sep), v)
        for k, v in data['files'].iteritems())
    for v in data['files'].itervalues():
      if 'l' in v:
        v['l'] = v['l'].replace(wrong_path_sep, os.path.sep)
  if 'relative_cwd' in data:
    data['relative_cwd'] = data['relative_cwd'].replace(
        wrong_path_sep, os.path.sep)
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

  def load(self, os_flavor, content):
    """Verifies the .isolated file is valid and loads this object with the json
    data.
    """
    logging.debug('IsolatedFile.load(%s)' % self.obj_hash)
    assert not self._is_parsed
    self.data = load_isolated(content, os_flavor, self.algo)
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

  def load(self, cache, root_isolated_hash, os_flavor, algo):
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
      item.load(os_flavor, cache.read(item_hash))
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
      # Ensure paths are correctly separated on windows.
      self.command = node.data['command']
      if self.command:
        self.command[0] = self.command[0].replace('/', os.path.sep)
        self.command = tools.fix_python_path(self.command)
    if self.read_only is None and node.data.get('read_only') is not None:
      self.read_only = node.data['read_only']
    if (self.relative_cwd is None and
        node.data.get('relative_cwd') is not None):
      self.relative_cwd = node.data['relative_cwd']


def fetch_isolated(
    isolated_hash, cache, outdir, os_flavor, algo, require_command):
  """Aggressively downloads the .isolated file(s), then download all the files.
  """
  settings = Settings()
  with WorkerPool() as pool:
    with cache:
      cache.set_pool(pool)
      with tools.Profiler('GetIsolateds'):
        # Optionally support local files.
        if not is_valid_hash(isolated_hash, algo):
          # Adds it in the cache. While not strictly necessary, this
          # simplifies the rest.
          h = hash_file(isolated_hash, algo)
          cache.add(isolated_hash, h)
          isolated_hash = h
        settings.load(cache, isolated_hash, os_flavor, algo)

      if require_command and not settings.command:
        raise ConfigError('No command to run')

      with tools.Profiler('GetRest'):
        create_directories(outdir, settings.files)
        create_links(outdir, settings.files.iteritems())
        remaining = generate_remaining_files(settings.files.iteritems())

        cwd = os.path.normpath(os.path.join(outdir, settings.relative_cwd))
        if not os.path.isdir(cwd):
          os.makedirs(cwd)

        # Now block on the remaining files to be downloaded and mapped.
        logging.info('Retrieving remaining files')
        last_update = time.time()
        with threading_utils.DeadlockDetector(DEADLOCK_TIMEOUT) as detector:
          while remaining:
            detector.ping()
            obj = cache.wait_for(remaining)
            for filepath, properties in remaining.pop(obj):
              outfile = os.path.join(outdir, filepath)
              cache.store_to(obj, outfile)
              if 'm' in properties:
                # It's not set on Windows.
                os.chmod(outfile, properties['m'])

            duration = time.time() - last_update
            if duration > DELAY_BETWEEN_UPDATES_IN_SECS:
              msg = '%d files remaining...' % len(remaining)
              print msg
              logging.info(msg)
              last_update = time.time()
  return settings


def download_isolated_tree(isolated_hash, target_directory, remote):
  """Downloads the dependencies to the given directory."""
  if not os.path.exists(target_directory):
    os.makedirs(target_directory)

  cache = MemoryCache(remote)
  return fetch_isolated(
      isolated_hash, cache, target_directory, None, remote.algo, False)


@subcommand.usage('<file1..fileN> or - to read from stdin')
def CMDarchive(parser, args):
  """Archives data to the server."""
  options, files = parser.parse_args(args)

  if files == ['-']:
    files = sys.stdin.readlines()

  if not files:
    parser.error('Nothing to upload')

  # Load the necessary metadata.
  # TODO(maruel): Use a worker pool to upload as the hashing is being done.
  infiles = dict(
      (
        f,
        {
          's': os.stat(f).st_size,
          'h': hash_file(f, get_hash_algo(options.namespace)),
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

  It can either download individual files or a complete tree from a .isolated
  file.
  """
  parser.add_option(
      '-i', '--isolated', metavar='HASH',
      help='hash of an isolated file, .isolated file content is discarded, use '
           '--file if you need it')
  parser.add_option(
      '-f', '--file', metavar='HASH DEST', default=[], action='append', nargs=2,
      help='hash and destination of a file, can be used multiple times')
  parser.add_option(
      '-t', '--target', metavar='DIR', default=os.getcwd(),
      help='destination directory')
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported arguments: %s' % args)
  if bool(options.isolated) == bool(options.file):
    parser.error('Use one of --isolated or --file, and only one.')

  options.target = os.path.abspath(options.target)
  remote = get_storage_api(options.isolate_server, options.namespace)
  for h, dest in options.file:
    logging.info('%s: %s', h, dest)
    file_write(
        os.path.join(options.target, dest),
        remote.fetch(h, UNKNOWN_FILE_SIZE))
  if options.isolated:
    settings = download_isolated_tree(options.isolated, options.target, remote)
    rel = os.path.join(options.target, settings.relative_cwd)
    print('To run this test please run from the directory %s:' %
          os.path.join(options.target, rel))
    print('  ' + ' '.join(settings.command))
  return 0


class OptionParserIsolateServer(tools.OptionParserWithLogging):
  def __init__(self, **kwargs):
    tools.OptionParserWithLogging.__init__(self, **kwargs)
    self.add_option(
        '-I', '--isolate-server',
        metavar='URL', default='',
        help='Isolate server to use')
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
