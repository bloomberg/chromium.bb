#!/usr/bin/env python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import with_statement

import errno
import optparse
import os
import re
import shutil
import subprocess
import sys
import urllib

try:
  import json
except ImportError:
  import simplejson as json

NeededMatcher = re.compile('^ *NEEDED *([^ ]+)\n$')
FormatMatcher = re.compile('^(.+):\\s*file format (.+)\n$')

FORMAT_ARCH_MAP = {
    # Names returned by Linux's objdump:
    'elf64-x86-64': 'x86-64',
    'elf32-i386': 'x86-32',
    # Names returned by x86_64-nacl-objdump:
    'elf64-nacl': 'x86-64',
    'elf32-nacl': 'x86-32',
    # TODO(mball): Add support for 'arm-32' and 'portable' architectures
    # 'elf32-little': 'arm-32',
    }

# These constants are used within nmf files.
RUNNABLE_LD = 'runnable-ld.so'  # Name of the dynamic loader
MAIN_NEXE = 'main.nexe'  # Name of entry point for execution
PROGRAM_KEY = 'program'  # Key of the program section in an nmf file
URL_KEY = 'url'  # Key of the url field for a particular file in an nmf file
FILES_KEY = 'files'  # Key of the files section in an nmf file

# The proper name of the dynamic linker, as kept in the IRT.  This is
# excluded from the nmf file by convention.
LD_NACL_MAP = {
    'x86-32': 'ld-nacl-x86-32.so.1',
    'x86-64': 'ld-nacl-x86-64.so.1',
}

_debug_mode = False  # Set to True to enable extra debug prints


def DebugPrint(message):
  if _debug_mode:
    sys.stderr.write('%s\n' % message)
    sys.stderr.flush()


class Error(Exception):
  '''Local Error class for this file.'''
  pass


class ArchFile(object):
  '''Simple structure containing information about

  Attributes:
    arch: Architecture of this file (e.g., x86-32)
    filename: name of this file
    path: Full path to this file on the build system
    url: Relative path to file in the staged web directory.
        Used for specifying the "url" attribute in the nmf file.'''
  def __init__(self, arch, name, path='', url=None):
    self.arch = arch
    self.name = name
    self.path = path
    self.url = url or '/'.join([arch, name])

  def __str__(self):
    '''Return the file path when invoked with the str() function'''
    return self.path


