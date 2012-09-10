#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that reads omahaproxy and gsutil to determine version of SDK to put
in manifest.
"""

# pylint is convinced the email module is missing attributes
# pylint: disable=E1101

import buildbot_common
import csv
import cStringIO
import email
import manifest_util
import optparse
import os
import posixpath
import re
import smtplib
import subprocess
import sys
import time
import traceback
import urllib2


MANIFEST_BASENAME = 'naclsdk_manifest2.json'
SCRIPT_DIR = os.path.dirname(__file__)
REPO_MANIFEST = os.path.join(SCRIPT_DIR, 'json', MANIFEST_BASENAME)
GS_BUCKET_PATH = 'gs://nativeclient-mirror/nacl/nacl_sdk/'
GS_SDK_MANIFEST = GS_BUCKET_PATH + MANIFEST_BASENAME
GS_MANIFEST_BACKUP_DIR = GS_BUCKET_PATH + 'manifest_backups/'

CANARY_BUNDLE_NAME = 'pepper_canary'
CANARY = 'canary'


def SplitVersion(version_string):
  """Split a version string (e.g. "18.0.1025.163") into its components.

  Note that this function doesn't handle versions in the form "trunk.###".
  """
  return tuple(map(int, version_string.split('.')))


def JoinVersion(version_tuple):
  """Create a string from a version tuple.

  The tuple should be of the form (18, 0, 1025, 163).
  """
  return '.'.join(map(str, version_tuple))


def GetTimestampManifestName():
  """Create a manifest name with a timestamp.

  Returns:
    A manifest name with an embedded date. This should make it easier to roll
    back if necessary.
  """
  return time.strftime('naclsdk_manifest2.%Y_%m_%d_%H_%M_%S.json',
      time.gmtime())


def GetPlatformFromArchiveUrl(url):
  """Get the platform name given an archive url.

  Args:
    url: An archive url.
  Returns:
    A platform name (e.g. 'linux')."""
  match = re.match(r'naclsdk_(.*?)(?:\.tar)?\.bz2', posixpath.basename(url))
  if not match:
    return None
  return match.group(1)


def GetPlatformsFromArchives(archive_urls):
  """Get the platform names for a sequence of archive urls.

  Args:
    archives: A sequence of archive urls.
  Returns:
    A list of platforms, one for each url in |archive_urls|."""
  platforms = []
  for url in archive_urls:
    platform = GetPlatformFromArchiveUrl(url)
    if platform:
      platforms.append(platform)
  return platforms


class Delegate(object):
  """Delegate all external access; reading/writing to filesystem, gsutil etc."""

  def GetRepoManifest(self):
    """Read the manifest file from the NaCl SDK repository.

    This manifest is used as a template for the auto updater; only pepper
    bundles with no archives are considered for auto updating.

    Returns:
      A manifest_util.SDKManifest object read from the NaCl SDK repo."""
    raise NotImplementedError()

  def GetHistory(self):
    """Read Chrome release history from omahaproxy.appspot.com

    Here is an example of data from this URL:
      cros,stable,18.0.1025.168,2012-05-01 17:04:05.962578\n
      win,canary,20.0.1123.0,2012-05-01 13:59:31.703020\n
      mac,canary,20.0.1123.0,2012-05-01 11:54:13.041875\n
      win,stable,18.0.1025.168,2012-04-30 20:34:56.078490\n
      mac,stable,18.0.1025.168,2012-04-30 20:34:55.231141\n
      ...
    Where each line has comma separated values in the following format:
    platform, channel, version, date/time\n

    Returns:
      A list where each element is a line from the document, represented as a
      tuple."""
    raise NotImplementedError()

  def GetTrunkRevision(self, version):
    """Given a Chrome version, get its trunk revision.

    Args:
      version: A version string of the form '18.0.1025.64'
    Returns:
      The revision number for that version, as a string."""
    raise NotImplementedError()

  def GsUtil_ls(self, url):
    """Runs gsutil ls |url|

    Args:
      url: The commondatastorage url to list.
    Returns:
      A list of URLs, all with the gs:// schema."""
    raise NotImplementedError()

  def GsUtil_cat(self, url):
    """Runs gsutil cat |url|

    Args:
      url: The commondatastorage url to read from.
    Returns:
      A string with the contents of the file at |url|."""
    raise NotImplementedError()

  def GsUtil_cp(self, src, dest, stdin=None):
    """Runs gsutil cp |src| |dest|

    Args:
      src: The file path or url to copy from.
      dest: The file path or url to copy to.
      stdin: If src is '-', this is used as the stdin to give to gsutil. The
          effect is that text in stdin is copied to |dest|."""
    raise NotImplementedError()

  def Print(self, *args):
    """Print a message."""
    raise NotImplementedError()


