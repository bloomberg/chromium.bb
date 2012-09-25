#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from optparse import OptionParser
import os
import subprocess
import sys


"""NEXE building script

This module will take a set of source files, include paths, library paths, and
additional arguments, and use them to build.
"""


def ErrOut(text):
  """ErrOut prints an error message and the command-line that caused it.

  Prints to standard err, both the command-line normally, and separated by
  >>...<< to make it easier to copy and paste the command, or to
  find command formating issues.
  """
  sys.stderr.write('\n\n')
  sys.stderr.write( '>>>' + '>> <<'.join(sys.argv) + '<<\n\n')
  sys.stderr.write(' '.join(sys.argv) + '<<\n\n')
  sys.stderr.write(text + '\n')
  sys.exit(1)


def MakeDir(outdir):
  if outdir and not os.path.exists(outdir):
    # There may be a race creating this directory, so ignore failure
    try:
      os.makedirs(outdir)
    except OSError:
      pass


def RemoveQuotes(opt):
  if opt and opt[0] == '"':
    return opt[1:-1]
  return opt


def ArgToList(opt):
  outlist = []
  optlist = RemoveQuotes(opt).split(' ')
  for optitem in optlist:
    optitem = RemoveQuotes(optitem).replace('\\"', '"')
    if optitem:
      outlist.append(optitem)
  return outlist


