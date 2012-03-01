#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''A simple tool to update the Native Client SDK to the latest version'''

import cStringIO
import cygtar
import errno
import exceptions
import hashlib
import json
import optparse
import os
import shutil
import subprocess
import sys
import tempfile
import time
import urllib2
import urlparse


#------------------------------------------------------------------------------
# Constants

# Bump the MINOR_REV every time you check this file in.
MAJOR_REV = 2
MINOR_REV = 16

GLOBAL_HELP = '''Usage: naclsdk [options] command [command_options]

naclsdk is a simple utility that updates the Native Client (NaCl)
Software Developer's Kit (SDK).  Each component is kept as a 'bundle' that
this utility can download as as subdirectory into the SDK.

Commands:
  help [command] - Get either general or command-specific help
  list - Lists the available bundles
  update/install - Updates/installs bundles in the SDK

Example Usage:
  naclsdk list
  naclsdk update --force pepper_17
  naclsdk install recommended
  naclsdk help update'''

MANIFEST_FILENAME='naclsdk_manifest2.json'
SDK_TOOLS='sdk_tools'  # the name for this tools directory
USER_DATA_DIR='sdk_cache'

HTTP_CONTENT_LENGTH = 'Content-Length'  # HTTP Header field for content length

# The following SSL certificates are used to validate the SSL connection
# to https://commondatastorage.googleapis.com
# TODO(mball): Validate at least one of these certificates.
# http://stackoverflow.com/questions/1087227/validate-ssl-certificates-with-python

EQUIFAX_SECURE_CA_CERTIFICATE='''-----BEGIN CERTIFICATE-----
MIIDIDCCAomgAwIBAgIENd70zzANBgkqhkiG9w0BAQUFADBOMQswCQYDVQQGEwJV
UzEQMA4GA1UEChMHRXF1aWZheDEtMCsGA1UECxMkRXF1aWZheCBTZWN1cmUgQ2Vy
dGlmaWNhdGUgQXV0aG9yaXR5MB4XDTk4MDgyMjE2NDE1MVoXDTE4MDgyMjE2NDE1
MVowTjELMAkGA1UEBhMCVVMxEDAOBgNVBAoTB0VxdWlmYXgxLTArBgNVBAsTJEVx
dWlmYXggU2VjdXJlIENlcnRpZmljYXRlIEF1dGhvcml0eTCBnzANBgkqhkiG9w0B
AQEFAAOBjQAwgYkCgYEAwV2xWGcIYu6gmi0fCG2RFGiYCh7+2gRvE4RiIcPRfM6f
BeC4AfBONOziipUEZKzxa1NfBbPLZ4C/QgKO/t0BCezhABRP/PvwDN1Dulsr4R+A
cJkVV5MW8Q+XarfCaCMczE1ZMKxRHjuvK9buY0V7xdlfUNLjUA86iOe/FP3gx7kC
AwEAAaOCAQkwggEFMHAGA1UdHwRpMGcwZaBjoGGkXzBdMQswCQYDVQQGEwJVUzEQ
MA4GA1UEChMHRXF1aWZheDEtMCsGA1UECxMkRXF1aWZheCBTZWN1cmUgQ2VydGlm
aWNhdGUgQXV0aG9yaXR5MQ0wCwYDVQQDEwRDUkwxMBoGA1UdEAQTMBGBDzIwMTgw
ODIyMTY0MTUxWjALBgNVHQ8EBAMCAQYwHwYDVR0jBBgwFoAUSOZo+SvSspXXR9gj
IBBPM5iQn9QwHQYDVR0OBBYEFEjmaPkr0rKV10fYIyAQTzOYkJ/UMAwGA1UdEwQF
MAMBAf8wGgYJKoZIhvZ9B0EABA0wCxsFVjMuMGMDAgbAMA0GCSqGSIb3DQEBBQUA
A4GBAFjOKer89961zgK5F7WF0bnj4JXMJTENAKaSbn+2kmOeUJXRmm/kEd5jhW6Y
7qj/WsjTVbJmcVfewCHrPSqnI0kBBIZCe/zuf6IWUrVnZ9NA2zsmWLIodz2uFHdh
1voqZiegDfqnc1zqcPGUIWVEX/r87yloqaKHee9570+sB3c4
-----END CERTIFICATE-----'''