class RealDelegate(Delegate):
  def __init__(self, dryrun=False, gsutil=None):
    super(RealDelegate, self).__init__()
    self.dryrun = dryrun
    if gsutil:
      self.gsutil = gsutil
    else:
      self.gsutil = buildbot_common.GetGsutil()

  def GetRepoManifest(self):
    """See Delegate.GetRepoManifest"""
    with open(REPO_MANIFEST, 'r') as sdk_stream:
      sdk_json_string = sdk_stream.read()

    manifest = manifest_util.SDKManifest()
    manifest.LoadDataFromString(sdk_json_string)
    return manifest

  def GetHistory(self):
    """See Delegate.GetHistory"""
    url_stream = urllib2.urlopen('https://omahaproxy.appspot.com/history')
    return [(platform, channel, version, date)
        for platform, channel, version, date in csv.reader(url_stream)]

  def GetTrunkRevision(self, version):
    """See Delegate.GetTrunkRevision"""
    url = 'http://omahaproxy.appspot.com/revision?version=%s' % (version,)
    return 'trunk.%s' % (urllib2.urlopen(url).read(),)

  def GsUtil_ls(self, url):
    """See Delegate.GsUtil_ls"""
    try:
      stdout = self._RunGsUtil(None, 'ls', url)
    except subprocess.CalledProcessError:
      return []

    # filter out empty lines
    return filter(None, stdout.split('\n'))

  def GsUtil_cat(self, url):
    """See Delegate.GsUtil_cat"""
    return self._RunGsUtil(None, 'cat', url)

  def GsUtil_cp(self, src, dest, stdin=None):
    """See Delegate.GsUtil_cp"""
    if self.dryrun:
      return

    # -p ensures we keep permissions when copying "in-the-cloud".
    return self._RunGsUtil(stdin, 'cp', '-p', '-a', 'public-read', src, dest)

  def Print(self, *args):
    sys.stdout.write(' '.join(map(str, args)) + '\n')

  def _RunGsUtil(self, stdin, *args):
    """Run gsutil as a subprocess.

    Args:
      stdin: If non-None, used as input to the process.
      *args: Arguments to pass to gsutil. The first argument should be an
          operation such as ls, cp or cat.
    Returns:
      The stdout from the process."""
    cmd = [self.gsutil] + list(args)
    if stdin:
      stdin_pipe = subprocess.PIPE
    else:
      stdin_pipe = None

    process = subprocess.Popen(cmd, stdin=stdin_pipe, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)
    stdout, stderr = process.communicate(stdin)

    if process.returncode != 0:
      sys.stderr.write(stderr)
      raise subprocess.CalledProcessError(process.returncode, ' '.join(cmd))
    return stdout


