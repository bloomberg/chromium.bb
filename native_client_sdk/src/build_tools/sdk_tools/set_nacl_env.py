#!/usr/bin/env python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

''' A tool to setup the NaCl build env and invoke a command such as make '''

__author__ = 'gwink@google.com (Georges Winkenbach)'

import optparse
import os
import re
import subprocess
import sys

# The default sdk platform to use if the user doesn't specify one.
__DEFAULT_SDK_PLATFORM = 'pepper_15'

# Usage info.
__GLOBAL_HELP = '''%prog options [command]

set-nacl-env is a utility that sets up the environment required to build
NaCl modules and invokes an optional command in a shell. If no command
is specified, set-nacl-env spawns a new shell instead. Optionally, the user
can request that the settings are printed to stdout.
'''

# Map the string stored in |sys.platform| into a toolchain host specifier.
__PLATFORM_TO_HOST_MAP = {
    'win32': 'windows',
    'cygwin': 'windows',
    'linux2': 'linux',
    'darwin': 'mac',
    }

# Map key triplet of (host, arch, variant) keys to the corresponding subdir in
# the toolchain path. For instance (mac, x86-32, newlib) maps to mac_x86_newlib.
# Note to NaCl eng.: this map is duplicated in nack_utils.py; you must keep them
# synched.
__HOST_TO_TOOLCHAIN_MAP = {
  'mac': {                           # Host     arch     variant
    'x86-32': {
      'newlib': 'mac_x86_newlib',    # Mac      x86-32   newlib
      'glibc' : 'mac_x86'},          # Mac      x86-32   glibc
    'x86-64': {
      'newlib': 'mac_x86_newlib',    # Mac      x86-64   newlib
      'glibc' : 'mac_x86'},          # Mac      x86-64   glibc
    },
  'windows': {
    'x86-32': {
      'newlib': 'win_x86_newlib',    # Windows  x86-32   newlib
      'glibc' : 'win_x86'},          # Windows  x86-32   glibc
    'x86-64': {
      'newlib': 'win_x86_newlib',    # Windows  x86-64   newlib
      'glibc' : 'win_x86'},          # Windows  x86-64   glibc
    },
  'linux': {
    'x86-32': {
      'newlib': 'linux_x86_newlib',  # Windows  x86-32   newlib
      'glibc' : 'linux_x86'},        # Windows  x86-32   glibc
    'x86-64': {
      'newlib': 'linux_x86_newlib',  # Windows  x86-64   newlib
      'glibc' : 'linux_x86'},        # Windows  x86-64   glibc
    },
  }

# Map architecture specification to the corresponding tool-name prefix.
# @private
__VALID_ARCH_SPECS = {
    'x86-32': 'i686',
    'x86-64': 'x86_64',
    }

# Valid lib variants.
__VALID_VARIANT = ['glibc', 'newlib']

# Lists of env keys for build tools. Note: Each matching value is actually a
# format template with fields for 'prefix' such as 'i686-nacl-' and 'extras'
# such as ' -m64'.
__BUILD_TOOLS = {
    'CC': '{prefix}gcc{extras}',
    'CXX': '{prefix}g++{extras}',
    'AR': '{prefix}ar{extras}',
    'LINK': '{prefix}g++{extras}',
    'STRIP': '{prefix}strip',
    'RANLIB': '{prefix}ranlib',
    }

# List of env keys for build options with corresponding settings that are
# common to all build configurations.
__BUILD_OPTIONS = {
    'CFLAGS': ['-std=gnu99', '-Wall', '-Wswitch-enum', '-g'],
    'CXXFLAGS': ['-std=gnu++98', '-Wswitch-enum', '-g', '-pthread'],
    'CPPFLAGS': ['-D_GNU_SOURCE=1', '-D__STDC_FORMAT_MACROS=1'],
    'LDFLAGS': [],
    }

# All the build-flags env keys in one list.
__ALL_ENV_KEYS = __BUILD_TOOLS.keys() + __BUILD_OPTIONS.keys()

# Map build types to the corresponding build flags.
__BUILD_TYPES = {
    'debug': ['-O0'],
    'release': ['-O3'],
    }


def FormatOptionList(option_list, prefix='', separator=' '):
  ''' Format a list of build-option items into a string.

  Format a list of build-option items into a string suitable for output.

  Args:
    prefix: a prefix string to prepend to each list item. For instance,
        prefix='-D' with item='__DEBUG' generates '-D__DEBUG'.
    separator: a separator string to insert between items.

  Returns:
    A formatted string. An empty string if list is empty.
  '''
  return separator.join([prefix + item for item in option_list])


