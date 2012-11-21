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
import sys
import time
import urllib2

import run_isolated


# The maximum number of upload attempts to try when uploading a single file.
MAX_UPLOAD_ATTEMPTS = 5

# The minimum size of files to upload directly to the blobstore.
MIN_SIZE_FOR_DIRECT_BLOBSTORE = 20 * 8


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
  boundary = hashlib.md5(str(time.time())).hexdigest()
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


def gen_url_request(url, payload, content_type='application/octet-stream'):
  """Returns a POST request."""
  request = urllib2.Request(url, data=payload)
  if payload is not None:
    request.add_header('Content-Type', content_type)
    request.add_header('Content-Length', len(payload))
  return request


def url_open(url, data, content_type='application/octet-stream'):
  """Opens the given url with the given data, repeating up to
  MAX_UPLOAD_ATTEMPTS times if it encounters an error.

  Arguments:
    url: The url to open.
    data: The data to send to the url.

  Returns:
    The response from the url, or it raises an exception it it failed to get
    a response.
  """
  request = gen_url_request(url, data, content_type)
  last_error = None
  for i in range(MAX_UPLOAD_ATTEMPTS):
    try:
      return urllib2.urlopen(request)
    except urllib2.URLError as e:
      last_error = e
      logging.warning('Unable to connect to %s, error msg: %s', url, e)
      time.sleep(0.5 + i)

  # If we get no response from the server after max_retries, assume it
  # is down and raise an exception
  raise run_isolated.MappingError(
      'Unable to connect to server, %s, to see which files are presents: %s' %
        (url, last_error))


def upload_hash_content_to_blobstore(generate_upload_url, content):
  """Uploads the given hash contents directly to the blobsotre via a generated
  url.

  Arguments:
    generate_upload_url: The url to get the new upload url from.
    hash_contents: The contents to upload.
  """
  logging.debug('Generating url to directly upload file to blobstore')
  upload_url = url_open(generate_upload_url, None).read()

  if not upload_url:
    logging.error('Unable to generate upload url')
    return

  content_type, body = encode_multipart_formdata(
      [], [('hash_contents', 'hash_content', content)])
  url_open(upload_url, body, content_type)


class UploadRemote(run_isolated.Remote):
  @staticmethod
  def get_file_handler(base_url):
    def upload_file(content, hash_key):
      content_url = base_url.rstrip('/') + '/content/'
      namespace = 'default'
      if len(content) > MIN_SIZE_FOR_DIRECT_BLOBSTORE:
        upload_hash_content_to_blobstore(
            content_url + 'generate_blobstore_url/' + namespace + '/' +
              hash_key,
            content)
      else:
        url_open(content_url + 'store/' + namespace + '/' + hash_key, content)
    return upload_file


def update_files_to_upload(query_url, queries, files_to_upload):
  """Queries the server to see which files from this batch already exist there.

  Arguments:
    queries: The hash files to potential upload to the server.
    files_to_upload: Any new files that need to be upload are added to
                     this list.
  """
  body = ''.join(
      (binascii.unhexlify(meta_data['h']) for (_, meta_data) in queries))
  assert (len(body) % 20) == 0, repr(body)

  response = url_open(query_url, body).read()
  if len(queries) != len(response):
    raise run_isolated.MappingError(
        'Got an incorrect number of responses from the server. Expected %d, '
        'but got %d' % (len(queries), len(response)))

  hit = 0
  for i in range(len(response)):
    if response[i] == chr(0):
      files_to_upload.append(queries[i])
    else:
      hit += 1
  logging.info('Queried %d files, %d cache hit', len(queries), hit)


