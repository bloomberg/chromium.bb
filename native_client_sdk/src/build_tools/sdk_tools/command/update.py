# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import download
import logging
import os
from sdk_update_common import Error
import sdk_update_common
import sys
import urlparse
import urllib2

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)
sys.path.append(PARENT_DIR)
try:
  import cygtar
except ImportError:
  # Try to find this in the Chromium repo.
  CHROME_SRC_DIR = os.path.abspath(
      os.path.join(PARENT_DIR, '..', '..', '..', '..'))
  sys.path.append(os.path.join(CHROME_SRC_DIR, 'native_client', 'build'))
  import cygtar


RECOMMENDED = 'recommended'
SDK_TOOLS = 'sdk_tools'
HTTP_CONTENT_LENGTH = 'Content-Length'  # HTTP Header field for content length


class UpdateDelegate(object):
  def BundleDirectoryExists(self, bundle_name):
    raise NotImplementedError()

  def DownloadToFile(self, url, dest_filename):
    raise NotImplementedError()

  def ExtractArchives(self, archives, extract_dir, rename_from_dir,
                      rename_to_dir):
    raise NotImplementedError()


class RealUpdateDelegate(UpdateDelegate):
  def __init__(self, user_data_dir, install_dir):
    UpdateDelegate.__init__(self)
    self.user_data_dir = user_data_dir
    self.install_dir = install_dir

  def BundleDirectoryExists(self, bundle_name):
    bundle_path = os.path.join(self.install_dir, bundle_name)
    return os.path.isdir(bundle_path)

  def DownloadToFile(self, url, dest_filename):
    sdk_update_common.MakeDirs(self.user_data_dir)
    dest_path = os.path.join(self.user_data_dir, dest_filename)
    out_stream = None
    url_stream = None
    try:
      out_stream = open(dest_path, 'wb')
      url_stream = download.UrlOpen(url)
      content_length = int(url_stream.info()[HTTP_CONTENT_LENGTH])
      progress = download.MakeProgressFunction(content_length)
      sha1, size = download.DownloadAndComputeHash(url_stream, out_stream,
                                                   progress)
      return sha1, size
    except urllib2.URLError as e:
      raise Error('Unable to read from URL "%s".\n  %s' % (url, e))
    except IOError as e:
      raise Error('Unable to write to file "%s".\n  %s' % (dest_filename, e))
    finally:
      if url_stream:
        url_stream.close()
      if out_stream:
        out_stream.close()

  def ExtractArchives(self, archives, extract_dir, rename_from_dir,
                      rename_to_dir):
    tar_file = None

    extract_path = os.path.join(self.install_dir, extract_dir)
    rename_from_path = os.path.join(self.install_dir, rename_from_dir)
    rename_to_path = os.path.join(self.install_dir, rename_to_dir)

    # Extract to extract_dir, usually "<bundle name>_update".
    # This way if the extraction fails, we haven't blown away the old bundle
    # (if it exists).
    sdk_update_common.RemoveDir(extract_path)
    sdk_update_common.MakeDirs(extract_path)
    curpath = os.getcwd()
    tar_file = None

    try:
      try:
        logging.info('Changing the directory to %s' % (extract_path,))
        os.chdir(extract_path)
      except Exception as e:
        raise Error('Unable to chdir into "%s".\n  %s' % (extract_path, e))

      for i, archive in enumerate(archives):
        archive_path = os.path.join(self.user_data_dir, archive)

        try:
          logging.info('Opening file %s (%d/%d).' % (archive_path, i + 1,
              len(archives)))
          try:
            tar_file = cygtar.CygTar(archive_path, 'r', verbose=True)
          except Exception as e:
            raise Error('Can\'t open archive "%s".\n  %s' % (archive_path, e))

          logging.info('Extracting to %s' % (extract_path,))
          if len(archives) > 1:
            print '(file %d/%d - "%s")' % (
                 i + 1, len(archives), os.path.basename(archive_path))
          tar_file.Extract()
        finally:
          if tar_file:
            tar_file.Close()

          # Remove the archive.
          if os.path.exists(archive_path):
            os.remove(archive_path)

      logging.info('Changing the directory to %s' % (curpath,))
      os.chdir(curpath)

      logging.info('Renaming %s->%s' % (rename_from_path, rename_to_path))
      sdk_update_common.RenameDir(rename_from_path, rename_to_path)
    finally:
      # Change the directory back so we can remove the update directory.
      os.chdir(curpath)

      # Clean up the ..._update directory.
      try:
        sdk_update_common.RemoveDir(extract_path)
      except Exception as e:
        logging.error('Failed to remove directory \"%s\".  %s' % (
            extract_path, e))