def GetNaclSdkRoot():
  ''' Produce a string with the full path to the NaCl SDK root.

  The path to nacl-sdk root is derived from one of two sources. First, if
  NACL_SDK_ROOT is defined in env it is assumed to contain the desired sdk
  root. That makes it possible for this tool to run from any location. If
  NACL_SDK_ROOT is not defined or is empty, the sdk root is taken as the
  parent directory to this script's file. This works well when this script
  is ran from the sdk_tools directory in the standard SDK installation.

  Returns:
    A string with the path to the NaCl SDK root.
  '''
  if 'NACL_SDK_ROOT' in os.environ:
    return os.environ['NACL_SDK_ROOT']
  else:
    SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
    SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(SCRIPT_DIR)))
    return os.path.join(SRC_DIR, 'native_client')


def GetToolchainPath(options):
  ''' Build the path to the toolchain directory.

  Given the host, sdk-root directory, sdk platform, architecture and library
  variant, this function builds the path to the toolchain directory.

  Examples:
    For
      sdk_root == 'c:/cool_code/nacl_sdk'
      arch == 'x86-32'
      lib variant == 'newlib'
      nacl platform = 'pepper_17'
      host == 'mac'
    this function returns :
      toolchain_path == /cool_code/nacl_sdk/pepper_17/toolchain/mac_x86_newlib

  Args:
    options: the options instances containing attributes options.host,
        options.arch, options.lib_variant, options.sdk_root and
        options.sdk_platform.

  Returns:
    A string containing the absolute path to the base directory for the
    toolchain.
  '''
  host = options.host
  arch = options.arch
  variant = options.lib_variant
  toolchain_dir = __HOST_TO_TOOLCHAIN_MAP[host][arch][variant]
  base_dir = os.path.abspath(options.sdk_root)
  return os.path.join(base_dir, options.sdk_platform, 'toolchain',
                      toolchain_dir)


def ConfigureBaseEnv(merge):
  ''' Configure and return a new env instance with the essential options.

  Create and return a new env instance with the base configuration. That env
  contains at least an empty entry for each key defined  in __ALL_ENV_KEYS.
  However, if merge is True, a copy of the current os.environ is used to seed
  env.

  Argument:
    merge: True ==> merge build configuration with os.environ..

  Returns:
    A base env map.
  '''
  env = {}
  if merge:
    for key, value in os.environ.items():
      env[key] = [value]
  # Ensure that every env key has a default definition.
  for key in __ALL_ENV_KEYS:
    env.setdefault(key, [])
  return env


def SetBuildTools(env, tool_bin_path, tool_prefix, extras_flags=''):
  ''' Configure the build tools build flags in env.

  Given the absolute path to the toolchain's bin directory, tool_prefix and
  optional extra_flags build flags, set the entries for the build tools
  in env. For instance, using the sample path from GetToolchainPath above and
  tool_prefix = 'i686-nacl-' we would get

  env['CC'] =
      '/cool_code/nacl_sdk/pepper_17/toolchain/mac_x86_newlib/bin/i686-nacl-gcc'

  Args:
    env: the env map to setup.
    tool_bin_path: the absolute path to the toolchain's bin directory.
    tool_prefix: a string with the tool's prefix, such as 'i686-nacl-'.
    extra_flags: optional extra flags, such as ' -m64'.
  '''
  for key, val in __BUILD_TOOLS.iteritems():
    tool_name = val.format(prefix=tool_prefix, extras=extras_flags)
    env[key] = os.path.join(tool_bin_path, tool_name)


def SetRuntimeTools(env, tool_runtime_path):
  ''' Setup the runtime tools in env.

  Given an absolute path to the toolchain's runtime directory, setup the
  entries for the runtime tools in env.

  Args:
    env: the env map to setup.
    tool_runtime_path: the absolute path to the toolchain's runtime directory.
  '''
  env['NACL_IRT_CORE32'] = os.path.join(tool_runtime_path,
                                        'irt_core_x86_32.nexe')
  env['NACL_IRT_CORE64'] = os.path.join(tool_runtime_path,
                                        'irt_core_x86_64.nexe')