class VersionFinder(object):
  """Finds a version of a pepper bundle that all desired platforms share."""
  def __init__(self, delegate):
    self.delegate = delegate
    self.history = delegate.GetHistory()

  def GetMostRecentSharedVersion(self, major_version, platforms):
    """Returns the most recent version of a pepper bundle that exists on all
    given platforms.

    Specifically, the resulting version should be the most recently released
    (meaning closest to the top of the listing on
    omahaproxy.appspot.com/history) version that has a Chrome release on all
    given platforms, and has a pepper bundle archive for each platform as well.

    Args:
      major_version: The major version of the pepper bundle, e.g. 19.
      platforms: A sequence of platforms to consider, e.g.
          ('mac', 'linux', 'win')
    Returns:
      A tuple (version, channel, archives). The version is a string such as
      "19.0.1084.41". The channel is one of ('stable', 'beta', or 'dev').
      |archives| is a list of archive URLs."""
    shared_version_generator = self._FindNextSharedVersion(
        platforms,
        lambda platform: self._GetPlatformMajorVersionHistory(major_version,
                                                              platform))
    return self._DoGetMostRecentSharedVersion(platforms,
        shared_version_generator, allow_trunk_revisions=False)

  def GetMostRecentSharedCanary(self, platforms):
    """Returns the most recent version of a canary pepper bundle that exists on
    all given platforms.

    Canary is special-cased because we don't care about its major version; we
    always use the most recent canary, regardless of major version.

    Args:
      platforms: A sequence of platforms to consider, e.g.
          ('mac', 'linux', 'win')
    Returns:
      A tuple (version, channel, archives). The version is a string such as
      "19.0.1084.41". The channel is always 'canary'. |archives| is a list of
      archive URLs."""
    # We don't ship canary on Linux, so it won't appear in self.history.
    # Instead, we can use the matching Linux trunk build for that version.
    shared_version_generator = self._FindNextSharedVersion(
        set(platforms) - set(('linux',)),
        self._GetPlatformCanaryHistory)
    return self._DoGetMostRecentSharedVersion(platforms,
        shared_version_generator, allow_trunk_revisions=True)

  def _DoGetMostRecentSharedVersion(self, platforms, shared_version_generator,
      allow_trunk_revisions):
    """Returns the most recent version of a pepper bundle that exists on all
    given platforms.

    This function does the real work for the public GetMostRecentShared* above.

    Args:
      platforms: A sequence of platforms to consider, e.g.
          ('mac', 'linux', 'win')
      shared_version_generator: A generator that will yield (version, channel)
          tuples in order of most recent to least recent.
      allow_trunk_revisions: If True, will search for archives using the
            trunk revision that matches the branch version.
    Returns:
      A tuple (version, channel, archives). The version is a string such as
      "19.0.1084.41". The channel is one of ('stable', 'beta', 'dev',
      'canary'). |archives| is a list of archive URLs."""
    version = None
    skipped_versions = []
    channel = ''
    while True:
      try:
        version, channel = shared_version_generator.next()
      except StopIteration:
        msg = 'No shared version for platforms: %s\n' % (', '.join(platforms))
        msg += 'Last version checked = %s.\n' % (version,)
        if skipped_versions:
          msg += 'Versions skipped due to missing archives:\n'
          for version, channel, archives in skipped_versions:
            if archives:
              archive_msg = '(%s available)' % (', '.join(archives))
            else:
              archive_msg = '(none available)'
          msg += '  %s (%s) %s\n' % (version, channel, archive_msg)
        raise Exception(msg)

      archives = self._GetAvailableNaClSDKArchivesFor(version)
      missing_platforms = set(platforms) - \
          set(GetPlatformsFromArchives(archives))
      if allow_trunk_revisions and missing_platforms:
        # Try to find trunk archives for platforms that are missing archives.
        trunk_version = self.delegate.GetTrunkRevision(version)
        trunk_archives = self._GetAvailableNaClSDKArchivesFor(trunk_version)
        for trunk_archive in trunk_archives:
          trunk_archive_platform = GetPlatformFromArchiveUrl(trunk_archive)
          if trunk_archive_platform in missing_platforms:
            archives.append(trunk_archive)
            missing_platforms.discard(trunk_archive_platform)

      if not missing_platforms:
        return version, channel, archives

      skipped_versions.append((version, channel, archives))

  def _GetPlatformMajorVersionHistory(self, with_major_version, with_platform):
    """Yields Chrome history for a given platform and major version.

    Args:
      with_major_version: The major version to filter for. If 0, match all
          versions.
      with_platform: The name of the platform to filter for.
    Returns:
      A generator that yields a tuple (channel, version) for each version that
      matches the platform and major version. The version returned is a tuple as
      returned from SplitVersion.
    """
    for platform, channel, version, _ in self.history:
      version = SplitVersion(version)
      if (with_platform == platform and
          (with_major_version == 0 or with_major_version == version[0])):
        yield channel, version

  def _GetPlatformCanaryHistory(self, with_platform):
    """Yields Chrome history for a given platform, but only for canary
    versions.

    Args:
      with_platform: The name of the platform to filter for.
    Returns:
      A generator that yields a tuple (channel, version) for each version that
      matches the platform and uses the canary channel. The version returned is
      a tuple as returned from SplitVersion.
    """
    for platform, channel, version, _ in self.history:
      version = SplitVersion(version)
      if with_platform == platform and channel == CANARY:
        yield channel, version


  def _FindNextSharedVersion(self, platforms, generator_func):
    """Yields versions of Chrome that exist on all given platforms, in order of
       newest to oldest.

    Versions are compared in reverse order of release. That is, the most
    recently updated version will be tested first.

    Args:
      platforms: A sequence of platforms to filter for. Any other platforms will
          be ignored.
    Returns:
      A generator that yields a tuple (version, channel) for each version that
      matches all platforms and the major version. The version returned is a
      string (e.g. "18.0.1025.164").
    """
    platform_generators = []
    for platform in platforms:
      platform_generators.append(generator_func(platform))

    shared_version = None
    platform_versions = [(tuple(), '')] * len(platforms)
    while True:
      try:
        for i, platform_gen in enumerate(platform_generators):
          if platform_versions[i][1] != shared_version:
            platform_versions[i] = platform_gen.next()
      except StopIteration:
        return

      shared_version = min(v for c, v in platform_versions)

      if all(v == shared_version for c, v in platform_versions):
        # grab the channel from an arbitrary platform
        first_platform = platform_versions[0]
        channel = first_platform[0]
        yield JoinVersion(shared_version), channel

        # force increment to next version for all platforms
        shared_version = None

  def _GetAvailableNaClSDKArchivesFor(self, version_string):
    """Downloads a list of all available archives for a given version.

    Args:
      version_string: The version to find archives for. (e.g. "18.0.1025.164")
    Returns:
      A list of strings, each of which is a platform-specific archive URL. (e.g.
      "gs://nativeclient_mirror/nacl/nacl_sdk/18.0.1025.164/"
      "naclsdk_linux.tar.bz2").

      All returned URLs will use the gs:// schema."""
    files = self.delegate.GsUtil_ls(GS_BUCKET_PATH + version_string)

    assert all(file.startswith('gs://') for file in files)

    archives = [f for f in files if not f.endswith('.json')]
    manifests = [f for f in files if f.endswith('.json')]

    # don't include any archives that don't have an associated manifest.
    return filter(lambda a: a + '.json' in manifests, archives)