class Builder(object):
  """Builder object maintains options and generates build command-lines.

  The Builder object takes a set of script command-line options, and generates
  a set of paths, and command-line options for the NaCl toolchain.
  """
  def __init__(self, options):
    arch = options.arch
    self.arch = arch
    build_type = options.build.split('_')
    toolname = build_type[0]
    self.outtype = build_type[1]

    if sys.platform.startswith('linux'):
      self.osname = 'linux'
    elif sys.platform.startswith('win'):
      self.osname = 'win'
    elif sys.platform.startswith('darwin'):
      self.osname = 'mac'
    else:
      ErrOut('Toolchain OS %s not supported.' % sys.platform)

    if arch in ['x86-32', 'x86-64']:
      self.mainarch = 'x86'
      self.subarch = arch.split('-')[1]
      tool_subdir = 'x86_64-nacl'
      self.is_pnacl_toolchain = False
    elif arch == 'arm':
      # For now assume that arm implies pnacl toolchain. But don't assume that
      # elsewhere in this script.
      self.mainarch = 'arm'
      self.subarch = ''
      tool_subdir = toolname
      self.is_pnacl_toolchain = True
    elif arch == 'pnacl':
      self.mainarch = arch
      self.subarch = ''
      tool_subdir = toolname
      self.is_pnacl_toolchain = True
    else:
      ErrOut('Toolchain architecture %s not supported.' % arch)

    if arch == 'arm' and toolname == 'glibc':
      ErrOut('arm/glibc not yet supported.')

    if arch == 'pnacl' and toolname == 'glibc':
      ErrOut('pnacl glibc not yet supported.')

    if self.is_pnacl_toolchain:
      self.toolname = '%s_x86_pnacl' % (self.osname)
    else:
      self.toolname = '%s_x86_%s' % (self.osname, toolname)

    if toolname not in ['newlib', 'glibc']:
      ErrOut('Toolchain of type %s not supported.' % toolname)

    self.root_path = options.root
    self.nacl_path = os.path.join(self.root_path, 'native_client')

    project_path, project_name = os.path.split(options.name)
    self.outdir = options.objdir

    # Set the toolchain directories
    self.toolchain = os.path.join(options.toolpath, self.toolname)
    self.toolbin = os.path.join(self.toolchain, tool_subdir, 'bin')

    if self.is_pnacl_toolchain:
      self.toollib = os.path.join(self.toolchain, tool_subdir,'lib')
      self.toolinc = os.path.join(self.toolchain, tool_subdir, 'sysroot',
                                  'include')
    else:
      self.toollib = os.path.join(self.toolchain,
                                  tool_subdir,
                                  'lib' + self.subarch)
      self.toolinc = os.path.join(self.toolchain, tool_subdir, 'include')

    self.inc_paths = ArgToList(options.incdirs)
    self.lib_paths = ArgToList(options.libdirs)
    self.define_list = ArgToList(options.defines)

    self.name = options.name
    self.BuildCompileOptions(options.compile_flags, self.define_list)
    self.BuildLinkOptions(options.link_flags)
    self.BuildArchiveOptions()
    self.verbose = options.verbose
    self.suffix = options.suffix
    self.strip = options.strip
    self.empty = options.empty
    self.strip_debug = options.strip_debug

    if self.verbose:
      print 'Compile options: %s' % self.compile_options
      print 'Linker options: %s' % self.link_options

  def GenNaClPath(self, path):
    """Helper which prepends path with the native client source directory."""
    return os.path.join(self.root_path, 'native_client', path)

  def GetBinName(self, name):
    """Helper which prepends executable with the toolchain bin directory."""
    return os.path.join(self.toolbin, name)

  def GetCCompiler(self):
    """Helper which returns C compiler path."""
    if self.is_pnacl_toolchain:
      return self.GetBinName('pnacl-clang')
    else:
      return self.GetBinName('gcc')

  def GetCXXCompiler(self):
    """Helper which returns C++ compiler path."""
    if self.is_pnacl_toolchain:
      return self.GetBinName('pnacl-clang++')
    else:
      return self.GetBinName('g++')

  def GetAr(self):
    """Helper which returns ar path."""
    if self.is_pnacl_toolchain:
      return self.GetBinName('pnacl-ar')
    else:
      return self.GetBinName('ar')

  def GetStrip(self):
    """Helper which returns strip path."""
    if self.is_pnacl_toolchain:
      return self.GetBinName('pnacl-strip')
    else:
      return self.GetBinName('strip')

  def BuildAssembleOptions(self, options):
    options = ArgToList(options)
    self.assemble_options = options + ['-I' + name for name in self.inc_paths]

  def BuildCompileOptions(self, options, define_list):
    """Generates compile options, called once by __init__."""
    options = ArgToList(options)
    # We want to shared gyp 'defines' with other targets, but not
    # ones that are host system dependent. Filtering them out.
    # This really should be better.
    # See: http://code.google.com/p/nativeclient/issues/detail?id=2936
    define_list = [define for define in define_list
                   if not (define.startswith('NACL_TARGET_ARCH=') or
                           define.startswith('NACL_TARGET_SUBARCH=') or
                           define.startswith('NACL_WINDOWS=') or
                           define.startswith('NACL_OSX=') or
                           define.startswith('NACL_LINUX=') or
                           define == 'COMPONENT_BUILD' or
                           'WIN32' in define or
                           'WINDOWS' in define or
                           'WINVER' in define)]
    options += ['-D' + define for define in define_list]
    # TODO(mseaborn): Enable -Werror for ARM/PNaCl when warnings in
    # Chromium's PPAPI proxy have been fixed.
    if self.mainarch == 'x86':
      options.append('-Werror')
    self.compile_options = options + ['-I' + name for name in self.inc_paths]

  def BuildLinkOptions(self, options):
    """Generates link options, called once by __init__."""
    options = ArgToList(options)
    if self.toolname in ['glibc', 'newlib'] and self.mainarch == 'x86':
      options += ['-B' + self.toollib]
    if self.outtype == 'nso':
      options += ['-Wl,-rpath-link,' + name for name in self.lib_paths]
      options += ['-shared']
      options += ['-Wl,-soname,' + os.path.basename(self.name)]
    self.link_options = options + ['-L' + name for name in self.lib_paths]

  def BuildArchiveOptions(self):
    """Generates link options, called once by __init__."""
    self.archive_options = []

  def Run(self, cmd_line, out):
    """Helper which runs a command line."""

    # For POSIX style path on windows for POSIX based toolchain
    # (just for arguments, not for the path to the command itself)
    cmd_line = [cmd_line[0]] + [cmd.replace('\\', '/') for cmd in cmd_line[1:]]

    if self.verbose:
      print ' '.join(cmd_line)
    try:
      if self.is_pnacl_toolchain:
        # PNaCl toolchain executable is a script, not a binary, so it doesn't
        # want to run on Windows without a shell
        ecode = subprocess.call(' '.join(cmd_line), shell=True)
      else:
        ecode = subprocess.call(cmd_line)

    except Exception, err:
      ErrOut('\n%s\nFAILED: %s\n\n' % (' '.join(cmd_line), str(err)))
    if ecode != 0:
      print 'Err %d: nacl-%s %s' % (ecode, os.path.basename(cmd_line[0]), out)
      print '>>%s<<' % '<< >>'.join(cmd_line)
    return ecode

  def GetObjectName(self, src):
    if self.strip:
      src = src.replace(self.strip,'')
    filepath, filename = os.path.split(src)
    filename, ext = os.path.splitext(filename)
    if self.suffix:
      return os.path.join(self.outdir, filename + '.o')
    else:
      filename = os.path.split(src)[1]
      return os.path.join(self.outdir, os.path.splitext(filename)[0] + '.o')

  def CleanOutput(self, out):
    if os.path.isfile(out):
      os.remove(out)

  def FixWindowsPath(self, path):
    # The windows version of the nacl toolchain returns badly
    # formed system header paths. As we do want changes in the
    # toolchain to trigger rebuilds, compensate by detecting
    # malformed paths (starting with /libexec/) and assume these
    # are actually toolchain relative.
    #
    # Additionally, in some cases the toolchains emit cygwin paths
    # which don't work in a win32 python.
    # Assume they are all /cygdrive/ relative and convert to a
    # drive letter.
    cygdrive = '/cygdrive/'
    if path.startswith('/cygdrive/'):
      path = os.path.normpath(
          path[len(cygdrive)] + ':' + path[len(cygdrive)+1:])
    elif path.startswith('/libexec/'):
      path = os.path.normpath(os.path.join(self.toolchain, path[1:]))
    return path

  def NeedsRebuild(self, outd, out, src):
    if not os.path.isfile(outd):
      return True
    if not os.path.isfile(out):
      return True
    out_tm = os.path.getmtime(out)
    outd_tm = os.path.getmtime(outd)
    src_tm = os.path.getmtime(src)
    if out_tm <= src_tm or outd_tm <= src_tm:
      return True
    # Decode emitted makefile.
    fh = open(outd, 'r')
    deps = fh.read()
    fh.close()
    deps = deps.replace('\\\n', ' ')
    deps = deps.replace('\n', '')
    deps = deps.split(':')[1]
    deps = deps.split()
    if sys.platform in ['win32', 'cygwin']:
      deps = [self.FixWindowsPath(d) for d in deps]
    # Check if any input has changed.
    for filename in deps:
      file_tm = os.path.getmtime(filename)
      if out_tm <= file_tm or outd_tm <= file_tm:
        return True
    return False

  def Compile(self, src):
    """Compile the source with pre-determined options."""

    filename, ext = os.path.splitext(src)
    if ext in ['.c', '.S']:
      bin_name = self.GetCCompiler()
      extra = ['-std=gnu99']
      if self.is_pnacl_toolchain and ext == '.S':
        extra.append('-arch')
        extra.append(self.arch)
    elif ext in ['.cc', '.cpp']:
      bin_name = self.GetCXXCompiler()
      extra = []
    else:
      if self.verbose and ext != '.h':
        print 'Skipping unknown type %s for %s.' % (ext, src)
      return None

    if self.verbose:
      print '\nCompile %s' % src

    out = self.GetObjectName(src)
    outd = out + '.d'

    # Don't rebuild unneeded.
    if not self.NeedsRebuild(outd, out, src):
      return out

    MakeDir(os.path.dirname(out))
    self.CleanOutput(out)
    self.CleanOutput(outd)
    cmd_line = [bin_name, '-c', src, '-o', out,
                '-MD', '-MF', outd] + extra + self.compile_options
    err = self.Run(cmd_line, out)
    if sys.platform.startswith('win') and err == 5:
      # Try again on mystery windows failure.
      err = self.Run(cmd_line, out)
    if err:
      self.CleanOutput(outd)
      ErrOut('\nFAILED with %d: %s\n\n' % (err, ' '.join(cmd_line)))
    else:
      assert not self.NeedsRebuild(outd, out, src)
    return out

  def Link(self, srcs):
    """Link these objects with predetermined options and output name."""
    out = self.name
    if self.verbose:
      print '\nLink %s' % out
    bin_name = self.GetCXXCompiler()
    MakeDir(os.path.dirname(out))
    self.CleanOutput(out)

    cmd_line = [bin_name, '-o', out, '-Wl,--as-needed']
    if not self.empty:
      cmd_line += srcs
    cmd_line += self.link_options

    err = self.Run(cmd_line, out)
    # TODO( Retry on windows
    if sys.platform.startswith('win') and err == 5:
      # Try again on mystery windows failure.
      err = self.Run(cmd_line, out)
    if err:
      ErrOut('\nFAILED with %d: %s\n\n' % (err, ' '.join(cmd_line)))
    return out

  def Archive(self, srcs):
    """Archive these objects with predetermined options and output name."""
    out = self.name
    if self.verbose:
      print '\nArchive %s' % out


    if '-r' in self.link_options:
      bin_name = self.GetCXXCompiler()
      cmd_line = [bin_name, '-o', out, '-Wl,--as-needed']
      if not self.empty:
        cmd_line += srcs
      cmd_line += self.link_options
    else:
      bin_name = self.GetAr()
      cmd_line = [bin_name, '-rc', out]
      if not self.empty:
        cmd_line += srcs

    MakeDir(os.path.dirname(out))
    self.CleanOutput(out)
    err = self.Run(cmd_line, out)
    if sys.platform.startswith('win') and err == 5:
      # Try again on mystery windows failure.
      err = self.Run(cmd_line, out)
    if err:
      ErrOut('\nFAILED with %d: %s\n\n' % (err, ' '.join(cmd_line)))
    return out

  def Strip(self, out):
    """Strip the NEXE"""
    if self.verbose:
      print '\nStrip %s' % out

    tmp = out + '.tmp'
    self.CleanOutput(tmp)
    os.rename(out, tmp)
    bin_name = self.GetStrip()
    cmd_line = [bin_name, '--strip-debug', tmp, '-o', out]
    err = self.Run(cmd_line, out)
    if sys.platform.startswith('win') and err == 5:
      # Try again on mystery windows failure.
      err = self.Run(cmd_line, out)
    if err:
      ErrOut('\nFAILED with %d: %s\n\n' % (err, ' '.join(cmd_line)))
    return out

  def Generate(self, srcs):
    """Generate final output file.

    Link or Archive the final output file, from the compiled sources.
    """
    if self.outtype in ['nexe', 'pexe', 'nso']:
      out = self.Link(srcs)
      if self.strip_debug:
        self.Strip(out)
    elif self.outtype in ['nlib', 'plib']:
      self.Archive(srcs)
    else:
      ErrOut('FAILED: Unknown outtype %s:\n' % (self.outtype))