def SetCommonBuildOptions(env, options):
  ''' Set the common build options, such as CFLAGS.

  Set the build options, such as CFLAGS that are common to all build
  configurations, given the built type.

  Args:
    env: the env map to set.
    build_type: one of 'debug' or 'release'.
  '''
  # Set the build flags from __BUILD_OPTIONS.
  for key, val in __BUILD_OPTIONS.iteritems():
    env[key].extend(val)
  # Add the build-type specific flags.
  env['CFLAGS'].extend(__BUILD_TYPES[options.build_type])
  env['CXXFLAGS'].extend(__BUILD_TYPES[options.build_type])
  if not options.no_ppapi:
    env['LDFLAGS'].extend(['-lppapi'])


def SetupX86Env(options):
  ''' Generate a new env map for X86 builds.

  Generate and return a new env map for x86-NN architecture. The NN bit
  size is derived from options.arch.

  Argument:
    options: the cmd-line options.

  Returns:
    A new env map with the build configuration flags set.
  '''
  env = ConfigureBaseEnv(options.merge)

  # Where to find tools and libraries within the toolchain directory.
  tool_bin_path = os.path.join(options.toolchain_path, 'bin')
  tool_runtime_path = os.path.join(options.toolchain_path, 'runtime')

  # Store the bin paths into env. This isn't really part of the build
  # environment. But it's nice to have there for reference.
  env['NACL_TOOL_BIN_PATH'] = tool_bin_path
  env['NACL_TOOL_RUNTIME_PATH'] = tool_runtime_path

  if options.arch == 'x86-32':
    SetBuildTools(env, tool_bin_path, 'i686-nacl-', extras_flags=' -m32')
  else:
    assert(options.arch == 'x86-64')
    SetBuildTools(env, tool_bin_path, 'x86_64-nacl-', extras_flags=' -m64')
  SetRuntimeTools(env, tool_runtime_path)
  SetCommonBuildOptions(env, options)
  return env


def dump(options, env, template):
  ''' Dump the build settings in env to stdout.

  Args:
    options: the cmd-line options, used to output the target buid configuartion.
    env: the env map with the build flags.
    template: a fiormatting template used to format options output. It must
        contain format fields 'option' and 'value'.
  '''
  if options.pretty_print:
    print '\nConfiguration:'
    print '-------------'
    print '  Host = %s' % options.host
    print '  NaCl SDK root = %s' % options.sdk_root
    print '  SDK platform = %s' % options.sdk_platform
    print '  Target architecture = %s' % options.arch
    print '  Lib variant = %s' % options.lib_variant

  if options.pretty_print:
    print '\nNaCl toolchain paths:'
    print '-------------------------'
    print '  toolchain = %s' % options.toolchain_path
    print '  toolchain bin = %s' % env['NACL_TOOL_BIN_PATH']
    print '  toolchain runtime = %s' % env['NACL_TOOL_RUNTIME_PATH']

  if options.pretty_print:
    print '\nBuild tools:'
    print '-----------'
  print template.format(option='CC', value=env['CC'])
  print template.format(option='CXX', value=env['CXX'])
  print template.format(option='AR', value=env['AR'])
  print template.format(option='LINK', value=env['LINK'])
  print template.format(option='STRIP', value=env['STRIP'])
  print template.format(option='RANLIB', value=env['RANLIB'])

  if options.pretty_print:
    print '\nBuild settings:'
    print '--------------'
  print template.format(option='CFLAGS',
                        value=FormatOptionList(option_list=env['CFLAGS']))
  print template.format(option='CXXFLAGS',
                        value=FormatOptionList(option_list=env['CXXFLAGS']))
  print template.format(option='LDFLAGS',
                        value=FormatOptionList(option_list=env['LDFLAGS']))
  print template.format(option='CPPFLAGS',
                        value=FormatOptionList(option_list=env['CPPFLAGS']))
  if options.pretty_print:
    print '\nRuntime tools:'
    print '-------------'
  print template.format(option='NACL_IRT_CORE32', value=env['NACL_IRT_CORE32'])
  print template.format(option='NACL_IRT_CORE64', value=env['NACL_IRT_CORE64'])
  print ''