GOOGLE_INTERNET_AUTHORITY_CERTIFICATE='''-----BEGIN CERTIFICATE-----
MIICsDCCAhmgAwIBAgIDC2dxMA0GCSqGSIb3DQEBBQUAME4xCzAJBgNVBAYTAlVT
MRAwDgYDVQQKEwdFcXVpZmF4MS0wKwYDVQQLEyRFcXVpZmF4IFNlY3VyZSBDZXJ0
aWZpY2F0ZSBBdXRob3JpdHkwHhcNMDkwNjA4MjA0MzI3WhcNMTMwNjA3MTk0MzI3
WjBGMQswCQYDVQQGEwJVUzETMBEGA1UEChMKR29vZ2xlIEluYzEiMCAGA1UEAxMZ
R29vZ2xlIEludGVybmV0IEF1dGhvcml0eTCBnzANBgkqhkiG9w0BAQEFAAOBjQAw
gYkCgYEAye23pIucV+eEPkB9hPSP0XFjU5nneXQUr0SZMyCSjXvlKAy6rWxJfoNf
NFlOCnowzdDXxFdF7dWq1nMmzq0yE7jXDx07393cCDaob1FEm8rWIFJztyaHNWrb
qeXUWaUr/GcZOfqTGBhs3t0lig4zFEfC7wFQeeT9adGnwKziV28CAwEAAaOBozCB
oDAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYEFL/AMOv1QxE+Z7qekfv8atrjaxIk
MB8GA1UdIwQYMBaAFEjmaPkr0rKV10fYIyAQTzOYkJ/UMBIGA1UdEwEB/wQIMAYB
Af8CAQAwOgYDVR0fBDMwMTAvoC2gK4YpaHR0cDovL2NybC5nZW90cnVzdC5jb20v
Y3Jscy9zZWN1cmVjYS5jcmwwDQYJKoZIhvcNAQEFBQADgYEAuIojxkiWsRF8YHde
BZqrocb6ghwYB8TrgbCoZutJqOkM0ymt9e8kTP3kS8p/XmOrmSfLnzYhLLkQYGfN
0rTw8Ktx5YtaiScRhKqOv5nwnQkhClIZmloJ0pC3+gz4fniisIWvXEyZ2VxVKfml
UUIuOss4jHg7y/j7lYe8vJD5UDI=
-----END CERTIFICATE-----'''

GOOGLE_USER_CONTENT_CERTIFICATE='''-----BEGIN CERTIFICATE-----
MIIEPDCCA6WgAwIBAgIKUaoA4wADAAAueTANBgkqhkiG9w0BAQUFADBGMQswCQYD
VQQGEwJVUzETMBEGA1UEChMKR29vZ2xlIEluYzEiMCAGA1UEAxMZR29vZ2xlIElu
dGVybmV0IEF1dGhvcml0eTAeFw0xMTA4MTIwMzQ5MjlaFw0xMjA4MTIwMzU5Mjla
MHExCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRYwFAYDVQQHEw1N
b3VudGFpbiBWaWV3MRMwEQYDVQQKEwpHb29nbGUgSW5jMSAwHgYDVQQDFBcqLmdv
b2dsZXVzZXJjb250ZW50LmNvbTCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEA
uDmDvqlKBj6DppbENEuUmwVsHe5hpixV0bn6D+Ujy3mWUP9HtkO35/RmeFf4/y9i
nGy78uWO6tk9QY1PsPSiyZN6LgplalBdkTeODCGAieVOVJFhHQ0KM330qDy9sKNM
rwdMOfLPzkBMYPyr1C7CCm24j//aFiMCxD40bDQXRJkCAwEAAaOCAgQwggIAMB0G
A1UdDgQWBBRyHxPfv+Lnm2Kgid72ja3pOszMsjAfBgNVHSMEGDAWgBS/wDDr9UMR
Pme6npH7/Gra42sSJDBbBgNVHR8EVDBSMFCgTqBMhkpodHRwOi8vd3d3LmdzdGF0
aWMuY29tL0dvb2dsZUludGVybmV0QXV0aG9yaXR5L0dvb2dsZUludGVybmV0QXV0
aG9yaXR5LmNybDBmBggrBgEFBQcBAQRaMFgwVgYIKwYBBQUHMAKGSmh0dHA6Ly93
d3cuZ3N0YXRpYy5jb20vR29vZ2xlSW50ZXJuZXRBdXRob3JpdHkvR29vZ2xlSW50
ZXJuZXRBdXRob3JpdHkuY3J0MCEGCSsGAQQBgjcUAgQUHhIAVwBlAGIAUwBlAHIA
dgBlAHIwgdUGA1UdEQSBzTCByoIXKi5nb29nbGV1c2VyY29udGVudC5jb22CFWdv
b2dsZXVzZXJjb250ZW50LmNvbYIiKi5jb21tb25kYXRhc3RvcmFnZS5nb29nbGVh
cGlzLmNvbYIgY29tbW9uZGF0YXN0b3JhZ2UuZ29vZ2xlYXBpcy5jb22CEGF0Z2ds
c3RvcmFnZS5jb22CEiouYXRnZ2xzdG9yYWdlLmNvbYIUKi5zLmF0Z2dsc3RvcmFn
ZS5jb22CCyouZ2dwaHQuY29tgglnZ3BodC5jb20wDQYJKoZIhvcNAQEFBQADgYEA
XDvIl0/id823eokdFpLA8bL3pb7wQGaH0i3b29572aM7cDKqyxmTBbwi9mMMgbxy
E/St8DoSEQg3cJ/t2UaTXtw8wCrA6M1dS/RFpNLfV84QNcVdNhLmKEuZjpa+miUK
8OtYzFSMdfwXrbqKgkAIaqUs6m+LWKG/AQShp6DvTPo=
-----END CERTIFICATE-----'''

# Some commonly-used key names.
ARCHIVES_KEY = 'archives'
BUNDLES_KEY = 'bundles'
NAME_KEY = 'name'
REVISION_KEY = 'revision'
VERSION_KEY = 'version'

# Valid values for bundle.stability field
STABILITY_LITERALS = [
    'obsolete', 'post_stable', 'stable', 'beta', 'dev', 'canary']
