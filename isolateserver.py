#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Archives a set of files to a server."""

__version__ = '0.1.1'

import binascii
import cStringIO
import hashlib
import itertools
import logging
import os
import Queue
import sys
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


def sha1_file(filepath):
  """Calculates the SHA-1 of a file without reading it all in memory at once."""
  digest = hashlib.sha1()
  with open(filepath, 'rb') as f:
    while True:
      # Read in 1mb chunks.
      chunk = f.read(1024*1024)
      if not chunk:
        break
      digest.update(chunk)
  return digest.hexdigest()


def valid_file(filepath, size):
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


def url_read(url, **kwargs):
  result = net.url_read(url, **kwargs)
  if result is None:
    # If we get no response from the server, assume it is down and raise an
    # exception.
    raise MappingError('Unable to connect to server %s' % url)
  return result


def upload_hash_content_to_blobstore(
    generate_upload_url, data, hash_key, content):
  """Uploads the given hash contents directly to the blobstore via a generated
  url.

  Arguments:
    generate_upload_url: The url to get the new upload url from.
    data: extra POST data.
    hash_key: sha1 of the uncompressed version of content.
    content: The contents to upload. Must fit in memory for now.
  """
  logging.debug('Generating url to directly upload file to blobstore')
  assert isinstance(hash_key, str), hash_key
  assert isinstance(content, str), (hash_key, content)
  # TODO(maruel): Support large files. This would require streaming support.
  content_type, body = encode_multipart_formdata(
      data, [('content', hash_key, content)])
  for _ in net.retry_loop(max_attempts=net.URL_OPEN_MAX_ATTEMPTS):
    # Retry HTTP 50x here.
    upload_url = net.url_read(generate_upload_url, data=data)
    if not upload_url:
      raise MappingError(
          'Unable to connect to server %s' % generate_upload_url)

    # Do not retry this request on HTTP 50x. Regenerate an upload url each time
    # since uploading "consumes" the upload url.
    result = net.url_read(
        upload_url, data=body, content_type=content_type, retry_50x=False)
    if result is not None:
      return result
  raise MappingError(
      'Unable to connect to server %s' % generate_upload_url)


class IsolateServer(object):
  def __init__(self, base_url, namespace):
    assert base_url.startswith('http'), base_url
    self.content_url = base_url.rstrip('/') + '/content/'
    self.namespace = namespace
    # TODO(maruel): Make this request much earlier asynchronously while the
    # files are being enumerated.
    self.token = urllib.quote(url_read(self.content_url + 'get_token'))

  def store(self, content, hash_key):
    # TODO(maruel): Detect failures.
    hash_key = str(hash_key)
    if len(content) > MIN_SIZE_FOR_DIRECT_BLOBSTORE:
      url = '%sgenerate_blobstore_url/%s/%s' % (
          self.content_url, self.namespace, hash_key)
      # token is guaranteed to be already quoted but it is unnecessary here, and
      # only here.
      data = [('token', urllib.unquote(self.token))]
      return upload_hash_content_to_blobstore(url, data, hash_key, content)
    else:
      url = '%sstore/%s/%s?token=%s' % (
          self.content_url, self.namespace, hash_key, self.token)
      return url_read(
          url, data=content, content_type='application/octet-stream')


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