class Updater(object):
  def __init__(self, delegate):
    self.delegate = delegate
    self.versions_to_update = []

  def AddVersionToUpdate(self, bundle_name, version, channel, archives):
    """Add a pepper version to update in the uploaded manifest.

    Args:
      bundle_name: The name of the pepper bundle, e.g. 'pepper_18'
      version: The version of the pepper bundle, e.g. '18.0.1025.64'
      channel: The stability of the pepper bundle, e.g. 'beta'
      archives: A sequence of archive URLs for this bundle."""
    self.versions_to_update.append((bundle_name, version, channel, archives))

  def Update(self, manifest):
    """Update a manifest and upload it.

    Args:
      manifest: The manifest used as a template for updating. Only pepper
      bundles that contain no archives will be considered for auto-updating."""
    for bundle_name, version, channel, archives in self.versions_to_update:
      self.delegate.Print('Updating %s to %s...' % (bundle_name, version))
      bundle = manifest.GetBundle(bundle_name)
      for archive in archives:
        platform_bundle = self._GetPlatformArchiveBundle(archive)
        # Normally the manifest snippet's bundle name matches our bundle name.
        # pepper_canary, however is called "pepper_###" in the manifest
        # snippet.
        platform_bundle.name = bundle_name
        bundle.MergeWithBundle(platform_bundle)
      bundle.stability = channel
      # We always recommend the stable version.
      if channel == 'stable':
        bundle.recommended = 'yes'
      else:
        bundle.recommended = 'no'
      manifest.MergeBundle(bundle)
    self._UploadManifest(manifest)
    self.delegate.Print('Done.')

  def _GetPlatformArchiveBundle(self, archive):
    """Downloads the manifest "snippet" for an archive, and reads it as a
       Bundle.

    Args:
      archive: A full URL of a platform-specific archive, using the gs schema.
    Returns:
      An object of type manifest_util.Bundle, read from a JSON file storing
      metadata for this archive.
    """
    stdout = self.delegate.GsUtil_cat(archive + '.json')
    bundle = manifest_util.Bundle('')
    bundle.LoadDataFromString(stdout)
    # Some snippets were uploaded with revisions and versions as strings. Fix
    # those here.
    bundle.revision = int(bundle.revision)
    bundle.version = int(bundle.version)
    return bundle

  def _UploadManifest(self, manifest):
    """Upload a serialized manifest_util.SDKManifest object.

    Upload one copy to gs://<BUCKET_PATH>/naclsdk_manifest2.json, and a copy to
    gs://<BUCKET_PATH>/manifest_backups/naclsdk_manifest2.<TIMESTAMP>.json.

    Args:
      manifest: The new manifest to upload.
    """
    timestamp_manifest_path = GS_MANIFEST_BACKUP_DIR + \
        GetTimestampManifestName()
    self.delegate.GsUtil_cp('-', timestamp_manifest_path,
        stdin=manifest.GetDataAsString())

    # copy from timestampped copy over the official manifest.
    self.delegate.GsUtil_cp(timestamp_manifest_path, GS_SDK_MANIFEST)