# Valid values for the archive.host_os field
HOST_OS_LITERALS = frozenset(['mac', 'win', 'linux', 'all'])
# Valid values for bundle-recommended field.
YES_NO_LITERALS = ['yes', 'no']
# Valid keys for various sdk objects, used for validation.
VALID_ARCHIVE_KEYS = frozenset(['host_os', 'size', 'checksum', 'url'])
VALID_BUNDLES_KEYS = frozenset([
    ARCHIVES_KEY, NAME_KEY, VERSION_KEY, REVISION_KEY,
    'description', 'desc_url', 'stability', 'recommended', 'repath',
    ])
VALID_MANIFEST_KEYS = frozenset(['manifest_version', BUNDLES_KEY])


#------------------------------------------------------------------------------
# General Utilities


_debug_mode = False
_quiet_mode = False


def DebugPrint(msg):
  '''Display a message to stderr if debug printing is enabled

  Note: This function appends a newline to the end of the string

  Args:
    msg: A string to send to stderr in debug mode'''
  if _debug_mode:
    sys.stderr.write("%s\n" % msg)
    sys.stderr.flush()


def InfoPrint(msg):
  '''Display an informational message to stdout if not in quiet mode

  Note: This function appends a newline to the end of the string

  Args:
    mgs: A string to send to stdio when not in quiet mode'''
  if not _quiet_mode:
    sys.stdout.write("%s\n" % msg)
    sys.stdout.flush()


def WarningPrint(msg):
  '''Display an informational message to stderr.

  Note: This function appends a newline to the end of the string

  Args:
    mgs: A string to send to stderr.'''
  sys.stderr.write("WARNING: %s\n" % msg)
  sys.stderr.flush()


class Error(Exception):
  '''Generic error/exception for sdk_update module'''
  pass


def GetHostOS():
  '''Returns the host_os value that corresponds to the current host OS'''
  return {
      'linux2': 'linux',
      'darwin': 'mac',
      'cygwin': 'win',
      'win32':  'win'
      }[sys.platform]


def ExtractInstaller(installer, outdir):
  '''Extract the SDK installer into a given directory

  If the outdir already exists, then this function deletes it

  Args:
    installer: full path of the SDK installer
    outdir: output directory where to extract the installer

  Raises:
    CalledProcessError - if the extract operation fails'''
  RemoveDir(outdir)

  if os.path.splitext(installer)[1] == '.exe':
    # If the installer has extension 'exe', assume it's a Windows NSIS-style
    # installer that handles silent (/S) and relocated (/D) installs.
    command = [installer, '/S', '/D=%s' % outdir]
    subprocess.check_call(command)
  else:
    os.mkdir(outdir)
    tar_file = None
    curpath = os.getcwd()
    try:
      tar_file = cygtar.CygTar(installer, 'r', verbose=True)
      if outdir: os.chdir(outdir)
      tar_file.Extract()
    finally:
      if tar_file:
        tar_file.Close()
      os.chdir(curpath)


def RemoveDir(outdir):
  '''Removes the given directory

  On Unix systems, this just runs shutil.rmtree, but on Windows, this doesn't
  work when the directory contains junctions (as does our SDK installer).
  Therefore, on Windows, it runs rmdir /S /Q as a shell command.  This always
  does the right thing on Windows. If the directory already didn't exist,
  RemoveDir will return successfully without taking any action.

  Args:
    outdir: The directory to delete

  Raises:
    CalledProcessError - if the delete operation fails on Windows
    OSError - if the delete operation fails on Linux
  '''

  DebugPrint('Removing %s' % outdir)
  try:
    shutil.rmtree(outdir)
  except:
    if not os.path.exists(outdir):
      return
    # On Windows this could be an issue with junctions, so try again with rmdir
    if sys.platform == 'win32':
      subprocess.check_call(['rmdir', '/S', '/Q', outdir], shell=True)


def RenameDir(srcdir, destdir):
  '''Renames srcdir to destdir. Removes destdir before doing the
     rename if it already exists.'''

  max_tries = 5
  for num_tries in xrange(max_tries):
    try:
      RemoveDir(destdir)
      os.rename(srcdir, destdir)
      return
    except OSError as err:
      if err.errno != errno.EACCES:
        raise err
      # If we are here, we didn't exit due to raised exception, so we are
      # handling a Windows flaky access error.  Sleep one second and try
      # again.
      time.sleep(num_tries + 1)
  # end of while loop -- could not RenameDir
  raise Error('Could not RenameDir %s => %s after %d tries.\n' %
              'Please check that no shells or applications '
              'are accessing files in %s.'
              % (srcdir, destdir, num_tries, destdir))


def ShowProgress(progress):
  ''' A download-progress function used by class Archive.
      (See DownloadAndComputeHash).'''
  global count  # A divider, so we don't emit dots too often.

  if progress == 0:
    count = 0
  elif progress == 100:
    sys.stdout.write('\n')
  else:
    count = count + 1
    if count > 10:
      sys.stdout.write('.')
      sys.stdout.flush()
      count = 0


