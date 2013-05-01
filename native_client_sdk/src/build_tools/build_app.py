#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import re
import sys

if sys.version_info < (2, 6, 0):
  sys.stderr.write("python 2.6 or later is required run this script\n")
  sys.exit(1)

import buildbot_common
import build_projects
import build_utils
import easy_template
import parse_dsc

from build_paths import SDK_SRC_DIR, OUT_DIR, SDK_EXAMPLE_DIR

sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
import getos
import oshelpers


def RemoveBuildCruft(outdir):
  for root, _, files in os.walk(outdir):
    for f in files:
      path = os.path.join(root, f)
      ext = os.path.splitext(path)[1]
      if (ext in ('.d', '.o') or
          f == 'dir.stamp' or
          re.search(r'_unstripped_.*?\.nexe', f)):
        buildbot_common.RemoveFile(path)


def StripNexes(outdir, platform, pepperdir):
  for root, _, files in os.walk(outdir):
    for f in files:
      path = os.path.join(root, f)
      m = re.search(r'lib(32|64).*\.so', path)
      arch = None
      if m:
        # System .so file. Must be x86, because ARM doesn't support glibc yet.
        arch = 'x86_' + m.group(1)
      else:
        basename, ext = os.path.splitext(f)
        if ext in ('.nexe', '.so'):
          # We can get the arch from the filename...
          valid_arches = ('x86_64', 'x86_32', 'arm')
          for a in valid_arches:
            if basename.endswith(a):
              arch = a
              break
      if not arch:
        continue

      strip = GetStrip(pepperdir, platform, arch, 'newlib')
      buildbot_common.Run([strip, path])


def GetStrip(pepperdir, platform, arch, toolchain):
  base_arch = {'x86_32': 'x86', 'x86_64': 'x86', 'arm': 'arm'}[arch]
  bin_dir = os.path.join(pepperdir, 'toolchain',
                         '%s_%s_%s' % (platform, base_arch, toolchain), 'bin')
  strip_prefix = {'x86_32': 'i686', 'x86_64': 'x86_64', 'arm': 'arm'}[arch]
  strip_name = '%s-nacl-strip' % strip_prefix
  return os.path.join(bin_dir, strip_name)


def main(args):
  parser = optparse.OptionParser()
  _, args = parser.parse_args(args[1:])

  toolchains = ['newlib', 'glibc']

  pepper_ver = str(int(build_utils.ChromeMajorVersion()))
  pepperdir = os.path.join(OUT_DIR, 'pepper_' + pepper_ver)
  app_dir = os.path.join(OUT_DIR, 'naclsdk_app')
  app_examples_dir = os.path.join(app_dir, 'examples')
  sdk_resources_dir = os.path.join(SDK_EXAMPLE_DIR, 'resources')
  platform = getos.GetPlatform()

  buildbot_common.RemoveDir(app_dir)
  buildbot_common.MakeDir(app_dir)

  # Add some dummy directories so build_projects doesn't complain...
  buildbot_common.MakeDir(os.path.join(app_dir, 'tools'))
  buildbot_common.MakeDir(os.path.join(app_dir, 'toolchain'))

  config = 'Release'

  filters = {}
  filters['DISABLE_PACKAGE'] = False
  filters['EXPERIMENTAL'] = False
  filters['TOOLS'] = toolchains
  filters['DEST'] = ['examples/api', 'examples/getting_started',
                     'examples/demo', 'examples/tutorial']
  tree = parse_dsc.LoadProjectTree(SDK_SRC_DIR, filters=filters)
  build_projects.UpdateHelpers(app_dir, platform, clobber=True)
  build_projects.UpdateProjects(app_dir, platform, tree, clobber=False,
                                toolchains=toolchains, configs=[config],
                                first_toolchain=True)

  easy_template.RunTemplateFile(
      os.path.join(sdk_resources_dir, 'manifest.json.template'),
      os.path.join(app_examples_dir, 'manifest.json'),
      {'version': build_utils.ChromeVersionNoTrunk()})
  for filename in ['background.js', 'icon128.png']:
    buildbot_common.CopyFile(os.path.join(sdk_resources_dir, filename),
                             os.path.join(app_examples_dir, filename))

  os.environ['NACL_SDK_ROOT'] = pepperdir

  build_projects.BuildProjects(app_dir, platform, tree, deps=True, clean=False,
                               config=config)

  RemoveBuildCruft(app_dir)
  StripNexes(app_dir, platform, pepperdir)

  app_zip = os.path.join(app_dir, 'examples.zip')
  os.chdir(app_examples_dir)
  oshelpers.Zip([app_zip, '-r', '*'])

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
