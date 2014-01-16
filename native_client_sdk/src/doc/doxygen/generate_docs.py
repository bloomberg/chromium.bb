#!/usr/bin/python

# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import json
import optparse
import os
import shutil
import subprocess
import sys
import urllib2


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DOC_DIR = os.path.dirname(SCRIPT_DIR)


ChannelInfo = collections.namedtuple('ChannelInfo', ['branch', 'version'])


def Trace(msg):
  if Trace.verbose:
    sys.stderr.write(str(msg) + '\n')

Trace.verbose = False


def GetChannelInfo():
  url = 'http://omahaproxy.appspot.com/json'
  u = urllib2.urlopen(url)
  try:
    data = json.loads(u.read())
  finally:
    u.close()

  channel_info = {}
  for os_row in data:
    osname = os_row['os']
    if osname not in ('win', 'mac', 'linux'):
      continue
    for version_row in os_row['versions']:
      channel = version_row['channel']
      # We don't display canary docs.
      if channel == 'canary':
        continue

      version = version_row['version'].split('.')[0]  # Major version
      branch = version_row['true_branch']
      if branch is None:
        branch = 'trunk'

      if channel in channel_info:
        existing_info = channel_info[channel]
        if branch != existing_info.branch:
          sys.stderr.write('Warning: found different branch numbers for '
              'channel %s: %s vs %s. Using %s.\n' % (
              channel, branch, existing_info.branch, existing_info.branch))
      else:
        channel_info[channel] = ChannelInfo(branch, version)

  return channel_info


def RemoveFile(filename):
  if os.path.exists(filename):
    os.remove(filename)


def RemoveDir(dirname):
  if os.path.exists(dirname):
    shutil.rmtree(dirname)


def GetSVNRepositoryRoot(branch):
  if branch == 'trunk':
    return 'http://src.chromium.org/chrome/trunk/src'
  return 'http://src.chromium.org/chrome/branches/%s/src' % branch


def CheckoutPepperDocs(branch, doc_dirname):
  Trace('Removing directory %s' % doc_dirname)
  RemoveDir(doc_dirname)

  svn_root_url = GetSVNRepositoryRoot(branch)

  for subdir in ('api', 'generators', 'cpp', 'utility'):
    url = svn_root_url + '/ppapi/%s' % subdir
    cmd = ['svn', 'co', url, os.path.join(doc_dirname, subdir)]
    Trace('Checking out docs into %s:\n  %s' % (doc_dirname, ' '.join(cmd)))
    subprocess.check_call(cmd)

  # The IDL generator needs PLY (a python lexing library); check it out into
  # generators.
  url = svn_root_url + '/third_party/ply'
  ply_dirname = os.path.join(doc_dirname, 'generators', 'ply')
  cmd = ['svn', 'co', url, ply_dirname]
  Trace('Checking out PLY into %s:\n  %s' % (ply_dirname, ' '.join(cmd)))
  subprocess.check_call(cmd)


def GenerateCHeaders(pepper_version, doc_dirname):
  script = os.path.join(os.pardir, 'generators', 'generator.py')
  cwd = os.path.join(doc_dirname, 'api')
  out_dirname = os.path.join(os.pardir, 'c')
  cmd = [sys.executable, script, '--cgen', '--release', 'M' + pepper_version,
         '--wnone', '--dstroot', out_dirname]
  Trace('Generating C Headers for version %s\n  %s' % (
      pepper_version, ' '.join(cmd)))
  subprocess.check_call(cmd, cwd=cwd)


def GenerateDoxyfile(template_filename, out_dirname, doc_dirname, doxyfile):
  Trace('Writing Doxyfile "%s" (from template %s)' % (
    doxyfile, template_filename))

  with open(template_filename) as f:
    data = f.read()

  with open(doxyfile, 'w') as f:
    f.write(data % {'out_dirname': out_dirname, 'doc_dirname': doc_dirname})