def NormalizeEnv(env):
  ''' Returns a copy of env normalized.

  Internally, this script uses lists to keep track of build settings in env.
  This function converts these list to space-separated strings of items,
  suitable for use as a subprocess env.

  Argument:
    env: env map that must be normalized.

  Returns:
    A copy of env with lists converted to strings.
  '''
  norm_env = {}
  for key, value in env.iteritems():
    if isinstance(value, list):
      norm_env[key] = ' '.join(value)
    else:
      norm_env[key] = value
  return norm_env


def RunCommandOrShell(cmd_list, env):
  ''' Run the command in cmd_list or a shell if cmd_list is empty.

  Run the command in cmd_list using a normalized copy of env. For instance,
  cmd_list might contain the items ['make', 'application'], which would
  cause command 'make application' to run in the current directory. If cmd_list
  is empty, this function will spawn a new sbushell instead.

  Args:
    cmd_list: the command list to run.
    env: the environment to use.
  '''
  # If cmd_list is empty, set it up to spawn a shell instead. If cmd_list
  # isn't empty, it will run in the current shell (for security and so that the
  # user can see the output).
  new_shell = False
  if cmd_list:
    # Normalize cmd_list by building a list of individual arguments.
    new_cmd_list = []
    for item in cmd_list:
      new_cmd_list += item.split()
    cmd_list = new_cmd_list
  else:
    # Build a shell command.
    new_shell = True
    if sys.platform == 'win32':
      cmd_list = ['cmd']
    else:
      cmd_list = ['/bin/bash', '-s']
  return subprocess.call(cmd_list, env=NormalizeEnv(env), shell=new_shell)


def GenerateBuildSettings(options, args):
  ''' Generate the build settings and dump them or invoke a command.

  Given the cmd-line options and remaining cmd-line arguments, generate the
  required build settings and either dump them to stdout or invoke a shell
  command.

  Args:
    options: cmd-line options.
    args: unconsumed cmd-line arguments.

  Returns:
    0 in case of success or a command result code otherwise.
  '''
  # A few generated options, which we store in options for convenience.
  options.host = __PLATFORM_TO_HOST_MAP[sys.platform]
  options.sdk_root = GetNaclSdkRoot()
  options.toolchain_path = GetToolchainPath(options)

  env = SetupX86Env(options)
  if options.dump:
    dump(options, env, template=options.format_template)
    return 0
  else:
    return RunCommandOrShell(args, env)


def main(argv):
  ''' Do main stuff, mainly.
  '''
  parser = optparse.OptionParser(usage=__GLOBAL_HELP)
  parser.add_option(
      '-a', '--arch', dest='arch',
      choices=['x86-32', 'x86-64'],
      default='x86-64',
      help='The target architecture; one of x86-32 or x86-64. '
           '[default = %default.]')
  parser.add_option(
      '-A', '--no_ppapi', dest='no_ppapi',
      default=False,
      action='store_true',
      help='Do not add -lppapi to the link settings.')
  parser.add_option(
      '-d', '--dump', dest='dump',
      default=False,
      action='store_true',
      help='Dump the build settings to stdout')
  parser.add_option(
      '-D', '--pretty_print', dest='pretty_print',
      default=False,
      action='store_true',
      help='Print section headers when dumping to stdout')
  parser.add_option(
      '-f', '--format_template', dest='format_template',
      default='  {option}={value}',
      help="The formatting template used to output (option, value) pairs."
           "[default='%default.']")
  parser.add_option(
      '-n', '--no_merge', dest='merge',
      default=True,
      action='store_false',
      help='Do not merge the build options with current environment. By default'
           ' %prog merges the build flags with the current environment vars.'
           ' This option turns that off.')
  parser.add_option(
      '-p', '--platform', dest='sdk_platform',
      default=__DEFAULT_SDK_PLATFORM,
      help='The SDK platform to use; e.g. pepper_16. [default = %default.]')
  parser.add_option(
      '-t', '--build_type', dest='build_type',
      choices=__BUILD_TYPES.keys(),
      default='debug',
      help='The desired build type; one of debug or release.'
           ' [default = %default.]')
  parser.add_option(
      '-v', '--variant', dest='lib_variant',
      choices=['glibc', 'newlib'], default='newlib',
      help='The lib variant to use; one of glibc or newlib. '
           '[default = %default.]')

  (options, args) = parser.parse_args(argv)

  # Verify that we're running on a supported host.
  if sys.platform not in __PLATFORM_TO_HOST_MAP:
    sys.stderr.write('Platform %s is not supported.' % sys.platform)
    return 1

  return GenerateBuildSettings(options, args)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