def Update(delegate, remote_manifest, local_manifest, bundle_names, force):
  valid_bundles = set([bundle.name for bundle in remote_manifest.GetBundles()])
  requested_bundles = _GetRequestedBundleNamesFromArgs(remote_manifest,
                                                       bundle_names)
  invalid_bundles = requested_bundles - valid_bundles
  if invalid_bundles:
    logging.warn('Ignoring unknown bundle(s): %s' % (
        ', '.join(invalid_bundles)))
    requested_bundles -= invalid_bundles

  if SDK_TOOLS in requested_bundles:
    logging.warn('Updating sdk_tools happens automatically. '
                 'Ignoring manual update request.')
    requested_bundles.discard(SDK_TOOLS)

  if requested_bundles:
    for bundle_name in requested_bundles:
      logging.info('Trying to update %s' % (bundle_name,))
      UpdateBundleIfNeeded(delegate, remote_manifest, local_manifest,
          bundle_name, force)
  else:
    logging.warn('No bundles to update.')


def UpdateBundleIfNeeded(delegate, remote_manifest, local_manifest,
                         bundle_name, force):
  bundle = remote_manifest.GetBundle(bundle_name)
  if bundle:
    if _BundleNeedsUpdate(delegate, local_manifest, bundle):
      # TODO(binji): It would be nicer to detect whether the user has any
      # modifications to the bundle. If not, we could update with impunity.
      if not force and delegate.BundleDirectoryExists(bundle_name):
        print ('%s already exists, but has an update available.\n'
            'Run update with the --force option to overwrite the '
            'existing directory.\nWarning: This will overwrite any '
            'modifications you have made within this directory.'
            % (bundle_name,))
        return

      _UpdateBundle(delegate, bundle, local_manifest)
    else:
      print '%s is already up-to-date.' % (bundle.name,)
  else:
    logging.error('Bundle %s does not exist.' % (bundle_name,))


def _GetRequestedBundleNamesFromArgs(remote_manifest, requested_bundles):
  requested_bundles = set(requested_bundles)
  if RECOMMENDED in requested_bundles:
    requested_bundles.discard(RECOMMENDED)
    requested_bundles |= set(_GetRecommendedBundleNames(remote_manifest))

  return requested_bundles


def _GetRecommendedBundleNames(remote_manifest):
  return [bundle.name for bundle in remote_manifest.GetBundles() if
      bundle.recommended]


def _BundleNeedsUpdate(delegate, local_manifest, bundle):
  # Always update the bundle if the directory doesn't exist;
  # the user may have deleted it.
  if not delegate.BundleDirectoryExists(bundle.name):
    return True

  return local_manifest.BundleNeedsUpdate(bundle)


def _UpdateBundle(delegate, bundle, local_manifest):
  archives = bundle.GetHostOSArchives()
  if not archives:
    logging.warn('Bundle %s does not exist for this platform.' % (bundle.name,))
    return

  archive_filenames = []

  print 'Downloading bundle %s' % (bundle.name,)
  for i, archive in enumerate(archives):
    if len(archives) > 1:
      print '(file %d/%d - "%s")' % (
          i + 1, len(archives), os.path.basename(archive.url))

    archive_filename = _GetFilenameFromURL(archive.url)
    sha1, size = delegate.DownloadToFile(archive.url, archive_filename)
    _ValidateArchive(archive, sha1, size)

    archive_filenames.append(archive_filename)

  print 'Updating bundle %s to version %s, revision %s' % (
      bundle.name, bundle.version, bundle.revision)
  extract_dir = bundle.name + '_update'

  repath_dir = bundle.get('repath', None)
  if repath_dir:
    # If repath is specified:
    # The files are extracted to nacl_sdk/<bundle.name>_update/<repath>/...
    # The destination directory is nacl_sdk/<bundle.name>/...
    rename_from_dir = os.path.join(extract_dir, repath_dir)
  else:
    # If no repath is specified:
    # The files are extracted to nacl_sdk/<bundle.name>_update/...
    # The destination directory is nacl_sdk/<bundle.name>/...
    rename_from_dir = extract_dir

  rename_to_dir = bundle.name

  delegate.ExtractArchives(archive_filenames, extract_dir, rename_from_dir,
                           rename_to_dir)

  logging.info('Updating local manifest to include bundle %s' % (bundle.name))
  local_manifest.MergeBundle(bundle)


def _GetFilenameFromURL(url):
  _, _, path, _, _, _ = urlparse.urlparse(url)
  return path.split('/')[-1]


def _ValidateArchive(archive, actual_sha1, actual_size):
  if actual_sha1 != archive.GetChecksum():
    raise Error('SHA1 checksum mismatch on "%s".  Expected %s but got %s' % (
        archive.name, archive.GetChecksum(), actual_sha1))
  if actual_size != archive.size:
    raise Error('Size mismatch on "%s".  Expected %s but got %s bytes' % (
        archive.name, archive.size, actual_size))