class NmfUtils(object):
  '''Helper class for creating and managing nmf files

  Attributes:
    manifest: A JSON-structured dict containing the nmf structure
    needed: A dict with key=filename and value=ArchFile (see GetNeeded)
  '''

  def __init__(self, main_files=None, objdump='x86_64-nacl-objdump',
               lib_path=None, extra_files=None, lib_prefix=None):
    ''' Constructor

    Args:
      main_files: List of main entry program files.  These will be named
          files->main.nexe for dynamic nexes, and program for static nexes
      objdump: path to x86_64-nacl-objdump tool (or Linux equivalent)
      lib_path: List of paths to library directories
      extra_files: List of extra files to include in the nmf
      lib_prefix: A list of path components to prepend to the library paths,
          both for staging the libraries and for inclusion into the nmf file.
          Examples:  ['..'], ['lib_dir'] '''
    self.objdump = objdump
    self.main_files = main_files or []
    self.extra_files = extra_files or []
    self.lib_path = lib_path or []
    self.manifest = None
    self.needed = None
    self.lib_prefix = lib_prefix or []

  def GleanFromObjdump(self, files):
    '''Get architecture and dependency information for given files

    Args:
      files: A dict with key=filename and value=list or set of archs.  E.g.:
          { '/path/to/my.nexe': ['x86-32', 'x86-64'],
            '/path/to/libmy.so': ['x86-32'],
            '/path/to/my2.nexe': None }  # Indicates all architectures

    Returns: A tuple with the following members:
      input_info: A dict with key=filename and value=ArchFile of input files.
          Includes the input files as well, with arch filled in if absent.
          Example: { '/path/to/my.nexe': ArchFile(my.nexe),
                     '/path/to/libfoo.so': ArchFile(libfoo.so) }
      needed: A set of strings formatted as "arch/name".  Example:
          set(['x86-32/libc.so', 'x86-64/libgcc.so'])
    '''
    DebugPrint("GleanFromObjdump(%s)" % ([self.objdump, '-p'] + files.keys()))
    proc = subprocess.Popen([self.objdump, '-p'] + files.keys(),
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, bufsize=-1)
    input_info = {}
    needed = set()
    output, err_output = proc.communicate()
    for line in output.splitlines(True):
      # Objdump should display the architecture first and then the dependencies
      # second for each file in the list.
      matched = FormatMatcher.match(line)
      if matched is not None:
        filename = matched.group(1)
        arch = FORMAT_ARCH_MAP[matched.group(2)]
        if files[filename] is None or arch in files[filename]:
          name = os.path.basename(filename)
          input_info[filename] = ArchFile(
              arch=arch,
              name=name,
              path=filename,
              url='/'.join(self.lib_prefix + [arch, name]))
      matched = NeededMatcher.match(line)
      if matched is not None:
        if files[filename] is None or arch in files[filename]:
          needed.add('/'.join([arch, matched.group(1)]))
    status = proc.poll()
    if status != 0:
      raise Error('%s\nStdError=%s\nobjdump failed with error code: %d' %
                  (output, err_output, status))
    return input_info, needed

  def FindLibsInPath(self, name):
    '''Finds the set of libraries matching |name| within lib_path

    Args:
      name: name of library to find

    Returns:
      A list of system paths that match the given name within the lib_path'''
    files = []
    for dir in self.lib_path:
      file = os.path.join(dir, name)
      if os.path.exists(file):
        files.append(file)
    if not files:
      raise Error('cannot find library %s' % name)
    return files

  def GetNeeded(self):
    '''Collect the list of dependencies for the main_files

    Returns:
      A dict with key=filename and value=ArchFile of input files.
          Includes the input files as well, with arch filled in if absent.
          Example: { '/path/to/my.nexe': ArchFile(my.nexe),
                     '/path/to/libfoo.so': ArchFile(libfoo.so) }'''
    if not self.needed:
      DebugPrint('GetNeeded(%s)' % self.main_files)
      examined = set()
      all_files, unexamined = self.GleanFromObjdump(
          dict([(file, None) for file in self.main_files]))
      for name, arch_file in all_files.items():
        arch_file.url = os.path.basename(name)
        if unexamined:
          unexamined.add('/'.join([arch_file.arch, RUNNABLE_LD]))
      while unexamined:
        files_to_examine = {}
        for arch_name in unexamined:
          arch, name = arch_name.split('/')
          for path in self.FindLibsInPath(name):
            files_to_examine.setdefault(path, set()).add(arch)
        new_files, needed = self.GleanFromObjdump(files_to_examine)
        all_files.update(new_files)
        examined |= unexamined
        unexamined = needed - examined
      # With the runnable-ld.so scheme we have today, the proper name of
      # the dynamic linker should be excluded from the list of files.
      ldso = [LD_NACL_MAP[arch] for arch in set(FORMAT_ARCH_MAP.values())]
      for name, arch_map in all_files.items():
        if arch_map.name in ldso:
          del all_files[name]
      self.needed = all_files
    return self.needed

  def StageDependencies(self, destination_dir):
    '''Copies over the dependencies into a given destination directory

    Each library will be put into a subdirectory that corresponds to the arch.

    Args:
      destination_dir: The destination directory for staging the dependencies
    '''
    needed = self.GetNeeded()
    for source, arch_file in needed.items():
      destination = os.path.join(destination_dir,
                                 urllib.url2pathname(arch_file.url))
      try:
        os.makedirs(os.path.dirname(destination))
      except OSError as exception_info:
        if exception_info.errno != errno.EEXIST:
          raise
      if (os.path.normcase(os.path.abspath(source)) !=
          os.path.normcase(os.path.abspath(destination))):
        shutil.copy2(source, destination)

  def _GenerateManifest(self):
    programs = {}
    files = {}

    def add_files(needed):
      for filename, arch_file in needed.items():
        files.setdefault(arch_file.arch, set()).add(arch_file.name)

    needed = self.GetNeeded()
    add_files(needed)

    for filename in self.main_files:
      arch_file = needed[filename]
      programs[arch_file.arch] = arch_file.name

    filemap = {}
    for arch in files:
      for file in files[arch]:
        if file not in programs.values() and file != RUNNABLE_LD:
          filemap.setdefault(file, set()).add(arch)

    def arch_name(arch, file):
      # nmf files expect unix-style path separators
      return {URL_KEY: '/'.join(self.lib_prefix + [arch, file])}

    # TODO(mcgrathr): perhaps notice a program with no deps
    # (i.e. statically linked) and generate program=nexe instead?
    manifest = {PROGRAM_KEY: {}, FILES_KEY: {MAIN_NEXE: {}}}
    for arch in programs:
      manifest[PROGRAM_KEY][arch] = arch_name(arch, RUNNABLE_LD)
      manifest[FILES_KEY][MAIN_NEXE][arch] = {URL_KEY: programs[arch]}

    for file in filemap:
      manifest[FILES_KEY][file] = dict([(arch, arch_name(arch, file))
                                      for arch in filemap[file]])
    self.manifest = manifest

  def GetManifest(self):
    '''Returns a JSON-formatted dict containing the NaCl dependencies'''
    if not self.manifest:
      self._GenerateManifest()

    return self.manifest

  def GetJson(self):
    '''Returns the Manifest as a JSON-formatted string'''
    pretty_string = json.dumps(self.GetManifest(), indent=2)
    # json.dumps sometimes returns trailing whitespace and does not put
    # a newline at the end.  This code fixes these problems.
    pretty_lines = pretty_string.split('\n')
    return '\n'.join([line.rstrip() for line in pretty_lines]) + '\n'


def Main(argv):
  parser = optparse.OptionParser(
      usage='Usage: %prog [options] nexe [extra_libs...]')
  parser.add_option('-o', '--output', dest='output',
                    help='Write manifest file to FILE (default is stdout)',
                    metavar='FILE')
  parser.add_option('-D', '--objdump', dest='objdump', default='objdump',
                    help='Use TOOL as the "objdump" tool to run',
                    metavar='TOOL')
  parser.add_option('-L', '--library-path', dest='lib_path',
                    action='append', default=[],
                    help='Add DIRECTORY to library search path',
                    metavar='DIRECTORY')
  parser.add_option('-s', '--stage-dependencies', dest='stage_dependencies',
                    help='Destination directory for staging libraries',
                    metavar='DIRECTORY')
  (options, args) = parser.parse_args(argv)

  if len(args) < 1:
    parser.print_usage()
    sys.exit(1)

  nmf = NmfUtils(objdump=options.objdump,
                 main_files=args,
                 lib_path=options.lib_path)

  manifest = nmf.GetManifest()

  if options.output is None:
    sys.stdout.write(nmf.GetJson())
  else:
    with open(options.output, 'w') as output:
      output.write(nmf.GetJson())

  if options.stage_dependencies:
    nmf.StageDependencies(options.stage_dependencies)


# Invoke this file directly for simple testing.
if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