class ProgressFunction(object):
  '''Create a progress function for a file with a given size'''

  def __init__(self, file_size=0):
    '''Constructor

    Args:
      file_size: number of bytes in file.  0 indicates unknown'''
    self.dots = 0
    self.file_size = int(file_size)

  def GetProgressFunction(self):
    '''Returns a progress function based on a known file size'''
    def ShowKnownProgress(progress):
      if progress == 0:
        sys.stdout.write('|%s|\n' % ('=' * 48))
      else:
        new_dots = progress * 50 / self.file_size - self.dots
        sys.stdout.write('.' * new_dots)
        self.dots += new_dots
        if progress == self.file_size:
          sys.stdout.write('\n')
      sys.stdout.flush()

    return ShowKnownProgress


def DownloadAndComputeHash(from_stream, to_stream=None, progress_func=None):
  ''' Download the archive data from from-stream and generate sha1 and
      size info.

  Args:
    from_stream:   An input stream that supports read.
    to_stream:     [optional] the data is written to to_stream if it is
                   provided.
    progress_func: [optional] A function used to report download progress. If
                   provided, progress_func is called with progress=0 at the
                   beginning of the download, periodically with progress=1
                   during the download, and progress=100 at the end.

  Return
    A tuple (sha1, size) where sha1 is a sha1-hash for the archive data and
    size is the size of the archive data in bytes.'''
  # Use a no-op progress function if none is specified.
  def progress_no_op(progress):
    pass
  if not progress_func:
    progress_func = progress_no_op

  sha1_hash = hashlib.sha1()
  size = 0
  progress_func(progress=0)
  while(1):
    data = from_stream.read(32768)
    if not data:
      break
    sha1_hash.update(data)
    size += len(data)
    if to_stream:
      to_stream.write(data)
    progress_func(size)

  progress_func(progress=100)
  return sha1_hash.hexdigest(), size


class Archive(dict):
  ''' A placeholder for sdk archive information. We derive Archive from
      dict so that it is easily serializable. '''
  def __init__(self, host_os_name):
    ''' Create a new archive for the given host-os name. '''
    self['host_os'] = host_os_name

  def CopyFrom(self, dict):
    ''' Update the content of the archive by copying values from the given
        dictionary.

    Args:
      dict: The dictionary whose values must be copied to the archive.'''
    for key, value in dict.items():
      self[key] = value

  def Validate(self):
    ''' Validate the content of the archive object. Raise an Error if
        an invalid or missing field is found.
    Returns: True if self is a valid bundle.
    '''
    host_os = self.get('host_os', None)
    if host_os and host_os not in HOST_OS_LITERALS:
      raise Error('Invalid host-os name in archive')
    # Ensure host_os has a valid string. We'll use it for pretty printing.
    if not host_os:
      host_os = 'all (default)'
    if not self.get('url', None):
      raise Error('Archive "%s" has no URL' % host_os)
    # Verify that all key names are valid.
    for key, val in self.iteritems():
      if key not in VALID_ARCHIVE_KEYS:
        raise Error('Archive "%s" has invalid attribute "%s"' % (host_os, key))

  def _OpenURLStream(self):
    ''' Open a file-like stream for the archives's url. Raises an Error if the
        url can't be opened.

    Return:
      A file-like object from which the archive's data can be read.'''
    try:
      url_stream = urllib2.urlopen(self['url'])
    except urllib2.URLError:
      raise Error('Cannot open "%s" for archive %s' %
          (self['url'], self['host_os']))

    return url_stream

  def ComputeSha1AndSize(self):
    ''' Compute the sha1 hash and size of the archive's data. Raises
        an Error if the url can't be opened.

    Return:
      A tuple (sha1, size) with the sha1 hash and data size respectively.'''
    stream = None
    sha1 = None
    size = 0
    try:
      print 'Scanning archive to generate sha1 and size info:'
      stream = self._OpenURLStream()
      content_length = int(stream.info()[HTTP_CONTENT_LENGTH])
      sha1, size = DownloadAndComputeHash(from_stream=stream,
                                          progress_func=ShowProgress)
      if size != content_length:
        raise Error('Download size mismatch for %s.\n'
                    'Expected %s bytes but got %s' %
                    (self['url'], content_length, size))
    finally:
      if stream: stream.close()
    return sha1, size

  def DownloadToFile(self, dest_path):
    ''' Download the archive's data to a file at dest_path. As a side effect,
        computes the sha1 hash and data size, both returned as a tuple. Raises
        an Error if the url can't be opened, or an IOError exception if
        dest_path can't be opened.

    Args:
      dest_path: Path for the file that will receive the data.
    Return:
      A tuple (sha1, size) with the sha1 hash and data size respectively.'''
    sha1 = None
    size = 0
    with open(dest_path, 'wb') as to_stream:
      from_stream = None
      try:
        from_stream = self._OpenURLStream()
        content_length = int(from_stream.info()[HTTP_CONTENT_LENGTH])
        progress_function = ProgressFunction(
            content_length).GetProgressFunction()
        InfoPrint('Downloading %s' % self['url'])
        sha1, size = DownloadAndComputeHash(
            from_stream,
            to_stream=to_stream,
            progress_func=progress_function)
        if size != content_length:
          raise Error('Download size mismatch for %s.\n'
                      'Expected %s bytes but got %s' %
                      (self['url'], content_length, size))
      finally:
        if from_stream: from_stream.close()
    return sha1, size

  def Update(self, url):
    ''' Update the archive with the new url. Automatically update the
        archive's size and checksum fields. Raises an Error if the url is
        is invalid. '''
    self['url'] = url
    sha1, size = self.ComputeSha1AndSize()
    self['size'] = size
    self['checksum'] = {'sha1': sha1}

  def GetUrl(self):
    '''Returns the URL of this Archive'''
    return self['url']

  def GetSize(self):
    '''Returns the size of this archive, in bytes'''
    return int(self['size'])

  def GetChecksum(self, type='sha1'):
    '''Returns a given cryptographic checksum of the archive'''
    return self['checksum'][type]


