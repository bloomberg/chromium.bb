#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Pack ARM relative relocations in a library (or copy unchanged).

If --enable-packing and --configuration-name=='Release', invoke the
relocation_packer tool to pack the .rel.dyn or .rela.dyn section in the given
library files.  This step is inserted after the libraries are stripped.
Packing adds a new .android.rel.dyn or .android.rela.dyn section to the file
and reduces the size of .rel.dyn or .rela.dyn accordingly.

Currently packing only understands ARM32 shared libraries.  For all other
architectures --enable-packing should be set to zero.  In this case the
script copies files verbatim, with no attempt to pack relative relocations.

Any library listed in --exclude-packing-list is also copied verbatim,
irrespective of any --enable-packing setting.  Typically this would be
'libchromium_android_linker.so'.
"""

import optparse
import os
import shlex
import shutil
import sys
import tempfile

from util import build_utils

def PackArmLibraryRelocations(android_pack_relocations,
                              android_objcopy,
                              has_relocations_with_addends,
                              library_path,
                              output_path):
  # Select an appropriate name for the section we add.
  if has_relocations_with_addends:
    new_section = '.android.rela.dyn'
  else:
    new_section = '.android.rel.dyn'

  # Copy and add a 'NULL' packed relocations section for the packing tool.
  with tempfile.NamedTemporaryFile() as stream:
    stream.write('NULL')
    stream.flush()
    objcopy_command = [android_objcopy,
                       '--add-section', '%s=%s' % (new_section, stream.name),
                       library_path, output_path]
    build_utils.CheckOutput(objcopy_command)

  # Pack R_ARM_RELATIVE relocations.
  pack_command = [android_pack_relocations, output_path]
  build_utils.CheckOutput(pack_command)


def CopyArmLibraryUnchanged(library_path, output_path):
  shutil.copy(library_path, output_path)


def main(args):
  args = build_utils.ExpandFileArgs(args)
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)
  parser.add_option('--clear-dir', action='store_true',
                    help='If set, the destination directory will be deleted '
                    'before copying files to it. This is highly recommended to '
                    'ensure that no stale files are left in the directory.')

  parser.add_option('--configuration-name',
      default='Release',
      help='Gyp configuration name (i.e. Debug, Release)')
  parser.add_option('--enable-packing',
      choices=['0', '1'],
      help=('Pack relocations if 1 and configuration name is \'Release\','
            ' otherwise plain file copy'))
  parser.add_option('--has-relocations-with-addends',
      choices=['0', '1'],
      help=('Pack into \'.android.rela.dyn\' if 1, else \'.android.rel.dyn\''))
  parser.add_option('--exclude-packing-list',
      default='',
      help='Names of any libraries explicitly not packed')
  parser.add_option('--android-pack-relocations',
      help='Path to the ARM relocations packer binary')
  parser.add_option('--android-objcopy',
      help='Path to the toolchain\'s objcopy binary')
  parser.add_option('--stripped-libraries-dir',
      help='Directory for stripped libraries')
  parser.add_option('--packed-libraries-dir',
      help='Directory for packed libraries')
  parser.add_option('--libraries', action='append',
      help='List of libraries')
  parser.add_option('--stamp', help='Path to touch on success')

  options, _ = parser.parse_args(args)
  enable_packing = (options.enable_packing == '1' and
                    options.configuration_name == 'Release')
  has_relocations_with_addends = (options.has_relocations_with_addends == '1')
  exclude_packing_set = set(shlex.split(options.exclude_packing_list))

  libraries = []
  for libs_arg in options.libraries:
    libraries += build_utils.ParseGypList(libs_arg)

  if options.clear_dir:
    build_utils.DeleteDirectory(options.packed_libraries_dir)

  build_utils.MakeDirectory(options.packed_libraries_dir)

  for library in libraries:
    library_path = os.path.join(options.stripped_libraries_dir, library)
    output_path = os.path.join(
        options.packed_libraries_dir, os.path.basename(library))

    if enable_packing and library not in exclude_packing_set:
      PackArmLibraryRelocations(options.android_pack_relocations,
                                options.android_objcopy,
                                has_relocations_with_addends,
                                library_path,
                                output_path)
    else:
      CopyArmLibraryUnchanged(library_path, output_path)

  if options.depfile:
    build_utils.WriteDepfile(
        options.depfile,
        libraries + build_utils.GetPythonDependencies())

  if options.stamp:
    build_utils.Touch(options.stamp)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
