#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import optparse
import os
import re
import shutil
import sys
import tempfile
import zipfile

from util import build_utils

sys.path.insert(1, os.path.join(os.path.dirname(__file__), os.path.pardir))

import convert_dex_profile


def _CheckFilePathEndsWithJar(parser, file_path):
  if not file_path.endswith(".jar"):
    parser.error("%s does not end in .jar" % file_path)


def _CheckFilePathsEndWithJar(parser, file_paths):
  for file_path in file_paths:
    _CheckFilePathEndsWithJar(parser, file_path)


def _ParseArgs(args):
  args = build_utils.ExpandFileArgs(args)

  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)

  parser.add_option('--output-directory',
                    default=os.getcwd(),
                    help='Path to the output build directory.')
  parser.add_option('--dex-path', help='Dex output path.')
  parser.add_option('--configuration-name',
                    help='The build CONFIGURATION_NAME.')
  parser.add_option('--proguard-enabled',
                    help='"true" if proguard is enabled.')
  parser.add_option('--debug-build-proguard-enabled',
                    help='"true" if proguard is enabled for debug build.')
  parser.add_option('--proguard-enabled-input-path',
                    help=('Path to dex in Release mode when proguard '
                          'is enabled.'))
  parser.add_option('--inputs', help='A list of additional input paths.')
  parser.add_option('--excluded-paths',
                    help='A list of paths to exclude from the dex file.')
  parser.add_option('--main-dex-list-path',
                    help='A file containing a list of the classes to '
                         'include in the main dex.')
  parser.add_option('--multidex-configuration-path',
                    help='A JSON file containing multidex build configuration.')
  parser.add_option('--multi-dex', default=False, action='store_true',
                    help='Generate multiple dex files.')
  parser.add_option('--d8-jar-path', help='Path to D8 jar.')
  parser.add_option('--release', action='store_true', default=False,
                    help='Run D8 in release mode. Release mode maximises main '
                    'dex and deletes non-essential line number information '
                    '(vs debug which minimizes main dex and keeps all line '
                    'number information, and then some.')
  parser.add_option('--min-api',
                    help='Minimum Android API level compatibility.')

  parser.add_option('--dexlayout-profile',
                    help=('Text profile for dexlayout. If present, a dexlayout '
                          'pass will happen'))
  parser.add_option('--profman-path',
                    help=('Path to ART profman binary. There should be a '
                          'lib/ directory at the same path containing shared '
                          'libraries (shared with dexlayout).'))
  parser.add_option('--dexlayout-path',
                    help=('Path to ART dexlayout binary. There should be a '
                          'lib/ directory at the same path containing shared '
                          'libraries (shared with dexlayout).'))
  parser.add_option('--dexdump-path', help='Path to dexdump binary.')
  parser.add_option(
      '--proguard-mapping-path',
      help=('Path to proguard map from obfuscated symbols in the jar to '
            'unobfuscated symbols present in the code. If not '
            'present, the jar is assumed not to be obfuscated.'))

  options, paths = parser.parse_args(args)

  required_options = ('d8_jar_path',)
  build_utils.CheckOptions(options, parser, required=required_options)

  if options.dexlayout_profile:
    build_utils.CheckOptions(
        options,
        parser,
        required=('profman_path', 'dexlayout_path', 'dexdump_path'))
  elif options.proguard_mapping_path is not None:
    raise Exception('Unexpected proguard mapping without dexlayout')

  if options.multidex_configuration_path:
    with open(options.multidex_configuration_path) as multidex_config_file:
      multidex_config = json.loads(multidex_config_file.read())
    options.multi_dex = multidex_config.get('enabled', False)

  if options.main_dex_list_path and not options.multi_dex:
    logging.warning('--main-dex-list-path is unused if multidex is not enabled')

  if options.inputs:
    options.inputs = build_utils.ParseGnList(options.inputs)
    _CheckFilePathsEndWithJar(parser, options.inputs)
  if options.excluded_paths:
    options.excluded_paths = build_utils.ParseGnList(options.excluded_paths)

  if options.proguard_enabled_input_path:
    _CheckFilePathEndsWithJar(parser, options.proguard_enabled_input_path)
  _CheckFilePathsEndWithJar(parser, paths)

  return options, paths


def _MoveTempDexFile(tmp_dex_dir, dex_path):
  """Move the temp dex file out of |tmp_dex_dir|.

  Args:
    tmp_dex_dir: Path to temporary directory created with tempfile.mkdtemp().
      The directory should have just a single file.
    dex_path: Target path to move dex file to.

  Raises:
    Exception if there are multiple files in |tmp_dex_dir|.
  """
  tempfiles = os.listdir(tmp_dex_dir)
  if len(tempfiles) > 1:
    raise Exception('%d files created, expected 1' % len(tempfiles))

  tmp_dex_path = os.path.join(tmp_dex_dir, tempfiles[0])
  shutil.move(tmp_dex_path, dex_path)