class Bundle(dict):
  ''' A placeholder for sdk bundle information. We derive Bundle from
      dict so that it is easily serializable.'''
  def __init__(self, obj):
    ''' Create a new bundle with the given bundle name.'''
    if isinstance(obj, str) or isinstance(obj, unicode):
      dict.__init__(self, [(ARCHIVES_KEY, []), (NAME_KEY, obj)])
    else:
      dict.__init__(self, obj)

  def MergeWithBundle(self, bundle):
    '''Merge this bundle with |bundle|.

    Merges dict in |bundle| with this one in such a way that keys are not
    duplicated: the values of the keys in |bundle| take precedence in the
    returned dictionary.

    Any keys in either the symlink or links dictionaries that also exist in
    either of the files or dicts sets are removed from the latter, meaning that
    symlinks or links which overlap file or directory entries take precedence.

    Args:
      bundle: The other bundle.  Must be a dict.
    Returns:
      A dict which is the result of merging the two Bundles.
    '''
    return Bundle(self.items() + bundle.items())

  def CopyFrom(self, dict):
    ''' Update the content of the bundle by copying values from the given
        dictionary.

    Args:
      dict: The dictionary whose values must be copied to the bundle.'''
    for key, value in dict.items():
      if key == ARCHIVES_KEY:
        archives = []
        for a in value:
          new_archive = Archive(a['host_os'])
          new_archive.CopyFrom(a)
          archives.append(new_archive)
        self[ARCHIVES_KEY] = archives
      else:
        self[key] = value

  def Validate(self):
    ''' Validate the content of the bundle. Raise an Error if an invalid or
        missing field is found. '''
    # Check required fields.
    if not self.get(NAME_KEY, None):
      raise Error('Bundle has no name')
    if self.get(REVISION_KEY, None) == None:
      raise Error('Bundle "%s" is missing a revision number' % self[NAME_KEY])
    if self.get(VERSION_KEY, None) == None:
      raise Error('Bundle "%s" is missing a version number' % self[NAME_KEY])
    if not self.get('description', None):
      raise Error('Bundle "%s" is missing a description' % self[NAME_KEY])
    if not self.get('stability', None):
      raise Error('Bundle "%s" is missing stability info' % self[NAME_KEY])
    if self.get('recommended', None) == None:
      raise Error('Bundle "%s" is missing the recommended field' %
                  self[NAME_KEY])
    # Check specific values
    if self['stability'] not in STABILITY_LITERALS:
      raise Error('Bundle "%s" has invalid stability field: "%s"' %
                  (self[NAME_KEY], self['stability']))
    if self['recommended'] not in YES_NO_LITERALS:
      raise Error(
          'Bundle "%s" has invalid recommended field: "%s"' %
          (self[NAME_KEY], self['recommended']))
    # Verify that all key names are valid.
    for key, val in self.iteritems():
      if key not in VALID_BUNDLES_KEYS:
        raise Error('Bundle "%s" has invalid attribute "%s"' %
                    (self[NAME_KEY], key))
    # Validate the archives
    for archive in self[ARCHIVES_KEY]:
      archive.Validate()

  def GetArchive(self, host_os_name):
    ''' Retrieve the archive for the given host os.

    Args:
      host_os_name: name of host os whose archive must be retrieved.
    Return:
      An Archive instance or None if it doesn't exist.'''
    for archive in self[ARCHIVES_KEY]:
      if archive['host_os'] == host_os_name:
        return archive
    return None

  def UpdateArchive(self, host_os, url):
    ''' Update or create  the archive for host_os with the new url.
        Automatically updates the archive size and checksum info by downloading
        the data from the given archive. Raises an Error if the url is invalid.

    Args:
      host_os: name of host os whose archive must be updated or created.
      url: the new url for the archive.'''
    archive = self.GetArchive(host_os)
    if not archive:
      archive = Archive(host_os_name=host_os)
      self[ARCHIVES_KEY].append(archive)
    archive.Update(url)

  def GetArchives(self):
    '''Returns all the archives in this bundle'''
    return self[ARCHIVES_KEY]


