#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import sys

import buildbot_common
import build_version
import generate_make
import parse_dsc

from build_paths import NACL_DIR, SDK_SRC_DIR, OUT_DIR, SDK_EXAMPLE_DIR
from build_paths import GSTORE
from generate_index import LandingPage

sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
sys.path.append(os.path.join(NACL_DIR, 'build'))
import getos
import http_download


MAKE = 'nacl_sdk/make_3_81/make.exe'
LIB_DICT = {
  'linux': [],
  'mac': [],
  'win': ['x86_32']
}


def CopyFilesFromTo(filelist, srcdir, dstdir):
  for filename in filelist:
    srcpath = os.path.join(srcdir, filename)
    dstpath = os.path.join(dstdir, filename)
    buildbot_common.CopyFile(srcpath, dstpath)


def UpdateHelpers(pepperdir, platform, clobber=False):
  if not os.path.exists(os.path.join(pepperdir, 'tools')):
    buildbot_common.ErrorExit('Examples depend on missing tools.')

  exampledir = os.path.join(pepperdir, 'examples')
  if clobber:
    buildbot_common.RemoveDir(exampledir)
  buildbot_common.MakeDir(exampledir)

  # Copy files for individual bild and landing page
  files = ['favicon.ico', 'httpd.cmd']
  CopyFilesFromTo(files, SDK_EXAMPLE_DIR, exampledir)

  resourcesdir = os.path.join(SDK_EXAMPLE_DIR, 'resources')
  files = ['index.css', 'index.js', 'button_close.png',
      'button_close_hover.png']
  CopyFilesFromTo(files, resourcesdir, exampledir)

  # Copy tools scripts and make includes
  buildbot_common.CopyDir(os.path.join(SDK_SRC_DIR, 'tools', '*.py'),
      os.path.join(pepperdir, 'tools'))
  buildbot_common.CopyDir(os.path.join(SDK_SRC_DIR, 'tools', '*.mk'),
      os.path.join(pepperdir, 'tools'))

  # On Windows add a prebuilt make
  if platform == 'win':
    buildbot_common.BuildStep('Add MAKE')
    http_download.HttpDownload(GSTORE + MAKE,
                               os.path.join(pepperdir, 'tools', 'make.exe'))


def UpdateProjects(pepperdir, platform, project_tree, toolchains,
                   clobber=False, configs=None):
  if configs is None:
    configs = ['Debug', 'Release']
  if not os.path.exists(os.path.join(pepperdir, 'tools')):
    buildbot_common.ErrorExit('Examples depend on missing tools.')
  if not os.path.exists(os.path.join(pepperdir, 'toolchain')):
    buildbot_common.ErrorExit('Examples depend on missing toolchains.')


  # Create the library output directories
  libdir = os.path.join(pepperdir, 'lib')
  for config in configs:
    for arch in LIB_DICT[platform]:
      dirpath = os.path.join(libdir, '%s_%s_host' % (platform, arch), config)
      if clobber:
        buildbot_common.RemoveDir(dirpath)
      buildbot_common.MakeDir(dirpath)

  for branch, projects in project_tree.iteritems():
    dirpath = os.path.join(pepperdir, branch)
    if clobber:
      buildbot_common.RemoveDir(dirpath)
    buildbot_common.MakeDir(dirpath)
    depth = len(branch.split('/'))
    targets = [desc['NAME'] for desc in projects]

    # Generate master make for this branch of projects
    generate_make.GenerateMasterMakefile(os.path.join(pepperdir, branch),
                                         targets, depth)

    if branch == 'examples':
      landing_page = LandingPage()

    # Generate individual projects
    for desc in projects:
      srcroot = os.path.dirname(desc['FILEPATH'])
      generate_make.ProcessProject(srcroot, pepperdir, desc, toolchains)

      if branch == 'examples':
        landing_page.AddDesc(desc)

    if branch == 'examples':
      # Generate the landing page text file.
      index_html = os.path.join(pepperdir, 'examples', 'index.html')
      example_resources_dir = os.path.join(SDK_EXAMPLE_DIR, 'resources')
      index_template = os.path.join(example_resources_dir,
                                    'index.html.template')
      with open(index_html, 'w') as fh:
        out = landing_page.GeneratePage(index_template)
        fh.write(out)
      print 'Generated landing page.'

  #
  # TODO(noelallen) Add back in once we move examples
  # Generate master example make
  # example_targets = ['api', 'demos', 'getting_started', 'tutorials']
  # example_targets = [x for x in example_targets if 'examples/'+x in desc_tree]
  # print 'Example targets: ' + str(example_targets)
  # generate_make.GenerateMasterMakefile(os.path.join(pepperdir, 'examples'),
  #                                     example_targets, 2)


def BuildProjects(pepperdir, platform, project_tree, deps=True,
                  clean=False, config='Debug'):
  for branch in project_tree:
    make_dir = os.path.join(pepperdir, branch)
    print "\n\nMake: " + make_dir
    if platform == 'win':
      # We need to modify the environment to build host on Windows.
      make = os.path.join(make_dir, 'make.bat')
    else:
      make = 'make'

    extra_args = ['CONFIG='+config]
    if not deps:
      extra_args += ['IGNORE_DEPS=1']

    try:
      buildbot_common.Run([make, '-j8', 'all_versions'] + extra_args,
                          cwd=make_dir)
    except:
      print 'Failed to build ' + branch
      raise

    if clean:
      # Clean to remove temporary files but keep the built
      buildbot_common.Run([make, '-j8', 'clean'] + extra_args,
                          cwd=make_dir)


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--clobber',
      help='Clobber project directories before copying new files',
      action='store_true', default=False)
  parser.add_option('-b', '--build',
      help='Build the projects.', action='store_true')
  parser.add_option('-x', '--experimental',
      help='Build experimental projects', action='store_true')
  parser.add_option('-t', '--toolchain',
      help='Build using toolchain. Can be passed more than once.',
      action='append', default=[])
  parser.add_option('-d', '--dest',
      help='Select which build destinations (project types) are valid.',
      action='append')
  parser.add_option('-p', '--project',
      help='Select which projects are valid.',
      action='append')

  options, files = parser.parse_args(args[1:])
  if len(files):
    parser.error('Not expecting files.')
    return 1

  pepper_ver = str(int(build_version.ChromeMajorVersion()))
  pepperdir = os.path.join(OUT_DIR, 'pepper_' + pepper_ver)
  platform = getos.GetPlatform()

  if not options.toolchain:
    options.toolchain = ['newlib', 'glibc', 'pnacl', 'host']

  if 'host' in options.toolchain:
    options.toolchain.append(platform)
    print 'Adding platform: ' + platform

  filters = {}
  if options.toolchain:
    filters['TOOLS'] = options.toolchain
    print 'Filter by toolchain: ' + str(options.toolchain)
  if not options.experimental:
    filters['EXPERIMENTAL'] = False
  if options.dest:
    filters['DEST'] = options.dest
    print 'Filter by type: ' + str(options.dest)
  if options.project:
    filters['NAME'] = options.project
    print 'Filter by name: ' + str(options.project)

  project_tree = parse_dsc.LoadProjectTree(SDK_SRC_DIR, filters=filters)
  parse_dsc.PrintProjectTree(project_tree)

  UpdateHelpers(pepperdir, platform, clobber=options.clobber)
  UpdateProjects(pepperdir, platform, project_tree, options.toolchain,
                 clobber=options.clobber)
  if options.build:
    BuildProjects(pepperdir, platform, project_tree)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
