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