class SDKManifest(object):
  '''This class contains utilities for manipulation an SDK manifest string

  For ease of unit-testing, this class should not contain any file I/O.
  '''

  def __init__(self):
    '''Create a new SDKManifest object with default contents'''
    self.MANIFEST_VERSION = MAJOR_REV
    self._manifest_data = {
        "manifest_version": self.MANIFEST_VERSION,
        "bundles": [],
        }

  def _ValidateManifest(self):
    '''Validate the Manifest file and raises an exception for problems'''
    # Validate the manifest top level
    if self._manifest_data["manifest_version"] > self.MANIFEST_VERSION:
      raise Error("Manifest version too high: %s" %
                              self._manifest_data["manifest_version"])
    # Verify that all key names are valid.
    for key, val in self._manifest_data.iteritems():
      if key not in VALID_MANIFEST_KEYS:
        raise Error('Manifest has invalid attribute "%s"' % key)
    # Validate each bundle
    for bundle in self._manifest_data[BUNDLES_KEY]:
      bundle.Validate()

  def GetBundle(self, name):
    ''' Get a bundle from the array of bundles.

    Args:
      name: the name of the bundle to return.
    Return:
      The first bundle with the given name, or None if it is not found.'''
    if not BUNDLES_KEY in self._manifest_data:
      return None
    bundles = filter(lambda b: b[NAME_KEY] == name,
                     self._manifest_data[BUNDLES_KEY])
    if len(bundles) > 1:
      WarningPrint("More than one bundle with name '%s' exists." % name)
    return bundles[0] if len(bundles) > 0 else None

  def SetBundle(self, new_bundle):
    '''Replace named bundle.  Add if absent.

    Args:
      name: Name of the bundle to replace or add.
      bundle: The bundle.
    '''
    name = new_bundle[NAME_KEY]
    if not BUNDLES_KEY in self._manifest_data:
      self._manifest_data[BUNDLES_KEY] = []
    bundles = self._manifest_data[BUNDLES_KEY]
    # Delete any bundles from the list, then add the new one.  This has the
    # effect of replacing the bundle if it already exists.  It also removes all
    # duplicate bundles.
    for i, bundle in enumerate(bundles):
      if bundle[NAME_KEY] == name:
        del bundles[i]
    bundles.append(new_bundle)

  def LoadManifestString(self, json_string, all_hosts=False):
    ''' Load a JSON manifest string. Raises an exception if json_string
        is not well-formed JSON.

    Args:
      json_string: a JSON-formatted string containing the previous manifest
      all_hosts: True indicates that we should load bundles for all hosts.
          False (default) says to only load bundles for the current host'''
    new_manifest = json.loads(json_string)
    for key, value in new_manifest.items():
      if key == BUNDLES_KEY:
        # Remap each bundle in |value| to a Bundle instance
        bundles = []
        for b in value:
          new_bundle = Bundle(b[NAME_KEY])
          new_bundle.CopyFrom(b)
          # Only add this archive if it's supported on this platform.
          # However, the sdk_tools bundle might not have an archive entry,
          # but is still always valid.
          if (all_hosts or new_bundle.GetArchive(GetHostOS()) or
              b[NAME_KEY] == 'sdk_tools'):
            bundles.append(new_bundle)
        self._manifest_data[key] = bundles
      else:
        self._manifest_data[key] = value
    self._ValidateManifest()

  def GetManifestString(self):
    '''Returns the current JSON manifest object, pretty-printed'''
    pretty_string = json.dumps(self._manifest_data, sort_keys=False, indent=2)
    # json.dumps sometimes returns trailing whitespace and does not put
    # a newline at the end.  This code fixes these problems.
    pretty_lines = pretty_string.split('\n')
    return '\n'.join([line.rstrip() for line in pretty_lines]) + '\n'


class SDKManifestFile(object):
  ''' This class provides basic file I/O support for manifest objects.'''

  def __init__(self, json_filepath):
    '''Create a new SDKManifest object with default contents.

    If |json_filepath| is specified, and it exists, its contents are loaded and
    used to initialize the internal manifest.

    Args:
      json_filepath: path to jason file to read/write, or None to write a new
          manifest file to stdout.
    '''
    self._json_filepath = json_filepath
    self._manifest = SDKManifest()
    if self._json_filepath:
      self._LoadFile()

  def _LoadFile(self):
    '''Load the manifest from the JSON file.

    This function returns quietly if the file doesn't exit.
    '''
    if not os.path.exists(self._json_filepath):
      return

    with open(self._json_filepath, 'r') as f:
      json_string = f.read()
    if json_string:
      self._manifest.LoadManifestString(json_string, all_hosts=True)

  def WriteFile(self):
    '''Write the json data to the file. If not file name was specified, the
       data is written to stdout.'''
    json_string = self._manifest.GetManifestString()
    if not self._json_filepath:
      # No file is specified; print the json data to stdout
      sys.stdout.write(json_string)
    else:
      # Write the JSON data to a temp file.
      temp_file_name = None
      # TODO(dspringer): Use file locks here so that multiple sdk_updates can
      # run at the same time.
      with tempfile.NamedTemporaryFile(mode='w', delete=False) as f:
        f.write(json_string)
        temp_file_name = f.name
      # Move the temp file to the actual file.
      if os.path.exists(self._json_filepath):
        os.remove(self._json_filepath)
      shutil.move(temp_file_name, self._json_filepath)

  def GetBundles(self):
    '''Return all the bundles in |_manifest|'''
    return self._manifest._manifest_data[BUNDLES_KEY]

  def GetBundleNamed(self, name):
    '''Return the first bundle named |name| or None if it doesn't exist'''
    return self._manifest.GetBundle(name)

  def BundleNeedsUpdate(self, bundle):
    '''Decides if a bundle needs to be updated.

    A bundle needs to be updated if it is not installed (doesn't exist in this
    manifest file) or if its revision is later than the revision in this file.

    Args:
      bundle: The Bundle to test.
    Returns:
      True if Bundle needs to be updated.
    '''
    if NAME_KEY not in bundle:
      raise KeyError("Bundle must have a 'name' key.")
    local_bundle = self.GetBundleNamed(bundle[NAME_KEY])
    return (local_bundle == None) or (
           (local_bundle[VERSION_KEY], local_bundle[REVISION_KEY]) <
           (bundle[VERSION_KEY], bundle[REVISION_KEY]))

  def MergeBundle(self, bundle):
    '''Merge a Bundle into this manifest.

    The new bundle is added if not present, or merged into the existing bundle.

    Args:
      bundle: The bundle to merge.
    '''
    if NAME_KEY not in bundle:
      raise KeyError("Bundle must have a 'name' key.")
    local_bundle = self.GetBundleNamed(bundle[NAME_KEY])
    if not local_bundle:
      self._manifest.SetBundle(bundle)
    else:
      self._manifest.SetBundle(local_bundle.MergeWithBundle(bundle))


