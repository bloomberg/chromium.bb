#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
}

ARCH_LOCATION = {
    'x86-32': 'lib32',
    'x86-64': 'lib64',
}

NAME_ARCH_MAP = {
    '32.nexe': 'x86-32',
    '64.nexe': 'x86-64',
    'arm.nexe': 'arm'
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


def DebugPrint(message):
  if DebugPrint.debug_mode:
    sys.stderr.write('%s\n' % message)


DebugPrint.debug_mode = False  # Set to True to enable extra debug prints


def MakeDir(dirname):
  """Just like os.makedirs but doesn't generate errors when dirname
  already exists.
  """
  if os.path.isdir(dirname):
    return

  Trace("mkdir: %s" % dirname)
  try:
    os.makedirs(dirname)
  except OSError as exception_info:
    if exception_info.errno != errno.EEXIST:
      raise


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

  def __repr__(self):
    return "<ArchFile %s>" % self.path

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
               lib_path=None, extra_files=None, lib_prefix=None,
               toolchain=None, remap=None):
    '''Constructor

    Args:
      main_files: List of main entry program files.  These will be named
          files->main.nexe for dynamic nexes, and program for static nexes
      objdump: path to x86_64-nacl-objdump tool (or Linux equivalent)
      lib_path: List of paths to library directories
      extra_files: List of extra files to include in the nmf
      lib_prefix: A list of path components to prepend to the library paths,
          both for staging the libraries and for inclusion into the nmf file.
          Examples:  ['..'], ['lib_dir']
      toolchain: Specify which toolchain newlib|glibc|pnacl which can require
          different forms of the NMF.
      remap: Remaps the library name in the manifest.
      '''
    self.objdump = objdump
    self.main_files = main_files or []
    self.extra_files = extra_files or []
    self.lib_path = lib_path or []
    self.manifest = None
    self.needed = None
    self.lib_prefix = lib_prefix or []
    self.toolchain = toolchain
    self.remap = remap or {}


  def GleanFromObjdump(self, files):
    '''Get architecture and dependency information for given files

    Args:
      files: A dict with key=filename and value=list or set of archs.  E.g.:
          { '/path/to/my.nexe': ['x86-32']
            '/path/to/lib64/libmy.so': ['x86-64'],
            '/path/to/mydata.so': ['x86-32', 'x86-64'],
            '/path/to/my.data': None }  # Indicates all architectures

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
              url='/'.join(self.lib_prefix + [ARCH_LOCATION[arch], name]))
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
    for dirname in self.lib_path:
      filename = os.path.join(dirname, name)
      if os.path.exists(filename):
        files.append(filename)
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
    if self.needed:
      return self.needed

    runnable = (self.toolchain != 'newlib' and self.toolchain != 'pnacl')
    DebugPrint('GetNeeded(%s)' % self.main_files)
    if runnable:
      examined = set()
      all_files, unexamined = self.GleanFromObjdump(
          dict([(f, None) for f in self.main_files]))
      for name, arch_file in all_files.items():
        arch_file.url = name
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
    else:
      need = {}
      for filename in self.main_files:
        arch = filename.split('_')[-1]
        arch = NAME_ARCH_MAP[arch]
        url = os.path.split(filename)[1]
        need[filename] = ArchFile(arch=arch, name=os.path.basename(filename),
                                  path=filename, url=url)
      self.needed = need

    return self.needed

  def StageDependencies(self, destination_dir):
    '''Copies over the dependencies into a given destination directory

    Each library will be put into a subdirectory that corresponds to the arch.

    Args:
      destination_dir: The destination directory for staging the dependencies
    '''
    needed = self.GetNeeded()
    for source, arch_file in needed.items():
      urldest = urllib.url2pathname(arch_file.url)
      if source.endswith('.nexe') and source in self.main_files:
        urldest = os.path.basename(urldest)

      destination = os.path.join(destination_dir, urldest)

      if (os.path.normcase(os.path.abspath(source)) !=
          os.path.normcase(os.path.abspath(destination))):
        # make sure target dir exists
        MakeDir(os.path.dirname(destination))

        Trace("copy: %s -> %s" % (source, destination))
        shutil.copy2(source, destination)

  def _GenerateManifest(self):
    '''Create a JSON formatted dict containing the files

    NaCl will map url requests based on architecture.  The startup NEXE
    can always be found under the top key PROGRAM.  Additional files are under
    the FILES key further mapped by file name.  In the case of 'runnable' the
    PROGRAM key is populated with urls pointing the runnable-ld.so which acts
    as the startup nexe.  The application itself, is then placed under the
    FILES key mapped as 'main.exe' instead of it's original name so that the
    loader can find it.'''
    manifest = { FILES_KEY: {}, PROGRAM_KEY: {} }
    runnable = (self.toolchain != 'newlib' and self.toolchain != 'pnacl')

    needed = self.GetNeeded()
    for need, archinfo in needed.items():
      urlinfo = { URL_KEY: archinfo.url }
      name = archinfo.name

      # If starting with runnable-ld.so, make that the main executable.
      if runnable:
        if need.endswith(RUNNABLE_LD):
          manifest[PROGRAM_KEY][archinfo.arch] = urlinfo
          continue

      # For the main nexes:
      if need.endswith('.nexe') and need in self.main_files:
        # Ensure that the nexe name is relative, not absolute.
        # We assume that the nexe and the corresponding nmf file are
        # installed in the same directory.
        urlinfo[URL_KEY] = os.path.basename(urlinfo[URL_KEY])
        # Place it under program if we aren't using the runnable-ld.so.
        if not runnable:
          manifest[PROGRAM_KEY][archinfo.arch] = urlinfo
          continue
        # Otherwise, treat it like another another file named main.nexe.
        name = MAIN_NEXE

      name = self.remap.get(name, name)
      fileinfo = manifest[FILES_KEY].get(name, {})
      fileinfo[archinfo.arch] = urlinfo
      manifest[FILES_KEY][name] = fileinfo
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


def DetermineToolchain(objdump):
  objdump = objdump.replace('\\', '/')
  paths = objdump.split('/')
  count = len(paths)
  for index in range(count - 2, 0, -1):
    if paths[index] == 'toolchain':
      if paths[index + 1].endswith('newlib'):
        return 'newlib'
      if paths[index + 1].endswith('glibc'):
        return 'glibc'
  raise Error('Could not deternime which toolchain to use.')


def Trace(msg):
  if Trace.verbose:
    sys.stderr.write(str(msg) + '\n')

Trace.verbose = False


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
  parser.add_option('-P', '--path-prefix', dest='path_prefix', default='',
                    help='A path to prepend to shared libraries in the .nmf',
                    metavar='DIRECTORY')
  parser.add_option('-s', '--stage-dependencies', dest='stage_dependencies',
                    help='Destination directory for staging libraries',
                    metavar='DIRECTORY')
  parser.add_option('-r', '--remove', dest='remove',
                    help='Remove the prefix from the files.',
                    metavar='PATH')
  parser.add_option('-t', '--toolchain', dest='toolchain',
                    help='Add DIRECTORY to library search path',
                    default=None, metavar='TOOLCHAIN')
  parser.add_option('-n', '--name', dest='name',
                    help='Rename FOO as BAR',
                    action='append', default=[], metavar='FOO,BAR')
  parser.add_option('-v', '--verbose',
                    help='Verbose output', action='store_true')
  parser.add_option('-d', '--debug-mode',
                    help='Debug mode', action='store_true')
  (options, args) = parser.parse_args(argv)
  if options.verbose:
    Trace.verbose = True
  if options.debug_mode:
    DebugPrint.debug_mode = True

  if len(args) < 1:
    raise Error("No nexe files specified.  See --help for more info")

  if not options.toolchain:
    options.toolchain = DetermineToolchain(os.path.abspath(options.objdump))

  if options.toolchain not in ['newlib', 'glibc', 'pnacl']:
    raise Error('Unknown toolchain: ' + str(options.toolchain))

  remap = {}
  for ren in options.name:
    parts = ren.split(',')
    if len(parts) != 2:
      raise Error('Expecting --name=<orig_arch.so>,<new_name.so>')
    remap[parts[0]] = parts[1]

  if options.path_prefix:
    path_prefix = options.path_prefix.split('/')
  else:
    path_prefix = []

  nmf = NmfUtils(objdump=options.objdump,
                 main_files=args,
                 lib_path=options.lib_path,
                 lib_prefix=path_prefix,
                 toolchain=options.toolchain,
                 remap=remap)

  nmf.GetManifest()
  if options.output is None:
    sys.stdout.write(nmf.GetJson())
  else:
    with open(options.output, 'w') as output:
      output.write(nmf.GetJson())

  if options.stage_dependencies:
    Trace("Staging dependencies...")
    nmf.StageDependencies(options.stage_dependencies)

  return 0


# Invoke this file directly for simple testing.
if __name__ == '__main__':
  try:
    rtn = Main(sys.argv[1:])
  except Error, e:
    print "%s: %s" % (os.path.basename(__file__), e)
    rtn = 1
  sys.exit(rtn)