def Run(delegate, platforms):
  """Entry point for the auto-updater."""
  manifest = delegate.GetRepoManifest()
  auto_update_bundles = []
  for bundle in manifest.GetBundles():
    if not bundle.name.startswith('pepper_'):
      continue
    archives = bundle.GetArchives()
    if not archives:
      auto_update_bundles.append(bundle)

  if not auto_update_bundles:
    delegate.Print('No versions need auto-updating.')
    return

  version_finder = VersionFinder(delegate)
  updater = Updater(delegate)

  for bundle in auto_update_bundles:
    if bundle.name == CANARY_BUNDLE_NAME:
      version, channel, archives = version_finder.GetMostRecentSharedCanary(
          platforms)
    else:
      version, channel, archives = version_finder.GetMostRecentSharedVersion(
          bundle.version, platforms)

    updater.AddVersionToUpdate(bundle.name, version, channel, archives)

  updater.Update(manifest)


def SendMail(send_from, send_to, subject, text, smtp='localhost'):
  """Send an email.

  Args:
    send_from: The sender's email address.
    send_to: A list of addresses to send to.
    subject: The subject of the email.
    text: The text of the email.
    smtp: The smtp server to use. Default is localhost.
  """
  msg = email.MIMEMultipart.MIMEMultipart()
  msg['From'] = send_from
  msg['To'] = ', '.join(send_to)
  msg['Date'] = email.Utils.formatdate(localtime=True)
  msg['Subject'] = subject
  msg.attach(email.MIMEText.MIMEText(text))
  smtp_obj = smtplib.SMTP(smtp)
  smtp_obj.sendmail(send_from, send_to, msg.as_string())
  smtp_obj.close()


class CapturedFile(object):
  """A file-like object that captures text written to it, but also passes it
  through to an underlying file-like object."""
  def __init__(self, passthrough):
    self.passthrough = passthrough
    self.written = cStringIO.StringIO()

  def write(self, s):
    self.written.write(s)
    if self.passthrough:
      self.passthrough.write(s)

  def getvalue(self):
    return self.written.getvalue()


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--gsutil', help='path to gsutil', dest='gsutil',
      default=None)
  parser.add_option('--mailfrom', help='email address of sender',
      dest='mailfrom', default=None)
  parser.add_option('--mailto', help='send error mails to...', dest='mailto',
      default=[], action='append')
  parser.add_option('--dryrun', help='don\'t upload the manifest.',
      dest='dryrun', action='store_true', default=False)
  options, args = parser.parse_args(args[1:])

  if (options.mailfrom is None) != (not options.mailto):
    options.mailfrom = None
    options.mailto = None
    sys.stderr.write('warning: Disabling email, one of --mailto or --mailfrom '
        'was missing.\n')

  if options.mailfrom and options.mailto:
    # Capture stderr so it can be emailed, if necessary.
    sys.stderr = CapturedFile(sys.stderr)

  try:
    delegate = RealDelegate(dryrun=options.dryrun, gsutil=options.gsutil)
    Run(delegate, ('mac', 'win', 'linux'))
  except Exception:
    if options.mailfrom and options.mailto:
      traceback.print_exc()
      scriptname = os.path.basename(sys.argv[0])
      subject = '[%s] Failed to update manifest' % (scriptname,)
      text = '%s failed.\n\nSTDERR:\n%s\n' % (scriptname, sys.stderr.getvalue())
      SendMail(options.mailfrom, options.mailto, subject, text)
      sys.exit(1)
    else:
      raise


if __name__ == '__main__':
  sys.exit(main(sys.argv))