def _NoClassFiles(jar_paths):
  """Returns True if there are no .class files in the given JARs.

  Args:
    jar_paths: list of strings representing JAR file paths.

  Returns:
    (bool) True if no .class files are found.
  """
  for jar_path in jar_paths:
    with zipfile.ZipFile(jar_path) as jar:
      if any(name.endswith('.class') for name in jar.namelist()):
        return False
  return True


def _RunD8(dex_cmd, input_paths, output_path):
  dex_cmd += ['--output', output_path]
  dex_cmd += input_paths
  build_utils.CheckOutput(dex_cmd, print_stderr=False)


def _EnvWithArtLibPath(binary_path):
  """Return an environment dictionary for ART host shared libraries.

  Args:
    binary_path: the path to an ART host binary.

  Returns:
    An environment dictionary where LD_LIBRARY_PATH has been augmented with the
    shared library path for the binary. This assumes that there is a lib/
    directory in the same location as the binary.
  """
  lib_path = os.path.join(os.path.dirname(binary_path), 'lib')
  env = os.environ.copy()
  libraries = [l for l in env.get('LD_LIBRARY_PATH', '').split(':') if l]
  libraries.append(lib_path)
  env['LD_LIBRARY_PATH'] = ':'.join(libraries)
  return env


def _CreateBinaryProfile(text_profile, input_dex, profman_path, temp_dir):
  """Create a binary profile for dexlayout.

  Args:
    text_profile: The ART text profile that will be converted to a binary
        profile.
    input_dex: The input dex file to layout.
    profman_path: Path to the profman binary.
    temp_dir: Directory to work in.

  Returns:
    The name of the binary profile, which will live in temp_dir.
  """
  binary_profile = os.path.join(
      temp_dir, 'binary_profile-for-' + os.path.basename(text_profile))
  open(binary_profile, 'w').close()  # Touch binary_profile.
  profman_cmd = [profman_path,
                 '--apk=' + input_dex,
                 '--dex-location=' + input_dex,
                 '--create-profile-from=' + text_profile,
                 '--reference-profile-file=' + binary_profile]
  build_utils.CheckOutput(
    profman_cmd,
    env=_EnvWithArtLibPath(profman_path),
    stderr_filter=lambda output:
        build_utils.FilterLines(output, '|'.join(
            [r'Could not find (method_id|proto_id|name):',
             r'Could not create type list'])))
  return binary_profile


def _LayoutDex(binary_profile, input_dex, dexlayout_path, temp_dir):
  """Layout a dexfile using a profile.

  Args:
    binary_profile: An ART binary profile, eg output from _CreateBinaryProfile.
    input_dex: The dex file used to create the binary profile.
    dexlayout_path: Path to the dexlayout binary.
    temp_dir: Directory to work in.

  Returns:
    List of output files produced by dexlayout. This will be one if the input
    was a single dexfile, or multiple files if the input was a multidex
    zip. These output files are located in temp_dir.
  """
  dexlayout_output_dir = os.path.join(temp_dir, 'dexlayout_output')
  os.mkdir(dexlayout_output_dir)
  dexlayout_cmd = [ dexlayout_path,
                    '-u',  # Update checksum
                    '-p', binary_profile,
                    '-w', dexlayout_output_dir,
                    input_dex ]
  build_utils.CheckOutput(
      dexlayout_cmd,
      env=_EnvWithArtLibPath(dexlayout_path),
      stderr_filter=lambda output:
          build_utils.FilterLines(output,
                                  r'Can.t mmap dex file.*please zipalign'))
  output_files = os.listdir(dexlayout_output_dir)
  if not output_files:
    raise Exception('dexlayout unexpectedly produced no output')
  return [os.path.join(dexlayout_output_dir, f) for f in output_files]