class ManifestTools(object):
  '''Wrapper class for supporting the SDK manifest file'''

  def __init__(self, options):
    self._options = options
    self._manifest = SDKManifest()

  def LoadManifest(self):
    DebugPrint("Running LoadManifest")
    try:
      # TODO(mball): Add certificate validation on the server
      url_stream = urllib2.urlopen(self._options.manifest_url)
    except urllib2.URLError:
      raise Error('Unable to open %s' % self._options.manifest_url)

    manifest_stream = cStringIO.StringIO()
    sha1, size = DownloadAndComputeHash(
        url_stream, manifest_stream)
    self._manifest.LoadManifestString(manifest_stream.getvalue())

  def GetBundles(self):
    return self._manifest._manifest_data[BUNDLES_KEY]


#------------------------------------------------------------------------------
# Commands


def List(options, argv):
  '''Usage: %prog [options] list

  Lists the available SDK bundles that are available for download.'''
  def PrintBundles(bundles):
    for bundle in bundles:
      InfoPrint('  %s' % bundle[NAME_KEY])
      for key, value in bundle.iteritems():
        if key not in [ARCHIVES_KEY, NAME_KEY]:
          InfoPrint('    %s: %s' % (key, value))

  DebugPrint("Running List command with: %s, %s" %(options, argv))

  parser = optparse.OptionParser(usage=List.__doc__)
  (list_options, args) = parser.parse_args(argv)
  tools = ManifestTools(options)
  tools.LoadManifest()
  bundles = tools.GetBundles()
  InfoPrint('Available bundles:')
  PrintBundles(bundles)
  # Print the local information.
  local_manifest = SDKManifestFile(os.path.join(options.user_data_dir,
                                                options.manifest_filename))
  bundles = local_manifest.GetBundles()
  InfoPrint('\nCurrently installed bundles:')
  PrintBundles(bundles)


def Update(options, argv):
  '''Usage: %prog [options] update [target]

  Updates the Native Client SDK to a specified version.  By default, this
  command updates all the recommended components.  The update process works
  like this:
    1. Fetch the manifest from the mirror.
    2. Load manifest from USER_DATA_DIR - if there is no local manifest file,
       make an empty manifest object.
    3. Update each the bundle:
      for bundle in bundles:
        # Compare bundle versions & revisions.
        # Test if local version.revision < mirror OR local doesn't exist.
        if local_manifest < mirror_manifest:
          update(bundle)
          update local_manifest with mirror_manifest for bundle
          write manifest to disk.  Use locks.
        else:
          InfoPrint('bundle is up-to-date')

  Targets:
    recommended: (default) Install/Update all recommended components
    all:         Install/Update all available components
    bundle_name: Install/Update only the given bundle
  '''
  DebugPrint("Running Update command with: %s, %s" % (options, argv))
  ALL='all'  # Update all bundles
  RECOMMENDED='recommended'  # Only update the bundles with recommended=yes

  parser = optparse.OptionParser(usage=Update.__doc__)
  parser.add_option(
      '-F', '--force', dest='force',
      default=False, action='store_true',
      help='Force updating existing components that already exist')
  (update_options, args) = parser.parse_args(argv)
  if len(args) == 0:
    args = [RECOMMENDED]
  tools = ManifestTools(options)
  tools.LoadManifest()
  bundles = tools.GetBundles()
  local_manifest = SDKManifestFile(os.path.join(options.user_data_dir,
                                                options.manifest_filename))
  for bundle in bundles:
    bundle_name = bundle[NAME_KEY]
    bundle_path = os.path.join(options.sdk_root_dir, bundle_name)
    bundle_update_path = '%s_update' % bundle_path
    if not (bundle_name in args or
            ALL in args or (RECOMMENDED in args and
                            bundle[RECOMMENDED] == 'yes')):
      continue
    def UpdateBundle():
      '''Helper to install a bundle'''
      archive = bundle.GetArchive(GetHostOS())
      (scheme, host, path, _, _, _) = urlparse.urlparse(archive['url'])
      dest_filename = os.path.join(options.user_data_dir, path.split('/')[-1])
      sha1, size = archive.DownloadToFile(os.path.join(options.user_data_dir,
                                                       dest_filename))
      if sha1 != archive['checksum']['sha1']:
        raise Error("SHA1 checksum mismatch on '%s'.  Expected %s but got %s" %
                    (bundle_name, archive['checksum']['sha1'], sha1))
      if size != archive['size']:
        raise Error("Size mismatch on Archive.  Expected %s but got %s bytes" %
                    (archive['size'], size))
      InfoPrint('Updating bundle %s to version %s, revision %s' % (
                (bundle_name, bundle[VERSION_KEY], bundle[REVISION_KEY])))
      ExtractInstaller(dest_filename, bundle_update_path)
      if bundle_name != SDK_TOOLS:
        repath = bundle.get('repath', None)
        if repath:
          bundle_move_path = os.path.join(bundle_update_path, repath)
        else:
          bundle_move_path = bundle_update_path
        RenameDir(bundle_move_path, bundle_path)
        if os.path.exists(bundle_update_path):
          RemoveDir(bundle_update_path)
      os.remove(dest_filename)
      local_manifest.MergeBundle(bundle)
      local_manifest.WriteFile()
    # Test revision numbers, update the bundle accordingly.
    # TODO(dspringer): The local file should be refreshed from disk each
    # iteration thought this loop so that multiple sdk_updates can run at the
    # same time.
    if local_manifest.BundleNeedsUpdate(bundle):
      if (not update_options.force and os.path.exists(bundle_path) and
          bundle_name != SDK_TOOLS):
        WarningPrint('%s already exists, but has an update available.\n'
                     'Run update with the --force option to overwrite the '
                     'existing directory.\nWarning: This will overwrite any '
                     'modifications you have made within this directory.'
                     % bundle_name)
      else:
        UpdateBundle()
    else:
      InfoPrint('%s is already up-to-date.' % bundle_name)

  # Validate the arg list against the available bundle names.  Raises an
  # error if any invalid bundle names or args are detected.
  valid_args = set([ALL, RECOMMENDED] +
                   [bundle[NAME_KEY] for bundle in bundles])
  bad_args = set(args) - valid_args
  if len(bad_args) > 0:
    raise Error("Unrecognized bundle name or argument: '%s'" %
                ', '.join(bad_args))


