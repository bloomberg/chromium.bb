# -*- python -*-
#
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Construct an Environment that uses the NaCl toolchain to build C/C++ code.
The base dir for the NaCl toolchain is in the NACL_SDK_ROOT environment
variable.
'''

import nacl_utils
import os

from SCons import Script

def NaClEnvironment(use_c_plus_plus_libs=False,
                    nacl_platform=None,
                    toolchain_arch=None,
                    toolchain_variant=None,
                    use_ppapi=True,
                    install_subdir=None,
                    lib_prefix=None):
  '''Make an Environment that uses the NaCl toolchain to build sources.

  This modifies a default construction Environment to point the compilers and
  other bintools at the NaCl-specific versions, adds some tools that set certain
  build flags needed by the NaCl-specific tools, and adds a custom Builder that
  generates .nmf files.

  Args:
    use_c_plus_plus_libs: Indicate whether to insert the C++ NaCl libs at the
        right place in the list of LIBS.
    nacl_platform: The target NaCl/Chrome/Papper platform for which the
        environment, e.g. 'pepper_14'.
    toolchain_arch: The target architecture of the toolchain (e.g., x86, pnacl)
    toolchain_variant: The libc of the toolchain (e.g., newlib, glibc)
    use_ppapi: flag indicating whether to compile again ppapi libraries
    install_subdir: subdirectory within the NACL_INSTALL_ROOT for this project.
    lib_prefix: an optional list of path components to prepend to the library
        path.  These components are joined with appropriate path separators
        Examples: ['..', '..'], ['..', 'peer_directory'].
  Returns:
    A SCons Environment with all the various Tool and keywords set to build
    NaCl modules.
  '''

  def GetCommandLineOption(option_name, option_value, option_default):
    '''Small helper function to get a command line option.

    Returns a command-line option value, which can be overridden.  If the
    option is set on the command line, then that value is favoured over the
    internally set value.  If option is neither set on the command line nor
    given a value, its default is used.

    Args:
      option_name: The name of the command line option, e.g. "variant".
      option_value: The initial value of the option. This value is used if the
          its not set via the command line.  Can be None.
      option_default: If the option value hasn't been set via the command line
          nor via an internal value, then this default value is used.  Can be
          None.

    Returns:
      The value of the command-line option, according to the override rules
          described above.
    '''
    cmd_line_value = Script.GetOption(option_name)
    if not cmd_line_value and not option_value:
      cmd_line_value = option_default
    return cmd_line_value or option_value

  nacl_utils.AddCommandLineOptions()
  env = Script.Environment()

  # We must have a nacl_platform, either as argument to this function or from
  # the command line. However, if we're cleaning we can relax this requirement.
  # (And our build bots will be much happier that way.)
  nacl_platform_from_option = Script.GetOption('nacl_platform')
  if not nacl_platform_from_option and not nacl_platform:
    if Script.GetOption('clean'):
      nacl_platform_from_option='.'
    else:
      raise ValueError('NaCl platform not specified')

  # Setup the base dir for tools, etc. Favor the nacl platform specified on
  # the command line if there's a conflict.
  nacl_platform_to_use = nacl_platform_from_option or nacl_platform

  toolchain_variant = GetCommandLineOption(
      'variant', toolchain_variant, nacl_utils.DEFAULT_TOOLCHAIN_VARIANT)
  toolchain_arch = GetCommandLineOption(
      'architecture', toolchain_arch, nacl_utils.DEFAULT_TOOLCHAIN_ARCH)

  base_dir = os.getenv('NACL_SDK_ROOT', '')
  base_dir = os.path.join(base_dir, nacl_platform_to_use)
  toolchain = nacl_utils.ToolchainPath(base_dir=base_dir,
                                       arch=toolchain_arch,
                                       variant=toolchain_variant)
  if (toolchain is None):
    raise ValueError('Cannot find a NaCl toolchain')

  tool_bin_path = os.path.join(toolchain, 'bin')
  tool_runtime_path = os.path.join(toolchain, 'runtime')
  staging_dir = os.path.abspath(os.getenv(
      'NACL_INSTALL_ROOT', os.path.join(os.getenv('NACL_SDK_ROOT', '.'),
                                        'staging')))
  if install_subdir:
    staging_dir = os.path.join(staging_dir, install_subdir)
  lib_prefix = lib_prefix or []
  if type(lib_prefix) is not list:
    # Break path down into list of directory components
    lib_prefix = filter(lambda x:x, lib_prefix.split('/'))

  # Invoke the various *nix tools that the NativeClient SDK resembles.  This
  # is done so that SCons doesn't try to invoke cl.exe on Windows in the
  # Object builder.
  env.Tool('g++')
  env.Tool('gcc')
  env.Tool('gnulink')
  env.Tool('ar')
  env.Tool('as')

  env.Tool('nacl_tools')
  # TODO(dspringer): Figure out how to make this dynamic and then compute it
  # based on the desired target arch.
  env.Replace(tools=['nacl_tools'],
              # Replace the normal unix tools with the NaCl ones.  Note the
              # use of the NACL_ARCHITECTURE prefix for the tools.  This
              # Environment variable is set in nacl_tools.py; it has no
              # default value.
              CC=os.path.join(tool_bin_path, '${NACL_ARCHITECTURE}gcc'),
              CXX=os.path.join(tool_bin_path, '${NACL_ARCHITECTURE}g++'),
              AR=os.path.join(tool_bin_path, '${NACL_ARCHITECTURE}ar'),
              AS=os.path.join(tool_bin_path, '${NACL_ARCHITECTURE}as'),
              GDB=os.path.join(tool_bin_path, '${NACL_ARCHITECTURE}gdb'),
              # NOTE: use g++ for linking so we can handle C AND C++.
              LINK=os.path.join(tool_bin_path, '${NACL_ARCHITECTURE}g++'),
              LD=os.path.join(tool_bin_path, '${NACL_ARCHITECTURE}ld'),
              STRIP=os.path.join(tool_bin_path, '${NACL_ARCHITECTURE}strip'),
              NACL_SEL_LDR32=os.path.join(tool_bin_path, 'sel_ldr_x86_32'),
              NACL_IRT_CORE32=os.path.join(tool_runtime_path,
                                           'irt_core_x86_32.nexe'),
              NACL_SEL_LDR64=os.path.join(tool_bin_path, 'sel_ldr_x86_64'),
              NACL_IRT_CORE64=os.path.join(tool_runtime_path,
                                           'irt_core_x86_64.nexe'),
              RANLIB=os.path.join(tool_bin_path, '${NACL_ARCHITECTURE}ranlib'),
              ASFLAGS=['${EXTRA_ASFLAGS}',
                      ],
              # c specific
              EXTRA_CFLAGS=[],
              CFLAGS=['${EXTRA_CFLAGS}',
                      '-std=gnu99',
                      ],
              # c++ specific
              EXTRA_CXXFLAGS=[],
              CXXFLAGS=['${EXTRA_CXXFLAGS}',
                        '-std=gnu++98',
                        '-Wno-long-long',
                        ],
              # Both C and C++
              CCFLAGS=['${EXTRA_CCFLAGS}',
                       '-Wall',
                       '-Wswitch-enum',
                       '-pthread',
                      ],
              CPPDEFINES=[# _GNU_SOURCE ensures that strtof() gets declared.
                         ('_GNU_SOURCE', 1),
                         # This ensures that PRId64 etc. get defined.
                         ('__STDC_FORMAT_MACROS', '1'),
                         # strdup, and other common stuff
                         ('_BSD_SOURCE', '1'),
                         ('_POSIX_C_SOURCE', '199506'),
                         ('_XOPEN_SOURCE', '600'),
                         ],
              CPPPATH=[],
              LINKFLAGS=['${EXTRA_LINKFLAGS}',
                        ],
              # The NaCl environment makes '.nexe' executables.  If this is
              # not explicitly set, then SCons on Windows doesn't understand
              # how to construct a Program builder properly.
              PROGSUFFIX='.nexe',
              # Target NaCl platform info.
              TARGET_NACL_PLATFORM=nacl_platform_to_use,
              NACL_TOOLCHAIN_VARIANT=toolchain_variant,
              NACL_TOOLCHAIN_ROOT=toolchain,
              NACL_INSTALL_ROOT=staging_dir,
              NACL_LIB_PREFIX=lib_prefix,
             )
  # This supresses the "MS_DOS style path" warnings on Windows.  It's benign on
  # all other platforms.
  env['ENV']['CYGWIN'] = 'nodosfilewarning'

  # Append the common NaCl libs.
  if use_ppapi:
    common_nacl_libs = ['ppapi']
    if use_c_plus_plus_libs:
      common_nacl_libs.extend(['ppapi_cpp'])
    env.Append(LIBS=common_nacl_libs)

  gen_nmf_builder = env.Builder(suffix='.nmf',
                                action=nacl_utils.GenerateNmf)
  env.Append(BUILDERS={'GenerateNmf': gen_nmf_builder})

  return env
