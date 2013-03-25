#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Archives a set of files to a server."""

import binascii
import hashlib
import logging
import optparse
import os
import cStringIO
import sys
import time
import zlib

import run_isolated
import run_test_cases


# The minimum size of files to upload directly to the blobstore.
MIN_SIZE_FOR_DIRECT_BLOBSTORE = 20 * 1024

# The number of files to check the isolate server for each query.
ITEMS_PER_CONTAINS_QUERY = 500

# A list of already compressed extension types that should not receive any
# compression before being uploaded.
ALREADY_COMPRESSED_TYPES = [
    '7z', 'avi', 'cur', 'gif', 'h264', 'jar', 'jpeg', 'jpg', 'pdf', 'png',
    'wav', 'zip'
]


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


def url_open(url, *args, **kwargs):
  result = run_isolated.url_open(url, *args, **kwargs)
  if not result:
    # If we get no response from the server, assume it is down and raise an
    # exception.
    raise run_isolated.MappingError('Unable to connect to server %s' % url)
  return result


def upload_hash_content_to_blobstore(
    generate_upload_url, data, hash_key, content):
  """Uploads the given hash contents directly to the blobsotre via a generated
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
  upload_url = url_open(generate_upload_url, data).read()

  if not upload_url:
    logging.error('Unable to generate upload url')
    return

  # TODO(maruel): Support large files.
  content_type, body = encode_multipart_formdata(
       data, [('hash_contents', hash_key, content)])
  return url_open(upload_url, body, content_type=content_type)


class UploadRemote(run_isolated.Remote):
  def __init__(self, namespace, base_url, token):
    self.namespace = str(namespace)
    self._token = token
    super(UploadRemote, self).__init__(base_url)

  def get_file_handler(self, base_url):
    base_url = str(base_url)
    def upload_file(content, hash_key):
      # TODO(maruel): Detect failures.
      hash_key = str(hash_key)
      content_url = base_url.rstrip('/') + '/content/'
      if len(content) > MIN_SIZE_FOR_DIRECT_BLOBSTORE:
        url = '%sgenerate_blobstore_url/%s/%s' % (
            content_url, self.namespace, hash_key)
        data = [('token', self._token)]
        upload_hash_content_to_blobstore(url, data, hash_key, content)
      else:
        url = '%sstore/%s/%s?token=%s' % (
            content_url, self.namespace, hash_key, self._token)
        url_open(url, content, content_type='application/octet-stream')
    return upload_file


def update_files_to_upload(query_url, queries, upload):
  """Queries the server to see which files from this batch already exist there.

  Arguments:
    queries: The hash files to potential upload to the server.
    upload: Any new files that need to be upload are sent to this function.
  """
  body = ''.join(
      (binascii.unhexlify(meta_data['h']) for (_, meta_data) in queries))
  assert (len(body) % 20) == 0, repr(body)

  response = url_open(
      query_url, body, content_type='application/octet-stream').read()
  if len(queries) != len(response):
    raise run_isolated.MappingError(
        'Got an incorrect number of responses from the server. Expected %d, '
        'but got %d' % (len(queries), len(response)))

  hit = 0
  for i in range(len(response)):
    if response[i] == chr(0):
      upload(queries[i])
    else:
      hit += 1
  logging.info('Queried %d files, %d cache hit', len(queries), hit)


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
      chunk = f.read(run_isolated.ZIPPED_FILE_CHUNK)
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
      run_isolated.Remote.HIGH if metadata.get('priority', '1') == '0'
      else run_isolated.Remote.MED)
  return upload_function(priority, compressed_data, metadata['h'], None)


def process_items(contains_hash_url, infiles, zip_and_upload):
  """Generates the list of files that need to be uploaded and send them to
  zip_and_upload.

  Some may already be on the server.
  """
  next_queries = []
  items = ((k, v) for k, v in infiles.iteritems() if 's' in v)
  for relfile, metadata in sorted(items, key=lambda x: -x[1]['s']):
    next_queries.append((relfile, metadata))
    if len(next_queries) == ITEMS_PER_CONTAINS_QUERY:
      update_files_to_upload(contains_hash_url, next_queries, zip_and_upload)
      next_queries = []
  if next_queries:
    update_files_to_upload(contains_hash_url, next_queries, zip_and_upload)


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
  assert base_url.startswith('http'), base_url
  base_url = base_url.rstrip('/')

  # TODO(maruel): Make this request much earlier asynchronously while the files
  # are being enumerated.
  token = url_open(base_url + '/content/get_token').read()

  # Create a pool of workers to zip and upload any files missing from
  # the server.
  num_threads = run_test_cases.num_processors()
  zipping_pool = run_isolated.ThreadPool(num_threads, num_threads, 0)
  remote_uploader = UploadRemote(namespace, base_url, token)

  # Starts the zip and upload process for a given query. The query is assumed
  # to be in the format (relfile, metadata).
  uploaded = []
  def zip_and_upload(query):
    relfile, metadata = query
    infile = os.path.join(indir, relfile)
    zipping_pool.add_task(0, zip_and_trigger_upload, infile, metadata,
                          remote_uploader.add_item)
    uploaded.append(query)

  contains_hash_url = '%s/content/contains/%s?token=%s' % (
      base_url, namespace, token)
  process_items(contains_hash_url, infiles, zip_and_upload)

  logging.info('Waiting for all files to finish zipping')
  zipping_pool.join()
  logging.info('All files zipped.')

  logging.info('Waiting for all files to finish uploading')
  # Will raise if any exception occurred.
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


def main(args):
  run_isolated.disable_buffering()
  parser = optparse.OptionParser(
      usage='%prog [options] <file1..fileN> or - to read from stdin',
      description=sys.modules[__name__].__doc__)
  parser.add_option('-r', '--remote', help='Remote server to archive to')
  parser.add_option(
        '-v', '--verbose',
        action='count', default=0,
        help='Use multiple times to increase verbosity')
  parser.add_option('--namespace', default='default-gzip',
                    help='The namespace to use on the server.')

  options, files = parser.parse_args(args)

  levels = [logging.ERROR, logging.INFO, logging.DEBUG]
  logging.basicConfig(
      level=levels[min(len(levels)-1, options.verbose)],
      format='%(levelname)5s %(module)15s(%(lineno)3d): %(message)s')
  if files == ['-']:
    files = sys.stdin.readlines()

  if not files:
    parser.error('Nothing to upload')
  if not options.remote:
    parser.error('Nowhere to send. Please specify --remote')

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

  with run_isolated.Profiler('Archive'):
    return upload_sha1_tree(
        base_url=options.remote,
        indir=os.getcwd(),
        infiles=infiles,
        namespace=options.namespace)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