def upload_sha1_tree(base_url, indir, infiles):
  """Uploads the given tree to the given url.

  Arguments:
    base_url: The base url, it is assume that |base_url|/has/ can be used to
              query if an element was already uploaded, and |base_url|/store/
              can be used to upload a new element.
    indir:    Root directory the infiles are based in.
    infiles:  dict of files to map from |indir| to |outdir|.
  """
  logging.info('upload tree(base_url=%s, indir=%s, files=%d)' %
               (base_url, indir, len(infiles)))

  # Generate the list of files that need to be uploaded (since some may already
  # be on the server.
  base_url = base_url.rstrip('/')
  contains_hash_url = base_url + '/content/contains/default'
  to_upload = []
  next_queries = []
  for relfile, metadata in infiles.iteritems():
    if 'l' in metadata:
      # Skip links when uploading.
      continue

    next_queries.append((relfile, metadata))
    if len(next_queries) == 1000:
      update_files_to_upload(contains_hash_url, next_queries, to_upload)
      next_queries = []

  if next_queries:
    update_files_to_upload(contains_hash_url, next_queries, to_upload)


  # Upload the required files.
  remote_uploader = UploadRemote(base_url)
  for relfile, metadata in to_upload:
    # TODO(csharp): Fix crbug.com/150823 and enable the touched logic again.
    # if metadata.get('T') == True:
    #   hash_data = ''
    infile = os.path.join(indir, relfile)
    with open(infile, 'rb') as f:
      hash_data = f.read()
    priority = (run_isolated.Remote.HIGH if metadata.get('priority', '1') == '0'
                else run_isolated.Remote.MED)
    remote_uploader.add_item(priority, hash_data, metadata['h'], None)
  remote_uploader.join()

  exception = remote_uploader.next_exception()
  if exception:
    raise exception[0], exception[1], exception[2]
  total = len(infiles)
  total_size = sum(metadata.get('s', 0) for metadata in infiles.itervalues())
  logging.info(
      'Total:      %6d, %9.1fkb',
      total,
      sum(m.get('s', 0) for m in infiles.itervalues()) / 1024.)
  cache_hit = set(infiles.iterkeys()) - set(x[0] for x in to_upload)
  cache_hit_size = sum(infiles[i].get('s', 0) for i in cache_hit)
  logging.info(
      'cache hit:  %6d, %9.1fkb, %6.2f%% files, %6.2f%% size',
      len(cache_hit),
      cache_hit_size / 1024.,
      len(cache_hit) * 100. / total,
      cache_hit_size * 100. / total_size)
  cache_miss = to_upload
  cache_miss_size = sum(infiles[i[0]].get('s', 0) for i in cache_miss)
  logging.info(
      'cache miss: %6d, %9.1fkb, %6.2f%% files, %6.2f%% size',
      len(cache_miss),
      cache_miss_size / 1024.,
      len(cache_miss) * 100. / total,
      cache_miss_size * 100. / total_size)


def main():
  parser = optparse.OptionParser(
      usage='%prog [options] <file1..fileN> or - to read from stdin',
      description=sys.modules[__name__].__doc__)
  # TODO(maruel): Support both NFS and isolateserver.
  parser.add_option('-o', '--outdir', help='Remote server to archive to')
  parser.add_option(
        '-v', '--verbose',
        action='count',
        help='Use multiple times to increase verbosity')

  options, files = parser.parse_args()

  levels = [logging.ERROR, logging.INFO, logging.DEBUG]
  logging.basicConfig(
      level=levels[min(len(levels)-1, options.verbose)],
      format='%(levelname)5s %(module)15s(%(lineno)3d): %(message)s')
  if files == ['-']:
    files = sys.stdin.readlines()

  if not files:
    parser.error('Nothing to upload')
  if not options.outdir:
    parser.error('Nowhere to send. Please specify --outdir')

  # Load the necessary metadata. This is going to be rewritten eventually to be
  # more efficient.
  infiles = dict(
      (
        f,
        {
          's': os.stat(f).st_size,
          'h': hashlib.sha1(open(f, 'r').read()).hexdigest(),
        }
      )
      for f in files)

  with run_isolated.Profiler('Archive'):
    upload_sha1_tree(
        base_url=options.outdir,
        indir=os.getcwd(),
        infiles=infiles)
  return 0


if __name__ == '__main__':
  sys.exit(main())
