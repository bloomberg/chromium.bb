# -*- python -*-
#
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Small utility library of python functions used by the various helper
scripts.
'''
# Needed for Python 2.5 -- Unnecessary (but harmless) for 2.6
from __future__ import with_statement

import optparse
import os
import sys

from SCons import Script
from site_tools import create_nmf

#------------------------------------------------------------------------------
# Parameters

# The newlib toolchain variant.  .nexes built with this variant require a
# different manifest file format than those built with glibc.
NEWLIB_TOOLCHAIN_VARIANT = 'newlib'

# The default toolchain architecture.
DEFAULT_TOOLCHAIN_ARCH = 'x86'

# The default toolchain variant.
DEFAULT_TOOLCHAIN_VARIANT = NEWLIB_TOOLCHAIN_VARIANT

# Map the string stored in |sys.platform| into a toolchain host specifier.
# @private
__PLATFORM_TO_HOST_MAP = {
    'win32': 'windows',
    'cygwin': 'windows',
    'linux2': 'linux',
    'darwin': 'mac'
}

# Map a platform host to a map of possible architectures and toolchains.
# The platform host can be derived from sys.platform using |PLATFORM_HOST_MAP_}|
# (see above). (Note: if this map changes, you must also update the copy in
# set_nacl_env.py.)
# TODO(gwink): This map is duplicated in set_nacl_env.py. Find a good way to
# share it instead.
# @private
__HOST_TO_TOOLCHAIN_MAP = {
    'windows': {
        'x86': {
            'glibc': 'win_x86',
            'newlib': 'win_x86_newlib',
        },
    },
    'linux': {
        'x86': {
            'glibc': 'linux_x86',
            'newlib': 'linux_x86_newlib',
        },
        'pnacl': {
            'newlib-32': 'pnacl_linux_i686_newlib',
            'newlib-64': 'pnacl_linux_x86_64_newlib',
            'glibc-64': 'pnacl_linux_x86_64_glibc',
        },
        'arm': {
            'newlib': 'pnacl_linux_x86_64_newlib',
            'glibc': 'pnacl_linux_x86_64_glibc',
        },
    },
    'mac': {
        'x86': {
            'glibc': 'mac_x86',
            'newlib': 'mac_x86_newlib',
        },
        'pnacl': {
            'newlib': 'pnacl_darwin_i386_newlib',
        },
    },
}

# Various architecture spec objects suitable for use with
# nacl_env_ext.SetArchFlags()
ARCH_SPECS = {
    'x86-32': {
        'arch': 'x86',
        'subarch': '32'
    },
    'x86-64': {
        'arch': 'x86',
        'subarch': '64'
    },
    'arm': {
        'arch': 'ARM',
        'subarch': ''
    }
}

# Default values for 'arch' and 'subarch'.
DEFAULT_ARCH = 'x86'
DEFAULT_SUBARCH = '32'

#------------------------------------------------------------------------------
# Functions

def AddCommandLineOptions():
  '''Register the cmd-line options.

  Registers --nacl-platform, --architecture and --variant. This function can be
  called multiple times. Only the first call registers the option.

  Args:
    None

  Returns:
    None
  '''

  try:
    Script.AddOption(
        '--nacl-platform',
        dest='nacl_platform',
        nargs=1,
        type='string',
        action='store',
        help='target pepper version')
    Script.Help('  --nacl-platform      '
                'Specify the pepper version to build for '
                '(e.g. --nacl-platform="pepper_14").\n')

    Script.AddOption(
        '--architecture',
        dest='architecture',
        nargs=1,
        type='string',
        action='store',
        help='NaCl target architecture')
    Script.Help('  --architecture       '
                'Specify the NaCl target architecture to build (e.g. '
                '--architecture="glibc").  Possible values are "x86", "pnacl" '
                '"arm".  Not all target architectures are available on all '
                'host platforms.  Defaults to "x86"\n')

    Script.AddOption(
        '--variant',
        dest='variant',
        nargs=1,
        type='string',
        action='store',
        help='NaCl toolchain variant')
    Script.Help('  --variant            '
                'Specify the NaCl toolchain variant to use when '
                'building (e.g. --variant="glibc").  Possible values are '
                '"glibc", "newlib"; when --architecture=pnacl is specified, '
                'values must include bit-width, e.g. "glibc-64".  Defaults to '
                '"newlib"\n')

  except optparse.OptionConflictError:
    pass


def PrintNaclPlatformBanner(module_name, nacl_platform, variant):
  '''Print a banner that shows what nacl platform is used to build a module.

  Args:
    module_name: The name of the module. Printed as-is.
    nacl_platform: The name - a.k.a. folder name - of the nacl platform.
                   Printed as-is.
    variant: The toolchain variant, one of 'newlib', 'glibc', 'pnacl', etc.
  Returns:
    None
  '''

  # Don't print the banner if we're just cleaning files.
  if not Script.GetOption('clean'):
    print '---------------------------------------------------------------'
    print ('+  Project "%s" is using NaCl platform "%s", toolchain "%s"' %
        (module_name, nacl_platform, variant))
    print '---------------------------------------------------------------'
    print ''
    sys.stdout.flush()


def ToolchainPath(base_dir=None,
                  arch=DEFAULT_TOOLCHAIN_ARCH,
                  variant=DEFAULT_TOOLCHAIN_VARIANT):
  '''Build a toolchain path based on the platform type.

  |base_dir| is the root directory which includes the platform-specific
  toolchain.  This could be something like "/usr/local/mydir/nacl_sdk/src".  If
  |base_dir| is None, then the environment variable NACL_SDK_ROOT is used (if
  it's set).  This method assumes that the platform-specific toolchain is found
  under <base_dir>/toolchain/<platform_spec>.

  Args:
    base_dir: The pathname of the root directory that contains the toolchain.
              The toolchain is expected to be in a dir called 'toolchain'
              within |base_dir|.
    variant: The toolchain variant, can be one of 'newlib', 'glibc', etc.
             Defaults to 'newlib'.
  Returns:
    The concatenated platform-specific path to the toolchain.  This will look
    like base_dir/toolchain/mac_x86
  '''

  if base_dir is None:
    base_dir = os.getenv('NACL_SDK_ROOT', '')
  if sys.platform in __PLATFORM_TO_HOST_MAP:
    host_platform = __PLATFORM_TO_HOST_MAP[sys.platform]
    toolchain_map = __HOST_TO_TOOLCHAIN_MAP[host_platform]
    if arch in toolchain_map:
      isa_toolchain = toolchain_map[arch]
      if variant in isa_toolchain:
        return os.path.normpath(os.path.join(
            base_dir, 'toolchain', isa_toolchain[variant]))
      else:
        raise ValueError('ERROR: Variant "%s" not in toolchain "%s/%s".' %
                         (variant, host_platform, arch))
    else:
      raise ValueError('ERROR: Architecture "%s" not supported on host "%s".' %
                       (arch, host_platform))

  else:
    raise ValueError('ERROR: Unsupported host platform "%s".' % sys.platform)


def GetJSONFromNexeSpec(nexe_spec):
  '''Generate a JSON string that represents the architecture-to-nexe mapping
  in |nexe_spec|.

  The nexe spec is a simple dictionary, whose keys are architecture names and
  values are the nexe files that should be loaded for the corresponding
  architecture.  For example:
      {'x86-32': 'hello_world_x86_32.nexe',
       'x86-64': 'hello_world_x86_64.nexe',
       'arm': 'hello_world_ARM.nexe'}

  Args:
    nexe_spec: The dictionary that maps architectures to .nexe files.
  Returns:
    A JSON string representing |nexe_spec|.
  '''
  nmf_json = '{\n'
  nmf_json += '  "program": {\n'

  # Add an entry in the JSON for each specified architecture.  Note that this
  # loop emits a trailing ',' for every line but the last one.
  if nexe_spec and len(nexe_spec):
    line_count = len(nexe_spec)
    for arch_key in nexe_spec:
      line_count -= 1
      eol_char = ',' if line_count > 0 else ''
      nmf_json += '    "%s": {"url": "%s"}%s\n' % (arch_key,
                                                   nexe_spec[arch_key],
                                                   eol_char)

  nmf_json += '  }\n'
  nmf_json += '}\n'
  return nmf_json


def GenerateNmf(target, source, env):
  '''This function is used to create a custom Builder that produces .nmf files.

  The .nmf files are given in the list of targets.  This expects the .nexe
  mapping to be given as the value of the 'nexes' keyword in |env|.  To add
  this function as a Builder, use this SCons code:
      gen_nmf_builder = nacl_env.Builder(suffix='.nmf',
                                         action=nacl_utils.GenerateNmf)
      nacl_env.Append(BUILDERS={'GenerateNmf': gen_nmf_builder})
  To invoke the Builder, do this, for example:
      # See examples/hello_world/build.scons for more details.
      hello_world_opt_nexes = [nacl_env.NaClProgram(....), ....]
      nacl_env.GenerateNmf(target='hello_world.nmf',
                           source=hello_world_opt_nexes,
                           nexes={'x86-32': 'hello_world_x86_32.nexe',
                                  'x86-64': 'hello_world_x86_64.nexe',
                                  'arm': 'hello_world_ARM.nexe'})

  A Builder that invokes this function is added to the NaCl Environment by
  the NaClEnvironment() function in make_nacl_env.py

  Args:
    target: The list of targets to build.  This is expected to be a list of
            File Nodes that point to the required .nmf files.
    source: The list of sources that the targets depend on.  This is typically
            a list of File Nodes that represent .nexes
    env: The SCons construction Environment that provides the build context.
  Returns:
    None on success.  Raises a ValueError() if there are missing parameters,
    such as the 'nexes' keyword in |env|.
  '''

  if target == None or source == None:
    raise ValueError('No value given for target or source.')

  nexes = env.get('nexes', [])
  if len(nexes) == 0:
    raise ValueError('No value for "nexes" keyword.')

  for target_file in target:
    # If any of the following functions raises an exception, just let the
    # exception bubble up to the calling context.  This will produce the
    # correct SCons error.
    target_path = target_file.get_abspath()
    if env['NACL_TOOLCHAIN_VARIANT'] == NEWLIB_TOOLCHAIN_VARIANT:
      nmf_json = GetJSONFromNexeSpec(nexes)
    else:
      nmf = create_nmf.NmfUtils(
          objdump=os.path.join(env['NACL_TOOLCHAIN_ROOT'], 'bin',
                               'x86_64-nacl-objdump'),
          main_files=[str(file) for file in source],
          lib_path=[os.path.join(env['NACL_TOOLCHAIN_ROOT'], 'x86_64-nacl', dir)
                    for dir in ['lib', 'lib32']],
          lib_prefix=env['NACL_LIB_PREFIX'])
      nmf_json = nmf.GetJson()
      if env.get('NACL_INSTALL_ROOT', None):
        nmf.StageDependencies(env['NACL_INSTALL_ROOT'])
    with open(target_path, 'w') as nmf_file:
      nmf_file.write(nmf_json)

  # Return None to indicate success.
  return None


def GetArchFromSpec(arch_spec):
  '''Pick out the values for 'arch' and 'subarch' from |arch_spec|, providing
  default values in case nothing is specified.

  Args:
    arch_spec: An object that can have keys 'arch' and 'subarch'.
  Returns:
    A tuple (arch, subarch) that contains either the values of the
        corresponding keys in |arch_spec| or a default value.
  '''
  if arch_spec == None:
    return (DEFAULT_ARCH, DEFAULT_SUBARCH)
  arch = arch_spec.get('arch', DEFAULT_ARCH)
  subarch = arch_spec.get('subarch', DEFAULT_SUBARCH)
  return (arch, subarch)


def GetArchName(arch_spec):
  ''' Return a name of the form arch_subarch for the given arch spec.

  Args:
    arch_spec: An object containing 'arch' and 'subarch' keys that describe
        the instruction set architecture of the output program.  See
        |ARCH_SPECS| in nacl_utils.py for valid examples.

  Returns:
    A string with the arch name.
  '''
  return '%s_%s' % GetArchFromSpec(arch_spec)


def MakeNaClCommonEnvironment(nacl_env,
                              arch_spec=ARCH_SPECS['x86-32'],
                              is_debug=False):
  '''Make a clone of nacl_env that is suitable for building programs or
  libraries for a specific build variant.

  Make a cloned NaCl Environment and setup variables for options like optimized
  versus debug CCFLAGS.

  Args:
    nacl_env: A SCons construction environment.  This is typically the return
        value of NaClEnvironment() (see above).
    arch_spec: An object containing 'arch' and 'subarch' keys that describe
        the instruction set architecture of the output program.  See
        |ARCH_SPECS| in nacl_utils.py for valid examples.
    is_debug: Indicates whether this program should be built for debugging or
        optimized.

  Returns:
    A SCons Environment setup with options for the specified variant of a NaCl
    module or library.
  '''
  arch_name = GetArchName(arch_spec)
  env = nacl_env.Clone()
  env.AppendOptCCFlags(is_debug)
  env.AppendArchFlags(arch_spec)

  # Wrap linker command with TEMPFILE so that if lines are longer than
  # MAXLINELENGTH, the tools will be run with @tmpfile. This isn't needed
  # for any of the sdk examples, but if people cargo cult them for other
  # purposes, they can end up hitting command line limits on Windows where
  # MAXLINELENGTH can be as low as 2048.
  env['LINKCOM'] = '${TEMPFILE("' + env['LINKCOM'] + '")}'
  env['SHLINKCOM'] = '${TEMPFILE ' + env['SHLINKCOM'] + '")}'

  return env


def MakeNaClModuleEnvironment(nacl_env,
                              sources,
                              module_name='nacl',
                              arch_spec=ARCH_SPECS['x86-32'],
                              is_debug=False,
                              build_dir_prefix=''):
  '''Make a NaClProgram Node for a specific build variant.

  Make a NaClProgram Node in a cloned Environment.  Set the environment
  in the cloned Environment variables for things like optimized versus debug
  CCFLAGS, and also adds a Program builder that makes a NaCl module.  The name
  of the module is derived from |module_name|, |arch_spec| and |is_debug|; for
  example:
    MakeNaClModuleEnvironment(nacl_env, sources, module_name='hello_world',
        arch_spec=nacl_utils.ARCH_SPECS['x86-64'], is_debug=True)
  will produce a NaCl module named
    hello_world_x86_64_dbg.nexe

  Args:
    nacl_env: A SCons construction environment.  This is typically the return
        value of NaClEnvironment() (see above).
    sources: A list of source Nodes used to build the NaCl module.
    module_name: The name of the module.  The name of the output program
        incorporates this as its prefix.
    arch_spec: An object containing 'arch' and 'subarch' keys that describe
        the instruction set architecture of the output program.  See
        |ARCH_SPECS| in nacl_utils.py for valid examples.
    is_debug: Indicates whether this program should be built for debugging or
        optimized.
    build_dir_prefix: Allows user to prefix the build directory with an
        additional string.

  Returns:
    A SCons Environment that builds the specified variant of a NaCl module.
  '''
  debug_name = 'dbg' if is_debug else 'opt'
  arch_name = GetArchName(arch_spec)
  env = MakeNaClCommonEnvironment(nacl_env, arch_spec, is_debug)
  return env.NaClProgram('%s_%s%s' % (module_name,
                                      arch_name,
                                      '_dbg' if is_debug else ''),
                         sources,
                         variant_dir='%s%s_%s' %
                             (build_dir_prefix, debug_name, arch_name))


def MakeNaClStaticLibEnvironment(nacl_env,
                                 sources,
                                 lib_name='nacl',
                                 arch_spec=ARCH_SPECS['x86-32'],
                                 is_debug=False,
                                 build_dir_prefix='',
                                 lib_dir=''):
  '''Make a NaClStaticLib Node for a specific build variant.

  Make a NaClStaticLib Node in a cloned Environment.  Set the environment
  in the cloned Environment variables for things like optimized versus debug
  CCFLAGS, and also adds a Program builder that makes a NaCl static library.
  The name of the library is derived from |lib_name|, |arch_spec| and
  |is_debug|; for example:
    MakeNaClStaticLibEnvironment(nacl_env, sources, lib_name='c_salt',
        arch_spec=nacl_utils.ARCH_SPECS['x86-64'], is_debug=True)
  will produce a NaCl static library named
    libc_salt_x86_64_dbg.a

  Args:
    nacl_env: A SCons construction environment.  This is typically the return
        value of NaClEnvironment() (see above).
    sources: A list of source Nodes used to build the NaCl library.
    lib_name: The name of the library.
    arch_spec: An object containing 'arch' and 'subarch' keys that describe
        the instruction set architecture of the output program.  See
        |ARCH_SPECS| in nacl_utils.py for valid examples.
    is_debug: Indicates whether this program should be built for debugging or
        optimized.
    build_dir_prefix: Allows user to prefix the build directory with an
        additional string.
    lib_dir: Where to output the final library file. Lib files are placed in
        directories within lib_dir, based on the arch type and build type.
        E.g. Specifying lib_dir = 'dest' for lib name 'c_salt' on x86-32 debug
        creates the library file 'dest/lib-x86-32/dbg/libc_salt.a'

  Returns:
    A SCons Environment that builds the specified variant of a NaCl module.
  '''
  env = MakeNaClCommonEnvironment(nacl_env, arch_spec, is_debug)
  debug_name = 'dbg' if is_debug else 'opt'
  arch_name = GetArchName(arch_spec)
  target_name = '%s_%s%s' % (lib_name, arch_name, debug_name)
  variant_dir = '%s%s_%s' % (build_dir_prefix, debug_name, arch_name)

  arch, subarch = GetArchFromSpec(arch_spec)
  lib_subdir = 'lib-' + arch
  if subarch: lib_subdir = lib_subdir + '-' + subarch
  lib_dir = os.path.join(lib_dir, lib_subdir, debug_name)

  return env.NaClStaticLib(target_name,
                           sources,
                           variant_dir,
                           lib_name,
                           lib_dir)
