#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""NEXE building script

This module will take a set of source files, include paths, library paths, and
additional arguments, and use them to build.
"""

from optparse import OptionParser
import os
import Queue
import shutil
import subprocess
import sys
import tempfile
import threading
import urllib2

class Error(Exception):
  pass


def FixPath(path):
  # On Windows, |path| can be a long relative path: ..\..\..\..\out\Foo\bar...
  # If the full path -- os.path.join(os.getcwd(), path) -- is longer than 255
  # characters, then any operations that open or check for existence will fail.
  # We can't use os.path.abspath here, because that calls into a Windows
  # function that still has the path length limit. Instead, we'll cheat and
  # normalize the path lexically.
  path = os.path.normpath(os.path.join(os.getcwd(), path))
  if sys.platform in ['win32', 'cygwin']:
    if len(path) > 255:
      raise Error('Path "%s" is too long (%d characters), and will fail.' % (
          path, len(path)))
  return path


def IsFile(path):
  return os.path.isfile(FixPath(path))


def MakeDir(outdir):
  outdir = FixPath(outdir)
  if outdir and not os.path.exists(outdir):
    # There may be a race creating this directory, so ignore failure.
    try:
      os.makedirs(outdir)
    except OSError:
      pass


def RemoveFile(path):
  os.remove(FixPath(path))


def OpenFile(path, mode='r'):
  return open(FixPath(path), mode)


def RemoveQuotes(opt):
  if opt and opt[0] == '"':
    return opt[1:-1]
  return opt


def ArgToList(opt):
  outlist = []
  if opt is None:
    return outlist
  optlist = RemoveQuotes(opt).split(' ')
  for optitem in optlist:
    optitem = RemoveQuotes(optitem).replace('\\"', '"')
    if optitem:
      outlist.append(optitem)
  return outlist


def GetMTime(filepath):
  """GetMTime returns the last modification time of the file or None."""
  try:
    return os.path.getmtime(FixPath(filepath))
  except OSError:
    return None


def IsStale(out_ts, src_ts, rebuilt=False):
  # If either source or output timestamp was not available, assume stale.
  if not out_ts or not src_ts:
    return True

  # If just rebuilt timestamps may be equal due to time granularity.
  if rebuilt:
    return out_ts < src_ts
  # If about to build, be conservative and rebuilt just in case.
  return out_ts <= src_ts


def GetGomaPath(osname, arch, toolname):
  """Returns full-path of gomacc if goma is available or None."""
  # Start goma support from os/arch/toolname that have been tested.
  # Set NO_NACL_GOMA=true to force to avoid using goma.
  if (osname != 'linux' or arch not in ['x86-32', 'x86-64']
      or toolname not in ['newlib', 'glibc']
      or os.environ.get('NO_NACL_GOMA', None)):
    return None

  try:
    gomacc_base = 'gomacc.exe' if os.name == 'nt' else 'gomacc'
    for directory in os.environ.get('PATH', '').split(os.path.pathsep):
      gomacc = os.path.join(directory, gomacc_base)
      if os.path.isfile(gomacc):
        port = subprocess.Popen(
            [gomacc, 'port'], stdout=subprocess.PIPE).communicate()[0].strip()
        if port:
          status = urllib2.urlopen(
              'http://127.0.0.1:%s/healthz' % port).read().strip()
          if status == 'ok':
            return gomacc
  except Exception:
    # Anyway, fallbacks to non-goma mode.
    pass
  return None


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
      raise Error('Toolchain OS %s not supported.' % sys.platform)

    # pnacl toolchain can be selected in three different ways
    # 1. by specifying --arch=pnacl directly to generate
    #    pexe targets.
    # 2. by specifying --build=newlib_translate to generated
    #    nexe via translation
    # 3. by specifying --build=newlib_nexe_pnacl use pnacl
    #    toolchain in arm-native mode (e.g. the arm IRT)
    self.is_pnacl_toolchain = False
    if self.outtype == 'translate':
      self.is_pnacl_toolchain = True

    if len(build_type) > 2 and build_type[2] == 'pnacl':
      self.is_pnacl_toolchain = True

    if arch in ['x86-32', 'x86-64']:
      mainarch = 'x86'
      self.subarch = arch.split('-')[1]
      self.tool_prefix = 'x86_64-nacl-'
    elif arch == 'arm':
      self.subarch = ''
      self.tool_prefix = 'arm-nacl-'
      mainarch = 'arm'
    elif arch == 'mips':
      self.is_pnacl_toolchain = True
    elif arch == 'pnacl':
      self.subarch = ''
      self.is_pnacl_toolchain = True
    else:
      raise Error('Toolchain architecture %s not supported.' % arch)

    if toolname not in ['newlib', 'glibc']:
      raise Error('Toolchain of type %s not supported.' % toolname)

    if arch == 'arm' and toolname == 'glibc':
      raise Error('arm glibc not yet supported.')

    if arch == 'mips' and toolname == 'glibc':
      raise Error('mips glibc not supported.')

    if arch == 'pnacl' and toolname == 'glibc':
      raise Error('pnacl glibc not yet supported.')

    if self.is_pnacl_toolchain:
      self.tool_prefix = 'pnacl-'
      tooldir = '%s_pnacl' % self.osname
    else:
      tooldir = '%s_%s_%s' % (self.osname, mainarch, toolname)

    self.root_path = options.root
    self.nacl_path = os.path.join(self.root_path, 'native_client')

    project_path, project_name = os.path.split(options.name)
    self.outdir = options.objdir

    # Set the toolchain directories
    self.toolchain = os.path.join(options.toolpath, tooldir)
    self.toolbin = os.path.join(self.toolchain, 'bin')
    self.toolstamp = os.path.join(self.toolchain, 'stamp.prep')
    if not IsFile(self.toolstamp):
      raise Error('Could not find toolchain prep stamp file: ' + self.toolstamp)

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
    self.strip_all = options.strip_all
    self.strip_debug = options.strip_debug
    self.finalize_pexe = options.finalize_pexe and arch == 'pnacl'
    self.gomacc = GetGomaPath(self.osname, arch, toolname)

    # Use unoptimized native objects for debug IRT builds for faster compiles.
    if (self.is_pnacl_toolchain
        and (self.outtype == 'nlib'
             or self.outtype == 'nexe')
        and self.arch != 'pnacl'):
      if (options.build_config is not None
          and options.build_config == 'Debug'):
        self.compile_options.extend(['--pnacl-allow-translate',
                                     '--pnacl-allow-native',
                                     '-arch', self.arch])
        # Also use fast translation because we are still translating libc/libc++
        self.link_options.append('-Wt,-O0')

    self.Log('Compile options: %s' % self.compile_options)
    self.Log('Linker options: %s' % self.link_options)

  def GenNaClPath(self, path):
    """Helper which prepends path with the native client source directory."""
    return os.path.join(self.root_path, 'native_client', path)

  def GetBinName(self, name):
    """Helper which prepends executable with the toolchain bin directory."""
    return os.path.join(self.toolbin, self.tool_prefix + name)

  def GetCCompiler(self):
    """Helper which returns C compiler path."""
    if self.is_pnacl_toolchain:
      return self.GetBinName('clang')
    else:
      return self.GetBinName('gcc')

  def GetCXXCompiler(self):
    """Helper which returns C++ compiler path."""
    if self.is_pnacl_toolchain:
      return self.GetBinName('clang++')
    else:
      return self.GetBinName('g++')

  def GetAr(self):
    """Helper which returns ar path."""
    return self.GetBinName('ar')

  def GetStrip(self):
    """Helper which returns strip path."""
    return self.GetBinName('strip')

  def GetObjCopy(self):
    """Helper which returns objcopy path."""
    return self.GetBinName('objcopy')

  def GetPnaclFinalize(self):
    """Helper which returns pnacl-finalize path."""
    assert self.is_pnacl_toolchain
    return self.GetBinName('finalize')

  def BuildAssembleOptions(self, options):
    options = ArgToList(options)
    self.assemble_options = options + ['-I' + name for name in self.inc_paths]

  def DebugName(self):
    return self.name + '.debug'

  def UntaggedName(self):
    return self.name + '.untagged'

  def LinkOutputName(self):
    if (self.is_pnacl_toolchain and self.finalize_pexe or
        self.strip_all or self.strip_debug):
      return self.DebugName()
    else:
      return self.name

  def ArchiveOutputName(self):
    if self.strip_debug:
      return self.DebugName()
    else:
      return self.name

  def StripOutputName(self):
    return self.name

  def TranslateOutputName(self):
    return self.name

  def Soname(self):
    return self.name

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
    define_list.extend(['NACL_WINDOWS=0',
                        'NACL_OSX=0',
                        'NACL_LINUX=0'])
    options += ['-D' + define for define in define_list]
    self.compile_options = options + ['-I' + name for name in self.inc_paths]

  def BuildLinkOptions(self, options):
    """Generates link options, called once by __init__."""
    options = ArgToList(options)
    if self.outtype == 'nso':
      options += ['-Wl,-rpath-link,' + name for name in self.lib_paths]
      options += ['-shared']
      options += ['-Wl,-soname,' + os.path.basename(self.Soname())]
    self.link_options = options + ['-L' + name for name in self.lib_paths]

  def BuildArchiveOptions(self):
    """Generates link options, called once by __init__."""
    self.archive_options = []

  def Log(self, msg):
    if self.verbose:
      sys.stderr.write(str(msg) + '\n')

  def Run(self, cmd_line, out):
    """Helper which runs a command line."""

    # For POSIX style path on windows for POSIX based toolchain
    # (just for arguments, not for the path to the command itself)
    cmd_line = [cmd_line[0]] + [cmd.replace('\\', '/') for cmd in cmd_line[1:]]
    # Windows has a command line length limitation of 8191 characters.
    temp_file = None
    if len(' '.join(cmd_line)) > 8000:
      with tempfile.NamedTemporaryFile(delete=False) as temp_file:
        temp_file.write(' '.join(cmd_line[1:]))
      cmd_line = [cmd_line[0], '@' + temp_file.name]

    self.Log(' '.join(cmd_line))
    try:
      if self.is_pnacl_toolchain:
        # PNaCl toolchain executable is a script, not a binary, so it doesn't
        # want to run on Windows without a shell
        ecode = subprocess.call(' '.join(cmd_line), shell=True)
      else:
        ecode = subprocess.call(cmd_line)
    except Exception as err:
      raise Error('%s\nFAILED: %s' % (' '.join(cmd_line), str(err)))

    if temp_file is not None:
      RemoveFile(temp_file.name)
    return ecode

  def GetObjectName(self, src):
    if self.strip:
      src = src.replace(self.strip,'')
    _, filename = os.path.split(src)
    filename, _ = os.path.splitext(filename)
    if self.suffix:
      return os.path.join(self.outdir, filename + '.o')
    else:
      filename = os.path.split(src)[1]
      return os.path.join(self.outdir, os.path.splitext(filename)[0] + '.o')

  def CleanOutput(self, out):
    if IsFile(out):
      RemoveFile(out)

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

  def NeedsRebuild(self, outd, out, src, rebuilt=False):
    if not IsFile(self.toolstamp):
      if rebuilt:
        raise Error('Could not find toolchain stamp file %s.' % self.toolstamp)
      return True
    if not IsFile(outd):
      if rebuilt:
        raise Error('Could not find dependency file %s.' % outd)
      return True
    if not IsFile(out):
      if rebuilt:
        raise Error('Could not find output file %s.' % out)
      return True
    stamp_tm = GetMTime(self.toolstamp)
    out_tm = GetMTime(out)
    outd_tm = GetMTime(outd)
    src_tm = GetMTime(src)
    if IsStale(out_tm, stamp_tm, rebuilt):
      if rebuilt:
        raise Error('Output %s is older than toolchain stamp %s' % (
            out, self.toolstamp))
      return True
    if IsStale(out_tm, src_tm, rebuilt):
      if rebuilt:
        raise Error('Output %s is older than source %s.' % (out, src))
      return True

    if IsStale(outd_tm, src_tm, rebuilt):
      if rebuilt:
        raise Error('Dependency file is older than source %s.' % src)
      return True

    # Decode emitted makefile.
    with open(FixPath(outd), 'r') as fh:
      deps = fh.read()
    # Remove line continuations
    deps = deps.replace('\\\n', ' ')
    deps = deps.replace('\n', '')
    # The dependencies are whitespace delimited following the first ':'
    deps = deps.split(':', 1)[1]
    deps = deps.split()
    if sys.platform in ['win32', 'cygwin']:
      deps = [self.FixWindowsPath(d) for d in deps]
    # Check if any input has changed.
    for filename in deps:
      file_tm = GetMTime(filename)
      if IsStale(out_tm, file_tm, rebuilt):
        if rebuilt:
          raise Error('Dependency %s is older than output %s.' % (
              filename, out))
        return True

      if IsStale(outd_tm, file_tm, rebuilt):
        if rebuilt:
          raise Error('Dependency %s is older than dep file %s.' % (
              filename, outd))
        return True
    return False

  def Compile(self, src):
    """Compile the source with pre-determined options."""

    _, ext = os.path.splitext(src)
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
      if ext != '.h':
        self.Log('Skipping unknown type %s for %s.' % (ext, src))
      return None

    self.Log('\nCompile %s' % src)

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
    if self.gomacc:
      cmd_line.insert(0, self.gomacc)
    err = self.Run(cmd_line, out)
    if err:
      self.CleanOutput(outd)
      raise Error('FAILED with %d: %s' % (err, ' '.join(cmd_line)))
    else:
      try:
        self.NeedsRebuild(outd, out, src, True)
      except Error as e:
        raise Error('Failed to compile %s to %s with deps %s and cmdline:\t%s'
                    '\nNeedsRebuild returned error: %s' % (
                        src, out, outd, ' '.join(cmd_line), e))
    return out

  def Link(self, srcs):
    """Link these objects with predetermined options and output name."""
    out = self.LinkOutputName()
    self.Log('\nLink %s' % out)
    bin_name = self.GetCXXCompiler()
    MakeDir(os.path.dirname(out))
    self.CleanOutput(out)

    cmd_line = [bin_name, '-o', out, '-Wl,--as-needed']
    if not self.empty:
      cmd_line += srcs
    cmd_line += self.link_options

    err = self.Run(cmd_line, out)
    if err:
      raise Error('FAILED with %d: %s' % (err, ' '.join(cmd_line)))
    return out

  # For now, only support translating a pexe, and not .o file(s)
  def Translate(self, src):
    """Translate a pexe to a nexe."""
    out = self.TranslateOutputName()
    self.Log('\nTranslate %s' % out)
    bin_name = self.GetBinName('translate')
    cmd_line = [bin_name, '-arch', self.arch, src, '-o', out]
    cmd_line += self.link_options

    err = self.Run(cmd_line, out)
    if err:
      raise Error('FAILED with %d: %s' % (err, ' '.join(cmd_line)))
    return out

  def Archive(self, srcs):
    """Archive these objects with predetermined options and output name."""
    out = self.ArchiveOutputName()
    self.Log('\nArchive %s' % out)

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
    if err:
      raise Error('FAILED with %d: %s' % (err, ' '.join(cmd_line)))
    return out

  def Strip(self, src):
    """Strip the NEXE"""
    self.Log('\nStrip %s' % src)

    out = self.StripOutputName()
    pre_debug_tagging = self.UntaggedName()
    self.CleanOutput(out)
    self.CleanOutput(pre_debug_tagging)

    # Strip from foo.debug to foo.untagged.
    strip_name = self.GetStrip()
    strip_option = '--strip-all' if self.strip_all else '--strip-debug'
    # pnacl does not have an objcopy so there are no way to embed a link
    if self.is_pnacl_toolchain:
      cmd_line = [strip_name, strip_option, src, '-o', out]
      err = self.Run(cmd_line, out)
      if err:
        raise Error('FAILED with %d: %s' % (err, ' '.join(cmd_line)))
    else:
      cmd_line = [strip_name, strip_option, src, '-o', pre_debug_tagging]
      err = self.Run(cmd_line, pre_debug_tagging)
      if err:
        raise Error('FAILED with %d: %s' % (err, ' '.join(cmd_line)))

      # Tag with a debug link to foo.debug copying from foo.untagged to foo.
      objcopy_name = self.GetObjCopy()
      cmd_line = [objcopy_name, '--add-gnu-debuglink', src,
                  pre_debug_tagging, out]
      err = self.Run(cmd_line, out)
      if err:
        raise Error('FAILED with %d: %s' % (err, ' '.join(cmd_line)))

      # Drop the untagged intermediate.
      self.CleanOutput(pre_debug_tagging)

    return out

  def Finalize(self, src):
    """Finalize the PEXE"""
    self.Log('\nFinalize %s' % src)

    out = self.StripOutputName()
    self.CleanOutput(out)
    bin_name = self.GetPnaclFinalize()
    cmd_line = [bin_name, src, '-o', out]
    err = self.Run(cmd_line, out)
    if err:
      raise Error('FAILED with %d: %s' % (err, ' '.join(cmd_line)))
    return out

  def Generate(self, srcs):
    """Generate final output file.

    Link or Archive the final output file, from the compiled sources.
    """
    if self.outtype in ['nexe', 'pexe', 'nso']:
      out = self.Link(srcs)
      if self.is_pnacl_toolchain and self.finalize_pexe:
        # Note: pnacl-finalize also does stripping.
        self.Finalize(out)
      elif self.strip_all or self.strip_debug:
        self.Strip(out)
    elif self.outtype in ['nlib', 'plib']:
      out = self.Archive(srcs)
      if self.strip_debug:
        self.Strip(out)
      elif self.strip_all:
        raise Error('FAILED: --strip-all on libs will result in unusable libs.')
    else:
      raise Error('FAILED: Unknown outtype: %s' % (self.outtype))


def Main(argv):
  parser = OptionParser()
  parser.add_option('--empty', dest='empty', default=False,
                    help='Do not pass sources to library.', action='store_true')
  parser.add_option('--no-suffix', dest='suffix', default=True,
                    help='Do not append arch suffix.', action='store_false')
  parser.add_option('--sufix', dest='suffix',
                    help='Do append arch suffix.', action='store_true')
  parser.add_option('--strip-debug', dest='strip_debug', default=False,
                    help='Strip the NEXE for debugging', action='store_true')
  parser.add_option('--strip-all', dest='strip_all', default=False,
                    help='Strip the NEXE for production', action='store_true')
  parser.add_option('--strip', dest='strip', default='',
                    help='Strip the filename')
  parser.add_option('--nonstable-pnacl', dest='finalize_pexe', default=True,
                    help='Do not finalize pnacl bitcode for ABI stability',
                    action='store_false')
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
                    help='Set build type (<toolchain>_<outtype>, ' +
                    'where toolchain is newlib or glibc and outtype is ' +
                    'one of nexe, nlib, nso, pexe, or translate)')
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
  parser.add_option('--config-name', dest='build_config',
                    help='GYP build configuration name (Release/Debug)')
  options, files = parser.parse_args(argv[1:])

  if not argv:
    parser.print_help()
    return 1

  try:
    if options.source_list:
      source_list_handle = open(options.source_list, 'r')
      source_list = source_list_handle.read().splitlines()
      source_list_handle.close()
      files = files + source_list

    # Use set instead of list not to compile the same file twice.
    # To keep in mind that the order of files may differ from the .gypcmd file,
    # the set is not converted to a list.
    # Having duplicated files can cause race condition of compiling during
    # parallel build using goma.
    # TODO(sbc): remove the duplication and turn it into an error.
    files = set(files)

    # Fix slash style to insulate invoked toolchains.
    options.toolpath = os.path.normpath(options.toolpath)

    build = Builder(options)
    objs = []

    if build.outtype == 'translate':
      # Just translate a pexe to a nexe
      if len(files) != 1:
        parser.error('Pexe translation requires exactly one input file.')
      build.Translate(list(files)[0])
      return 0

    if build.gomacc:  # use goma build.
      returns = Queue.Queue()
      def CompileThread(filename, queue):
        try:
          queue.put(build.Compile(filename))
        except Exception:
          # Put current exception info to the queue.
          queue.put(sys.exc_info())
      build_threads = []
      # Start parallel build.
      for filename in files:
        thr = threading.Thread(target=CompileThread, args=(filename, returns))
        thr.start()
        build_threads.append(thr)
      for thr in build_threads:
        thr.join()
        out = returns.get()
        # An exception raised in the thread may come through the queue.
        # Raise it again here.
        if (isinstance(out, tuple) and len(out) == 3 and
            isinstance(out[1], Exception)):
          raise out[0], None, out[2]
        elif out:
          objs.append(out)
    else:  # slow path.
      for filename in files:
        out = build.Compile(filename)
        if out:
          objs.append(out)

    # Do not link if building an object. However we still want the output file
    # to be what was specified in options.name
    if options.compile_only:
      if len(objs) > 1:
        raise Error('--compile mode cannot be used with multiple sources')
      shutil.copy(objs[0], options.name)
    else:
      build.Generate(objs)
    return 0
  except Error as e:
    sys.stderr.write('%s\n' % e)
    return 1

if __name__ == '__main__':
  sys.exit(Main(sys.argv))