def _ZipMultidex(file_dir, dex_files):
  """Zip dex files into a multidex.

  Args:
    file_dir: The directory into which to write the output.
    dex_files: The dexfiles forming the multizip. Their names must end with
      classes.dex, classes2.dex, ...

  Returns:
    The name of the multidex file, which will live in file_dir.
  """
  ordered_files = []  # List of (archive name, file name)
  for f in dex_files:
    if f.endswith('classes.dex.zip'):
      ordered_files.append(('classes.dex', f))
      break
  if not ordered_files:
    raise Exception('Could not find classes.dex multidex file in %s',
                    dex_files)
  for dex_idx in xrange(2, len(dex_files) + 1):
    archive_name = 'classes%d.dex' % dex_idx
    for f in dex_files:
      if f.endswith(archive_name):
        ordered_files.append((archive_name, f))
        break
    else:
      raise Exception('Could not find classes%d.dex multidex file in %s',
                      dex_files)
  if len(set(f[1] for f in ordered_files)) != len(ordered_files):
    raise Exception('Unexpected clashing filenames for multidex in %s',
                    dex_files)

  zip_name = os.path.join(file_dir, 'multidex_classes.zip')
  build_utils.DoZip(((archive_name, os.path.join(file_dir, file_name))
                     for archive_name, file_name in ordered_files),
                    zip_name)
  return zip_name


def _ZipSingleDex(dex_file, zip_name):
  """Zip up a single dex file.

  Args:
    dex_file: A dexfile whose name is ignored.
    zip_name: The output file in which to write the zip.
  """
  build_utils.DoZip([('classes.dex', dex_file)], zip_name)


def main(args):
  options, paths = _ParseArgs(args)
  if ((options.proguard_enabled == 'true'
          and options.configuration_name == 'Release')
      or (options.debug_build_proguard_enabled == 'true'
          and options.configuration_name == 'Debug')):
    paths = [options.proguard_enabled_input_path]

  if options.inputs:
    paths += options.inputs

  if options.excluded_paths:
    # Excluded paths are relative to the output directory.
    exclude_paths = options.excluded_paths
    paths = [p for p in paths if not
             os.path.relpath(p, options.output_directory) in exclude_paths]

  input_paths = list(paths)
  if options.multi_dex and options.main_dex_list_path:
    input_paths.append(options.main_dex_list_path)

  dex_cmd = ['java', '-jar', options.d8_jar_path, '--no-desugaring']
  if options.multi_dex and options.main_dex_list_path:
    dex_cmd += ['--main-dex-list', options.main_dex_list_path]
  if options.release:
    dex_cmd += ['--release']
  if options.min_api:
    dex_cmd += ['--min-api', options.min_api]

  is_dex = options.dex_path.endswith('.dex')
  is_jar = options.dex_path.endswith('.jar')

  with build_utils.TempDir() as tmp_dir:
    tmp_dex_dir = os.path.join(tmp_dir, 'tmp_dex_dir')
    os.mkdir(tmp_dex_dir)
    if is_jar and _NoClassFiles(paths):
      # Handle case where no classfiles are specified in inputs
      # by creating an empty JAR
      with zipfile.ZipFile(options.dex_path, 'w') as outfile:
        outfile.comment = 'empty'
    else:
      # .dex files can't specify a name for D8. Instead, we output them to a
      # temp directory then move them after the command has finished running
      # (see _MoveTempDexFile). For other files, tmp_dex_dir is None.
      _RunD8(dex_cmd, paths, tmp_dex_dir)

    tmp_dex_output = os.path.join(tmp_dir, 'tmp_dex_output')
    if is_dex:
      _MoveTempDexFile(tmp_dex_dir, tmp_dex_output)
    else:
      # d8 supports outputting to a .zip, but does not have deterministic file
      # ordering: https://issuetracker.google.com/issues/119945929
      build_utils.ZipDir(tmp_dex_output, tmp_dex_dir)

    if options.dexlayout_profile:
      if options.proguard_mapping_path is not None:
        matching_profile = os.path.join(tmp_dir, 'obfuscated_profile')
        convert_dex_profile.ObfuscateProfile(
            options.dexlayout_profile, tmp_dex_output,
            options.proguard_mapping_path, options.dexdump_path,
            matching_profile)
      else:
        logging.warning('No obfuscation for %s', options.dexlayout_profile)
        matching_profile = options.dexlayout_profile
      binary_profile = _CreateBinaryProfile(matching_profile, tmp_dex_output,
                                            options.profman_path, tmp_dir)
      output_files = _LayoutDex(binary_profile, tmp_dex_output,
                                options.dexlayout_path, tmp_dir)
      target = None
      if len(output_files) > 1:
        target = _ZipMultidex(tmp_dir, output_files)
      else:
        output = output_files[0]
        if not zipfile.is_zipfile(output):
          target = os.path.join(tmp_dir, 'dex_classes.zip')
          _ZipSingleDex(output, target)
        else:
          target = output
      shutil.move(os.path.join(tmp_dir, target), tmp_dex_output)

    # The dex file is complete and can be moved out of tmp_dir.
    shutil.move(tmp_dex_output, options.dex_path)

  build_utils.WriteDepfile(
      options.depfile, options.dex_path, input_paths, add_pydeps=False)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