def Main(argv):
  parser = OptionParser()
  parser.add_option('--empty', dest='empty', default=False,
                    help='Do not pass sources to library.', action='store_true')
  parser.add_option('--no-suffix', dest='suffix', default=True,
                    help='Do not append arch suffix.', action='store_false')
  parser.add_option('--sufix', dest='suffix',
                    help='Do append arch suffix.', action='store_true')
  parser.add_option('--strip-debug', dest='strip_debug', default=False,
                    help='Strip the NEXE', action='store_true')
  parser.add_option('--strip', dest='strip', default='',
                    help='Strip the filename')
  parser.add_option('--source-list', dest='source_list',
                    help='Filename to load a source list from')
  parser.add_option('-a', '--arch', dest='arch',
                    help='Set target architecture')
  parser.add_option('-c', '--compile', dest='compile_only', default=False,
                    help='Compile only.', action='store_true')
  parser.add_option('-i', '--include-dirs', dest='incdirs',
                    help='Set include directories.')
  parser.add_option('-l', '--lib-dirs', dest='libdirs',
                    help='Set library directories.')
  parser.add_option('-n', '--name', dest='name',
                    help='Base path and name of the nexe.')
  parser.add_option('-o', '--objdir', dest='objdir',
                    help='Base path of the object output dir.')
  parser.add_option('-r', '--root', dest='root',
                    help='Set the root directory of the sources')
  parser.add_option('-b', '--build', dest='build',
                    help='Set build type (newlib, glibc).')
  parser.add_option('--compile_flags', dest='compile_flags',
                    help='Set compile flags.')
  parser.add_option('--defines', dest='defines',
                    help='Set defines')
  parser.add_option('--link_flags', dest='link_flags',
                    help='Set link flags.')
  parser.add_option('-v', '--verbose', dest='verbose', default=False,
                    help='Enable verbosity', action='store_true')
  parser.add_option('-t', '--toolpath', dest='toolpath',
                    help='Set the path for of the toolchains.')
  (options, files) = parser.parse_args(argv[1:])

  if not argv:
    parser.print_help()
    return 1

  if options.source_list:
    source_list_handle = open(options.source_list, 'r')
    source_list = source_list_handle.read().splitlines()
    source_list_handle.close()
    files = files + source_list

  # Fix slash style to insulate invoked toolchains.
  options.toolpath = os.path.normpath(options.toolpath)

  build = Builder(options)
  objs = []
  for filename in files:
    out = build.Compile(filename)
    if out:
      objs.append(out)
  # Do not link if building an object
  if not options.compile_only:
    build.Generate(objs)
  return 0

if __name__ == '__main__':
  sys.exit(Main(sys.argv))