def RunDoxygen(out_dirname, doxyfile):
  Trace('Removing old output directory %s' % out_dirname)
  RemoveDir(out_dirname)

  Trace('Making new output directory %s' % out_dirname)
  os.makedirs(out_dirname)

  cmd = ['doxygen', doxyfile]
  Trace('Running Doxygen:\n  %s' % ' '.join(cmd))
  subprocess.check_call(cmd)


def RunDoxyCleanup(out_dirname):
  cmd = [sys.executable, 'doxy_cleanup.py', out_dirname]
  if Trace.verbose:
    cmd.append('-v')
  Trace('Running doxy_cleanup:\n  %s' % ' '.join(cmd))
  subprocess.check_call(cmd)


def RunRstIndex(kind, channel, pepper_version, out_dirname, out_rst_filename):
  assert kind in ('root', 'c', 'cpp')
  cmd = [sys.executable, 'rst_index.py',
         '--' + kind,
         '--channel', channel,
         '--version', pepper_version,
         out_dirname,
         '-o', out_rst_filename]
  Trace('Running rst_index:\n  %s' % ' '.join(cmd))
  subprocess.check_call(cmd)


def GenerateDocs(channel, pepper_version, branch):
  Trace('Generating docs for %s (branch %s)' % (channel, branch))
  pepper_dirname = 'pepper_%s' % channel
  # i.e. ../_build/chromesite/pepper_beta
  chromesite_dir = os.path.join(DOC_DIR, '_build', 'chromesite', pepper_dirname)

  CheckoutPepperDocs(branch, pepper_dirname)
  GenerateCHeaders(pepper_version, pepper_dirname)

  doxyfile_c = ''
  doxyfile_cpp = ''

  try:
    # Generate Root index
    rst_index_root = os.path.join(DOC_DIR, pepper_dirname, 'index.rst')
    RunRstIndex('root', channel, pepper_version, chromesite_dir, rst_index_root)

    # Generate C docs
    out_dirname_c = os.path.join(chromesite_dir, 'c')
    doxyfile_c = 'Doxyfile.c.%s' % channel
    rst_index_c = os.path.join(DOC_DIR, pepper_dirname, 'c', 'index.rst')
    GenerateDoxyfile('Doxyfile.c.template', out_dirname_c, pepper_dirname,
                     doxyfile_c)
    RunDoxygen(out_dirname_c, doxyfile_c)
    RunDoxyCleanup(out_dirname_c)
    RunRstIndex('c', channel, pepper_version, out_dirname_c, rst_index_c)

    # Generate C++ docs
    out_dirname_cpp = os.path.join(chromesite_dir, 'cpp')
    doxyfile_cpp = 'Doxyfile.cpp.%s' % channel
    rst_index_cpp = os.path.join(DOC_DIR, pepper_dirname, 'cpp', 'index.rst')
    GenerateDoxyfile('Doxyfile.cpp.template', out_dirname_cpp, pepper_dirname,
                     doxyfile_cpp)
    RunDoxygen(out_dirname_cpp, doxyfile_cpp)
    RunDoxyCleanup(out_dirname_cpp)
    RunRstIndex('cpp', channel, pepper_version, out_dirname_cpp, rst_index_cpp)
  finally:
    # Cleanup
    RemoveDir(pepper_dirname)
    RemoveFile(doxyfile_c)
    RemoveFile(doxyfile_cpp)


def main(argv):
  parser = optparse.OptionParser(usage='Usage: %prog [options]')
  parser.add_option('-v', '--verbose',
                    help='Verbose output', action='store_true')
  options, _ = parser.parse_args(argv)

  if options.verbose:
    Trace.verbose = True

  channel_info = GetChannelInfo()
  for channel, info in channel_info.iteritems():
    GenerateDocs(channel, info.version, info.branch)

  return 0


if __name__ == '__main__':
  try:
    rtn = main(sys.argv[1:])
  except KeyboardInterrupt:
    sys.stderr.write('%s: interrupted\n' % os.path.basename(__file__))
    rtn = 1
  sys.exit(rtn)
