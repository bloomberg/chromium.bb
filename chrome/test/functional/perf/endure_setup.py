#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Automate the setup process of Chrome Endure environment.

Usage:
  python endure_setup.py [option]

We use <ENDURE_DIR> to refer to the root directory in which Chrome Endure
is set up. By default, <ENDURE_DIR> is the current working directory.

First, run:
  >python endure_setup.py
This command will automatically setup Chrome Endure in <ENDURE_DIR>.

Next, run your first endure test by:
  >TEST_LENGTH=30 LOCAL_PERF_DIR="<ENDURE_DIR>/chrome_graph" \\
   python <ENDURE_DIR>/src/chrome/test/functional/perf_endure.py \\
   perf_endure.ChromeEndureGmailTest.testGmailComposeDiscard \\
The above commands runs a Chrome Endure test for 30 seconds and saves
the results to <ENDURE_DIR>/chrome_graph.

Last, to view the graphs, run another script endure_server.py
within <ENDURE_DIR> to start a local HTTP server that serves
the graph directory, see endure_server.py for details.

Use python endure_setup.py --help for more options.

This script depends on the following modules
(which will be downloaded automatically):
  depot_tools
  src/chrome/test/pyautolib/fetch_prebuilt_pyauto.py

Supported platforms: Linux and Linux_x64.
"""

import logging
import optparse
import os
import platform
import shutil
import subprocess
import sys
import urllib
import urllib2
import zipfile

URLS = {'depot_tools': ('http://src.chromium.org'
                        '/chrome/trunk/tools/depot_tools'),
        'pyauto': ('https://src.chromium.org/'
                   'chrome/trunk/src/chrome/test/functional.DEPS'),
        'binary': ('http://commondatastorage.googleapis.com/'
                   'chromium-browser-continuous/{os_type}/{revision}'),
       }


class SetupError(Exception):
  """Catch errors in setting up Chrome Endure."""
  pass


class HelpFormatter(optparse.IndentedHelpFormatter):
  """Format the help message of this script."""

  def format_description(self, description):
    """Override to keep the original format of the description."""
    return description + '\n' if description else ''


def Main(argv):
  """Fetch Chrome Endure.

  Usage:
    python endure_setup.py [options]

  Examples:
    >python endure_setup.py
      Fetch the latest version of Chrome Endure to the current
      working directory.

    >python endure_setup.py --endure-dir=/home/user/endure_dir
      Fetch the latest version of Chrome Endure to /home/user/endure_dir.
  """
  parser = optparse.OptionParser(
      formatter=HelpFormatter(), description=Main.__doc__)
  parser.add_option(
      '-d', '--endure-dir', type='string', default=os.getcwd(),
      help='Directory in which to setup or update. ' \
           'Default value is the current working directory.')
  # TODO(fdeng): remove this option once the Chrome Endure
  # graphing code is checked into chrome tree.
  parser.add_option(
      '-g', '--graph-zip-url', type='string', default=None,
      help='URL to a zip file containing the chrome graphs.')
  os_type = GetCurrentOSType()
  if not os_type.startswith('Linux'):
    raise SetupError('Only support Linux or Linux_x64, %s found'
                     % os_type)
  options, _ = parser.parse_args(argv)
  endure_dir = os.path.abspath(options.endure_dir)
  depot_dir = os.path.join(endure_dir, 'depot_tools')
  gclient = os.path.join(depot_dir, 'gclient')
  fetch_py = os.path.join(endure_dir, 'src', 'chrome',
                          'test', 'pyautolib',
                          'fetch_prebuilt_pyauto.py')
  binary_dir = os.path.join(endure_dir, 'src', 'out', 'Release')
  graph_zip_url = options.graph_zip_url
  graph_dir = os.path.join(endure_dir, 'chrome_graph')

  if not os.path.isdir(endure_dir):
    os.makedirs(endure_dir)

  logging.info('Fetching depot tools...')
  FetchDepot(depot_dir)
  logging.info('Fetching PyAuto (python code)...')
  FetchPyAuto(gclient, endure_dir)
  logging.info('Fetching binaries(chrome, pyautolib, chrome driver)...')
  FetchBinaries(fetch_py, binary_dir, os_type)
  # TODO(fdeng): remove this after it is checked into the chrome tree.
  logging.info('Fetching chrome graphing files...')
  FetchGraph(graph_zip_url, graph_dir)
  return 0


def FetchDepot(depot_dir):
  """Fetch depot_tools.

  Args:
    depot_dir: The directory where depot_tools will be checked out.

  Raises:
    SetupError: If fail.
  """
  if subprocess.call(['svn', 'co', URLS['depot_tools'], depot_dir]) != 0:
    raise SetupError('Error found when checking out depot_tools.')
  if not CheckDepot(depot_dir):
    raise SetupError('Could not get depot_tools.')


def CheckDepot(depot_dir):
  """Check that some expected depot_tools files exist.

  Args:
    depot_dir: The directory where depot_tools are checked out.

  Returns:
    True if check passes otherwise False.
  """
  gclient = os.path.join(depot_dir, 'gclient')
  gclient_py = os.path.join(depot_dir, 'gclient.py')
  files = [gclient, gclient_py]
  for f in files:
    if not os.path.exists(f):
      return False
  try:
    subprocess.call([gclient, '--version'])
  except OSError:
    return False
  return True


def FetchPyAuto(gclient, endure_dir):
  """Use gclient to fetch python code.

  Args:
    gclient: The path to the gclient executable.
    endure_dir: Directory where Chrome Endure and
                its dependencies will be checked out.

  Raises:
    SetupError: if fails.
  """
  cur_dir = os.getcwd()
  os.chdir(endure_dir)
  config_cmd = [gclient, 'config', URLS['pyauto']]
  if subprocess.call(config_cmd) != 0:
    raise SetupError('Running "%s" failed.' % ' '.join(config_cmd))
  sync_cmd = [gclient, 'sync']
  if subprocess.call(sync_cmd) != 0:
    raise SetupError('Running "%s" failed.' % ' '.join(sync_cmd))
  CheckPyAuto(endure_dir)
  logging.info('Sync PyAuto python code done.')
  os.chdir(cur_dir)


def CheckPyAuto(endure_dir):
  """Sanity check for Chrome Endure code.

  Args:
    endure_dir: Directory of Chrome Endure and its dependencies.

  Raises:
    SetupError: If fails.
  """
  fetch_py = os.path.join(endure_dir, 'src', 'chrome',
                          'test', 'pyautolib',
                          'fetch_prebuilt_pyauto.py')
  pyauto_py = os.path.join(endure_dir, 'src',
                           'chrome', 'test',
                           'pyautolib', 'pyauto.py')
  files = [fetch_py, pyauto_py]
  for f in files:
    if not os.path.exists(f):
      raise SetupError('Checking %s failed.' % f)


def FetchBinaries(fetch_py, binary_dir, os_type):
  """Get the prebuilt binaries from continuous build archive.

  Args:
    fetch_py: Path to the script which fetches pre-built binaries.
    binary_dir: Directory of the pre-built binaries.
    os_type: 'Mac', 'Win', 'Linux', 'Linux_x64'.

  Raises:
    SetupError: If fails.
  """
  revision = GetLatestRevision(os_type)
  logging.info('Cleaning %s', binary_dir)
  if os.path.exists(binary_dir):
    shutil.rmtree(binary_dir)
  logging.info('Downloading binaries...')
  cmd = [fetch_py, '-d', binary_dir,
         URLS['binary'].format(
             os_type=os_type, revision=revision)]
  if subprocess.call(cmd) == 0 and os.path.exists(binary_dir):
    logging.info('Binaries at revision %s', revision)
  else:
    raise SetupError('Running "%s" failed.' % ' '.join(cmd))


def FetchGraph(graph_zip_url, graph_dir):
  """Fetch graph code.

  Args:
    graph_zip_url: The url to a zip file containing the chrome graphs.
    graph_dir: Directory of the chrome graphs.

  Raises:
    SetupError: if unable to retrive the zip file.
  """
  # TODO(fdeng): remove this function once chrome graph
  # is checked into chrome tree.
  if not graph_zip_url:
    logging.info(
        'Skip fetching chrome graphs' +
        ' since --graph-zip-url is not set.')
    return
  graph_zip = urllib.urlretrieve(graph_zip_url)[0]
  if graph_zip is None or not os.path.exists(graph_zip):
    raise SetupError('Unable to retrieve %s' % graph_zip_url)
  if not os.path.exists(graph_dir):
    os.mkdir(graph_dir)
  UnzipFilenameToDir(graph_zip, graph_dir)
  logging.info('Graph code is downloaded to %s', graph_dir)


def GetCurrentOSType():
  """Get a string representation for the current OS.

  Returns:
    'Mac', 'Win', 'Linux', or 'Linux_64'.

  Raises:
    RuntimeError: if OS can't be identified.
  """
  if sys.platform == 'darwin':
    os_type = 'Mac'
  if sys.platform == 'win32':
    os_type = 'Win'
  if sys.platform.startswith('linux'):
    os_type = 'Linux'
    if platform.architecture()[0] == '64bit':
      os_type += '_x64'
  else:
    raise RuntimeError('Unknown platform')
  return os_type


def GetLatestRevision(os_type):
  """Figure out the latest revision number of the prebuilt binary archive.

  Args:
    os_type: 'Mac', 'Win', 'Linux', or 'Linux_64'.

  Returns:
    A string of latest revision number.

  Raises:
    SetupError: If unable to get the latest revision number.
  """
  last_change_url = ('http://commondatastorage.googleapis.com/'
                     'chromium-browser-continuous/%s/LAST_CHANGE' % os_type)
  response = urllib2.urlopen(last_change_url)
  last_change = response.read()
  if not last_change:
    raise SetupError('Unable to get the latest revision number from %s' %
                     last_change_url)
  return last_change


def UnzipFilenameToDir(filename, directory):
  """Unzip |filename| to directory |directory|.

  This works with as low as python2.4 (used on win).
  (Code is adapted from fetch_prebuilt_pyauto.py)
  """
  # TODO(fdeng): remove this function as soon as the Chrome Endure
  # graphing code is checked into the chrome tree.
  zf = zipfile.ZipFile(filename)
  pushd = os.getcwd()
  if not os.path.isdir(directory):
    os.mkdir(directory)
  os.chdir(directory)
  # Extract files.
  for info in zf.infolist():
    name = info.filename
    if name.endswith('/'):  # dir
      if not os.path.isdir(name):
        os.makedirs(name)
    else:  # file
      directory = os.path.dirname(name)
      if directory and not os.path.isdir(directory):
        os.makedirs(directory)
      out = open(name, 'wb')
      out.write(zf.read(name))
      out.close()
    # Set permissions. Permission info in external_attr is shifted 16 bits.
    os.chmod(name, info.external_attr >> 16L)
  os.chdir(pushd)


if '__main__' == __name__:
  logging.basicConfig(format='[%(levelname)s] %(message)s', level=logging.DEBUG)
  sys.exit(Main(sys.argv[1:]))