#------------------------------------------------------------------------------
# Command-line interface


def main(argv):
  '''Main entry for the sdk_update utility'''
  parser = optparse.OptionParser(usage=GLOBAL_HELP)
  DEFAULT_SDK_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

  parser.add_option(
      '-U', '--manifest-url', dest='manifest_url',
      default='https://commondatastorage.googleapis.com/nativeclient-mirror/'
              'nacl/nacl_sdk/%s' % MANIFEST_FILENAME,
      help='override the default URL for the NaCl manifest file')
  parser.add_option(
      '-d', '--debug', dest='debug',
      default=False, action='store_true',
      help='enable displaying debug information to stderr')
  parser.add_option(
      '-q', '--quiet', dest='quiet',
      default=False, action='store_true',
      help='suppress displaying informational prints to stdout')
  parser.add_option(
      '-u', '--user-data-dir', dest='user_data_dir',
      # TODO(mball): the default should probably be in something like
      # ~/.naclsdk (linux), or ~/Library/Application Support/NaClSDK (mac),
      # or %HOMEPATH%\Application Data\NaClSDK (i.e., %APPDATA% on windows)
      default=os.path.join(DEFAULT_SDK_ROOT, USER_DATA_DIR),
      help="specify location of NaCl SDK's data directory")
  parser.add_option(
      '-s', '--sdk-root-dir', dest='sdk_root_dir',
      default=DEFAULT_SDK_ROOT,
      help="location where the SDK bundles are installed")
  parser.add_option(
      '-v', '--version', dest='show_version',
      action='store_true',
      help='show version information and exit')
  parser.add_option(
      '-m', '--manifest', dest='manifest_filename',
      default=MANIFEST_FILENAME,
      help="name of local manifest file relative to user-data-dir")

  COMMANDS = {
      'list': List,
      'update': Update,
      'install': Update,
      }

  # Separate global options from command-specific options
  global_argv = argv
  command_argv = []
  for index, arg in enumerate(argv):
    if arg in COMMANDS:
      global_argv = argv[:index]
      command_argv = argv[index:]
      break

  (options, args) = parser.parse_args(global_argv)
  args += command_argv

  global _debug_mode, _quiet_mode
  _debug_mode = options.debug
  _quiet_mode = options.quiet

  def PrintHelpAndExit(unused_options=None, unused_args=None):
    parser.print_help()
    exit(1)

  if options.show_version:
    print "Native Client SDK Updater, version %s.%s" % (MAJOR_REV, MINOR_REV)
    exit(0)

  if not args:
    print "Need to supply a command"
    PrintHelpAndExit()

  def DefaultHandler(unused_options=None, unused_args=None):
    print "Unknown Command: %s" % args[0]
    PrintHelpAndExit()

  def InvokeCommand(args):
    command = COMMANDS.get(args[0], DefaultHandler)
    command(options, args[1:])

  if args[0] == 'help':
    if len(args) == 1:
      PrintHelpAndExit()
    else:
      InvokeCommand([args[1], '-h'])
  else:
    # Make sure the user_data_dir exists.
    if not os.path.exists(options.user_data_dir):
      os.makedirs(options.user_data_dir)
    InvokeCommand(args)

  return 0  # Success


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except Error as error:
    print "Error: %s" % error
    sys.exit(1)