class RemoteOperation(object):
  """Priority based worker queue to operate on action items.

  It execute a function with the given task items. It is specialized to download
  files.

  When the priority of items is equals, works in strict FIFO mode.
  """
  # Initial and maximum number of worker threads.
  INITIAL_WORKERS = 2
  MAX_WORKERS = 16
  # Priorities.
  LOW, MED, HIGH = (1<<8, 2<<8, 3<<8)
  INTERNAL_PRIORITY_BITS = (1<<8) - 1
  RETRIES = 5

  def __init__(self, do_item):
    # Function to fetch a remote object or upload to a remote location.
    self._do_item = do_item
    # Contains tuple(priority, obj).
    self._done = Queue.PriorityQueue()
    self._pool = threading_utils.ThreadPool(
        self.INITIAL_WORKERS, self.MAX_WORKERS, 0, 'remote')

  def join(self):
    """Blocks until the queue is empty."""
    return self._pool.join()

  def close(self):
    """Terminates all worker threads."""
    self._pool.close()

  def add_item(self, priority, obj, dest, size):
    """Retrieves an object from the remote data store.

    The smaller |priority| gets fetched first.

    Thread-safe.
    """
    assert (priority & self.INTERNAL_PRIORITY_BITS) == 0
    return self._add_item(priority, obj, dest, size)

  def _add_item(self, priority, obj, dest, size):
    assert isinstance(obj, basestring), obj
    assert isinstance(dest, basestring), dest
    assert size is None or isinstance(size, int), size
    return self._pool.add_task(
        priority, self._task_executer, priority, obj, dest, size)

  def get_one_result(self):
    return self._pool.get_one_result()

  def _task_executer(self, priority, obj, dest, size):
    """Wraps self._do_item to trap and retry on IOError exceptions."""
    try:
      self._do_item(obj, dest)
      if size and not valid_file(dest, size):
        download_size = os.stat(dest).st_size
        os.remove(dest)
        raise IOError('File incorrect size after download of %s. Got %s and '
                      'expected %s' % (obj, download_size, size))
      # TODO(maruel): Technically, we'd want to have an output queue to be a
      # PriorityQueue.
      return obj
    except IOError as e:
      logging.debug('Caught IOError: %s', e)
      # Remove unfinished download.
      if os.path.exists(dest):
        os.remove(dest)
      # Retry a few times, lowering the priority.
      if (priority & self.INTERNAL_PRIORITY_BITS) < self.RETRIES:
        self._add_item(priority + 1, obj, dest, size)
        return
      raise


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


def zip_and_trigger_upload(infile, metadata, upload_function):
  # TODO(csharp): Fix crbug.com/150823 and enable the touched logic again.
  # if not metadata['T']:
  compressed_data = read_and_compress(infile, compression_level(infile))
  priority = (
      RemoteOperation.HIGH if metadata.get('priority', '1') == '0'
      else RemoteOperation.MED)
  return upload_function(priority, compressed_data, metadata['h'], None)


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


def upload_sha1_tree(base_url, indir, infiles, namespace):
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
  zipping_pool = threading_utils.ThreadPool(min(2, num_threads),
                                            num_threads, 0, 'zip')
  remote = IsolateServer(base_url, namespace)
  remote_uploader = RemoteOperation(remote.store)

  # Starts the zip and upload process for files that are missing
  # from the server.
  contains_hash_url = '%scontains/%s?token=%s' % (
      remote.content_url, namespace, remote.token)
  uploaded = []
  for relfile, metadata in get_files_to_upload(contains_hash_url, infiles):
    infile = os.path.join(indir, relfile)
    zipping_pool.add_task(0, zip_and_trigger_upload, infile, metadata,
                          remote_uploader.add_item)
    uploaded.append((relfile, metadata))

  logging.info('Waiting for all files to finish zipping')
  zipping_pool.join()
  zipping_pool.close()
  logging.info('All files zipped.')

  logging.info('Waiting for all files to finish uploading')
  # Will raise if any exception occurred.
  remote_uploader.join()
  remote_uploader.close()
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
  infiles = dict(
      (
        f,
        {
          's': os.stat(f).st_size,
          'h': sha1_file(f),
        }
      )
      for f in files)

  with tools.Profiler('Archive'):
    return upload_sha1_tree(
        base_url=options.isolate_server,
        indir=os.getcwd(),
        infiles=infiles,
        namespace=options.namespace)
  return 0


def CMDdownload(parser, args):
  """Download data from the server."""
  _options, args = parser.parse_args(args)
  parser.error('Sorry, it\'s not really supported.')
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
        help='The namespace to use on the server.')

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
