# -*- python -*-
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import glob
import os
import stat
import sys
sys.path.append("./common")

# NOTE: Underlay for  src/third_party_mod/gtest
# TODO: try to eliminate this hack
Dir('src/third_party_mod/gtest').addRepository(
    Dir('#/../testing/gtest'))

# ----------------------------------------------------------
environment_list = []

# ----------------------------------------------------------
# Base environment for both nacl and non-nacl variants.
pre_base_env = Environment(
    tools = ['component_setup'],
    # SOURCE_ROOT is one leave above the native_client directory.
    SOURCE_ROOT = Dir('#/..').abspath,
    # Publish dlls as final products (to staging).
    COMPONENT_LIBRARY_PUBLISH = True,

    # Use workaround in special scons version.
    LIBS_STRICT = True,
    LIBS_DO_SUBST = True,
)

# ----------------------------------------------------------
# Generic Test Wrapper
# all test suites know so far
TEST_SUITES = {'all_tests': None}

def AddNodeToTestSuite(env, node, suite_name, node_name=None):
  if not node:
    return
  AlwaysBuild(node)
  env.Alias('all_tests', node)
  for s in suite_name:
    if s not in TEST_SUITES:
      TEST_SUITES[s] = None
    env.Alias(s, node)
  if node_name:
    env.ComponentTestOutput(node_name, node)

pre_base_env.AddMethod(AddNodeToTestSuite)

# ----------------------------------------------------------
# Convenient testing aliases
# NOTE: work around for scons non-determinism in the following two lines
Alias('small_tests', [])
Alias('medium_tests', [])

Alias('unit_tests', 'small_tests')
Alias('smoke_tests', ['small_tests', 'medium_tests'])

# ----------------------------------------------------------
def Banner(text):
  print '=' * 70
  print text
  print '=' * 70

pre_base_env.AddMethod(Banner)

# ----------------------------------------------------------
# PLUGIN PREREQUISITES
# ----------------------------------------------------------

PREREQUISITES = pre_base_env.Alias('plugin_prerequisites', [])


def GetPluginPrerequsites():
  # the first source is the banner. drop it
  return PREREQUISITES[0].sources[1:]

# this is a horrible hack printing all the dependencies
# dynamically accumulated for PREREQUISITES via AddPluginPrerequisite
def PluginPrerequisiteInfo(target, source, env):
  Banner("Pluging Prerequisites")
  deps =  [dep.abspath for dep in GetPluginPrerequsites()]
  print "abbreviated list: ", str([os.path.basename(d) for d in deps])
  print "full list:"
  for dep in deps:
    print dep
  return None

banner = pre_base_env.Command('plugin_prerequisites_banner', [],
                              PluginPrerequisiteInfo)

pre_base_env.Alias('plugin_prerequisites', banner)

def AddPluginPrerequisite(env, nodes):
  env.Alias('plugin_prerequisites', nodes)
  return n

pre_base_env.AddMethod(AddPluginPrerequisite)


# NOTE: PROGRAMFILES is only used for windows
#       SILENT is used to turn of user ack
INSTALL_COMMAND = ['${PYTHON}',
                   '${SCONSTRUCT_DIR}/tools/firefoxinstall.py',
                   'SILENT="%s"' % ARGUMENTS.get('SILENT', ''),
                   '"PROGRAMFILES=%s"' % os.getenv('PROGRAMFILES', ''),
                   ]

n = pre_base_env.Alias(target='firefox_install_restore',
                       source=[],
                       action=' '.join(INSTALL_COMMAND + ['MODE=RESTORE']))
AlwaysBuild(n)


n = pre_base_env.Alias(target='firefox_install_backup',
                       source=[],
                       action=' '.join(INSTALL_COMMAND + ['MODE=BACKUP']))
AlwaysBuild(n)


def InstallPlugin(target, source, env):
  Banner('Pluging Installation')
  deps =  [dep.abspath for dep in GetPluginPrerequsites()]
  command = env.subst(' '.join(INSTALL_COMMAND + ['MODE=INSTALL'] + deps))
  return os.system(command)

# In prebuild mode we ignore the dependencies so that stuff does
# NOT get build again
# Optionally ignore the build process.
DeclareBit('prebuilt', 'Disable all build steps, only support install steps')
pre_base_env.SetBitFromOption('prebuilt', False)


if pre_base_env.Bit('prebuilt'):
  n = pre_base_env.Command('firefox_install_command',
                           [],
                           InstallPlugin)
else:
  n = pre_base_env.Command('firefox_install_command',
                           PREREQUISITES,
                           InstallPlugin)

n = pre_base_env.Alias('firefox_install', n)
AlwaysBuild(n)



# ----------------------------------------------------------
# Define the build and target platforms, and use them to define the path
# for the scons-out directory (aka TARGET_ROOT)
#.
# We have "build" and "target" platforms for the non nacl environments
# which govern service runtime, validator, etc.
#
# "build" means the  platform the code is running on
# "target" means the platform the validatir is checking for.
# Typically they are the same but testing it useful to have flexibility.
#
# Various variables in the scons environment are related to this, e.g.
#
# BUILD_ARCH: (arm, x86)
# BUILD_SUBARCH: (32, 64)
#
# The settings can be controlled using scons command line variables:
#
#
# buildplatform=: controls the build platform
# targetplatform=: controls the target platform
# platform=: controls both
#
# This dictionary is used to translate from a platform name to a
# (arch, subarch) pair
AVAILABLE_PLATFORMS = {
    'x86-32' : { 'arch' : 'x86' , 'subarch' : '32' },
    'x86-64' : { 'arch' : 'x86' , 'subarch' : '64' },
    'arm'    : { 'arch' : 'arm' , 'subarch' : '32' }
    }

# Look up the platform name from the command line arguments,
# defaulting to 'platform' if needed.
def GetPlatform(name):
  platform = ARGUMENTS.get(name)
  if platform is None:
    return ARGUMENTS.get('platform', 'x86-32')
  elif ARGUMENTS.get('platform') is None:
    return platform
  else:
    Banner("Can't specify both %s and %s on the command line"
           % ('platform', name))
    assert 0

# Decode platform into list [ ARCHITECTURE , EXEC_MODE ].
def DecodePlatform(platform):
  if platform in AVAILABLE_PLATFORMS:
    return AVAILABLE_PLATFORMS[platform]
  Banner("Unrecognized platform: %s" % ( platform ))
  assert 0

BUILD_NAME = GetPlatform('buildplatform')
pre_base_env.Replace(BUILD_ARCHITECTURE = DecodePlatform(BUILD_NAME)['arch'])
pre_base_env.Replace(BUILD_SUBARCH = DecodePlatform(BUILD_NAME)['subarch'])

TARGET_NAME = GetPlatform('targetplatform')
pre_base_env.Replace(TARGET_ARCHITECTURE = DecodePlatform(TARGET_NAME)['arch'])
pre_base_env.Replace(TARGET_SUBARCH = DecodePlatform(TARGET_NAME)['subarch'])

# Determine where the object files go
if BUILD_NAME == TARGET_NAME:
  TARGET_ROOT = '${DESTINATION_ROOT}/${BUILD_TYPE}-%s' % TARGET_NAME
else:
  TARGET_ROOT = '${DESTINATION_ROOT}/${BUILD_TYPE}-%s-to-%s' % (BUILD_NAME,
                                                                TARGET_NAME)
pre_base_env.Replace(TARGET_ROOT = TARGET_ROOT)

# ----------------------------------------------------------
EXTRA_ENV = [('XAUTHORITY', None),
              ('HOME', None),
              ('DISPLAY', None),
              ('SSH_TTY', None),
              ('KRB5CCNAME', None),
              ]

def DemoSelLdrNacl(env,
                   target,
                   nexe,
                   log_verbosity=2,
                   args=[]):

  # NOTE: that the variable TRUSTED_ENV is set by ExportSpecialFamilyVars()
  if 'TRUSTED_ENV' not in env:
    print ('WARNING: no trusted env specified skipping demo %s\n' % target)
    return []

  trusted_env = env['TRUSTED_ENV']
  sel_ldr = trusted_env.File('${STAGING_DIR}/'
                             + '${PROGPREFIX}sel_ldr${PROGSUFFIX}')
  deps = [sel_ldr, nexe]
  command = ['${SOURCES[0].abspath}', '-d', '-f',
             '${SOURCES[1].abspath}', '--'] + args

  # NOT: since most of the demos use X11 we need to make sure
  #      some env vars are set for tag, val in extra_env:
  for tag, val in EXTRA_ENV:
    if val is None:
      if os.getenv(tag) is not None:
        env['ENV'][tag] = os.getenv(tag)
      else:
        env['ENV'][tag] =  env.subst(val)

  node = env.Command(target, deps, ' '.join(command))
  return node

if pre_base_env['TARGET_ARCHITECTURE'] == 'x86':
  # arm support would likely require some emulation magic
  pre_base_env.AddMethod(DemoSelLdrNacl)

# ----------------------------------------------------------
def CommandSelLdrTestNacl(env, name, command,
                          log_verbosity=2,
                          size='medium',
                          **extra):

  # NOTE: that the variable TRUSTED_ENV is set by ExportSpecialFamilyVars()
  if 'TRUSTED_ENV' not in env:
    print 'WARNING: no trusted env specified skipping test %s' % name
    return []

  trusted_env = env['TRUSTED_ENV']
  sel_ldr = trusted_env.File('${STAGING_DIR}/'
                             + '${PROGPREFIX}sel_ldr${PROGSUFFIX}')
  command = [sel_ldr, '-d', '-f'] + command

  # NOTE(robertm): log handling is a little magical
  # We do not pass these via flags because those are not usable for sel_ldr
  # when testing via plugin, esp windows.
  if 'log_golden' in extra:
    logout = '${TARGET}.log'
    extra['logout'] = logout
    extra['osenv'] = 'NACLLOG=%s,NACLVERBOSITY=%d' % (logout, log_verbosity)

  return CommandTestAgainstGoldenOutput(env, name, command, size, **extra)

pre_base_env.AddMethod(CommandSelLdrTestNacl)

# ----------------------------------------------------------
TEST_EXTRA_ARGS = ['stdin', 'logout',
                   'stdout_golden', 'stderr_golden', 'log_golden',
                   'stdout_filter', 'stderr_filter', 'log_filter',
                   'osenv', 'exit_status']

TEST_TIME_THRESHOLD = {
    'small':   2,
    'medium': 10,
    'large':  60,
    'huge': 1800,
    }

TEST_SCRIPT = '${SCONSTRUCT_DIR}/tools/command_tester.py'

def CommandTestAgainstGoldenOutput(env, name, command, size='small', **extra):
  if not  name.endswith('.out') or name.startswith('$'):
    print "ERROR: bad  test filename for test output ", name
    assert 0

  name = '${TARGET_ROOT}/test_results/' + name
  # NOTE: using the long version of 'name' helps distinguish opt vs dbg
  max_time = TEST_TIME_THRESHOLD[size]
  script_flags = ['--name', name,
                  '--report', name,
                  '--time_warning', str(max_time),
                  '--time_error', str(10 * max_time),
                  ]

  deps = [TEST_SCRIPT]

  # extract deps from command and rewrite
  for n, c in enumerate(command):
    if type(c) != str:
      deps.append(c)
      command[n] = '${SOURCES[%d].abspath}' % (len(deps) - 1)

  # extract deps from flags and rewrite
  for e in extra:
    assert e in TEST_EXTRA_ARGS
    if type(extra[e]) != str:
      deps.append(extra[e])
      extra[e] = '${SOURCES[%d].abspath}' % (len(deps) - 1)
    script_flags.append('--' + e)
    script_flags.append(extra[e])

  # NOTE: "SOURCES[X]" references the scons object in deps[x]
  command = ['${PYTHON}',
             '${SOURCES[0].abspath}',
             ' '.join(script_flags),
             ' '.join(command),
             ]

  return env.Command(name, deps, ' '.join(command))

pre_base_env.AddMethod(CommandTestAgainstGoldenOutput)

# ----------------------------------------------------------
if ARGUMENTS.get('pp', 0):
  def CommandPrettyPrinter(cmd, targets, source, env):
    prefix = env.subst('$SOURCE_ROOT') + '/'
    target =  targets[0]
    cmd_tokens = cmd.split()
    if "python" in cmd_tokens[0]:
      cmd_name = cmd_tokens[1]
    else:
      cmd_name = cmd_tokens[0].split('(')[0]
    if cmd_name.startswith(prefix):
      cmd_name = cmd_name[len(prefix):]
    env_name = env.subst('${BUILD_TYPE}${BUILD_SUBTYPE}')
    print '[%s] [%s] %s' % (cmd_name, env_name, target.get_path())
  pre_base_env.Append(PRINT_CMD_LINE_FUNC = CommandPrettyPrinter)

# ----------------------------------------------------------
base_env = pre_base_env.Clone()
base_env.Append(
  BUILD_SUBTYPE = '',
  BUILD_SCONSCRIPTS = [
        'src/trusted/service_runtime/build.scons',
        'src/shared/imc/build.scons',
        'src/shared/npruntime/build.scons',
        'src/shared/srpc/build.scons',
        'src/shared/utils/build.scons',
        'src/third_party_mod/gtest/build.scons',
        'src/trusted/desc/build.scons',
        'src/trusted/nonnacl_util/build.scons',
        'src/trusted/platform_qualify/build.scons',
        'src/trusted/plugin/build.scons',
        'src/trusted/sandbox/build.scons',
        'src/trusted/platform/build.scons',
        'tests/python_version/build.scons',
        'tests/tools/build.scons',
    ],
    CPPDEFINES = [
        ['NACL_BLOCK_SHIFT', '5'],
        ['NACL_BLOCK_SIZE', '32'],
        ['NACL_BUILD_ARCH', '${BUILD_ARCHITECTURE}' ],
        ['NACL_BUILD_SUBARC', '${BUILD_SUBARCH}' ],
        ['NACL_TARGET_ARCH', '${TARGET_ARCHITECTURE}' ],
        ['NACL_TARGET_SUBARCH', '${TARGET_SUBARCH}' ],
    ],
    CPPPATH = ['${SOURCE_ROOT}'],

    EXTRA_CFLAGS = [],
    EXTRA_CCFLAGS = [],
    EXTRA_CXXFLAGS = [],
    EXTRA_LIBS = [],
    CFLAGS = ['${EXTRA_CFLAGS}'],
    CCFLAGS = ['${EXTRA_CCFLAGS}'],
    CXXFLAGS = ['${EXTRA_CXXFLAGS}'],
    LIBS = ['${EXTRA_LIBS}'],
)


if base_env['TARGET_ARCHITECTURE'] == 'arm':
  base_env.Append(
      BUILD_SCONSCRIPTS = ['src/trusted/validator_arm/build.scons',]
      )

elif base_env['TARGET_ARCHITECTURE'] == 'x86':
  base_env.Append(
      BUILD_SCONSCRIPTS = ['src/trusted/validator_x86/build.scons',]
      )
else:
  Banner("unknown TARGET_ARCHITECTURE %s" % base_env['TARGET_ARCHITECTURE'])


base_env.Replace(
    NACL_BUILD_FAMILY = 'TRUSTED',

    SDL_HERMETIC_LINUX_DIR='${MAIN_DIR}/../third_party/sdl/linux/v1_2_13',
    SDL_HERMETIC_MAC_DIR='${MAIN_DIR}/../third_party/sdl/osx/v1_2_13',
    SDL_HERMETIC_WINDOWS_DIR='${MAIN_DIR}/../third_party/sdl/win/v1_2_13',
)

# Add optional scons files if present in the directory tree.
if os.path.exists(pre_base_env.subst('${MAIN_DIR}/supplement/build.scons')):
  base_env.Append(BUILD_SCONSCRIPTS=['${MAIN_DIR}/supplement/build.scons'])

# Select tests to run under coverage build.
base_env['COVERAGE_TARGETS'] = ['small_tests', 'medium_tests']

# Add the sdk root path (without setting up the tools).
base_env['NACL_SDK_ROOT_ONLY'] = True
base_env.Tool('naclsdk')


base_env.Help("""\
======================================================================
Help for NaCl
======================================================================

Common tasks:
-------------

* cleaning:           scons -c
* building:           scons
* just the doc:       scons --mode=doc
* build mandel:       scons --model=nacl mandel.nexe
* smoke test:         scons --mode=nacl,opt-linux -k pp=1 smoke_tests
* build the plugin:   scons --mode=opt-linux npGoogleNaClPlugin
* another way to build the plugin: scons --mode=opt-linux src/trusted/plugin
* install the plugin: scons --verbose firefox_install
* sel_ldr:            scons --mode=opt-linux sel_ldr
* install pre-built plugin: scons firefox_install --prebuilt

Options:
--------
pp=1              use command line pretty printing (more concise output)
sdl=<mode>        where <mode>:
                  'none': don't use SDL (default)
                  'local': use locally installed SDL
                  'hermetic': use the hermetic SDL copy
naclsdk_mode=<mode>   where <mode>:
                      'local': use locally installed sdk kit
                      'download': use the download copy (default)
                      'custom:<path>': use kit at <path>

--prebuilt        Do not build things, just do install steps

sysinfo=          Suppress verbose system info printing

Automagically generated help:
-----------------------------
""")

# ---------------------------------------------------------
def GenerateOptimizationLevels(env):
  # Generate debug variant.
  debug_env = env.Clone(tools = ['target_debug'])
  debug_env['OPTIMIZATION_LEVEL'] = 'dbg'
  debug_env['BUILD_TYPE'] = debug_env.subst('$BUILD_TYPE')
  debug_env['BUILD_DESCRIPTION'] = debug_env.subst('$BUILD_DESCRIPTION')
  # Add to the list of fully described environments.
  environment_list.append(debug_env)

  # Generate opt variant.
  opt_env = env.Clone(tools = ['target_optimized'])
  opt_env['OPTIMIZATION_LEVEL'] = 'opt'
  opt_env['BUILD_TYPE'] = opt_env.subst('$BUILD_TYPE')
  opt_env['BUILD_DESCRIPTION'] = opt_env.subst('$BUILD_DESCRIPTION')
  # Add opt to the default group.
  opt_env.Append(BUILD_GROUPS = ['default'])
  # Add to the list of fully described environments.
  environment_list.append(opt_env)

  return (debug_env, opt_env)


# ----------------------------------------------------------
windows_env = base_env.Clone(
    BUILD_TYPE = '${OPTIMIZATION_LEVEL}-win',
    BUILD_TYPE_DESCRIPTION = 'Windows ${OPTIMIZATION_LEVEL} build',
    tools = ['target_platform_windows'],
    # Windows /SAFESEH linking requires either an .sxdata section be present or
    # that @feat.00 be defined as a local, abolute symbol with an odd value.
    ASCOM = '$ASPPCOM /E | as -defsym @feat.00=1 -o $TARGET',
    PDB = '${TARGET.base}.pdb',
    # Strict doesnt't currently work for windows since some of the system
    # libraries like wsock32 are magical.
    LIBS_STRICT = False,
)

windows_env.Append(
    CPPDEFINES = [
        ['NACL_WINDOWS', '1'],
        ['NACL_OSX', '0'],
        ['NACL_LINUX', '0'],
        ['_WIN32_WINNT', '0x0501'],
        ['__STDC_LIMIT_MACROS', '1'],
    ],
    LIBS = ['wsock32', 'advapi32'],
    CCFLAGS = ['/EHsc', '/WX'],
)

# This linker option allows us to ensure our builds are compatible with
# Chromium, which uses it.
windows_env.Append(LINKFLAGS = "/safeseh")

windows_env['ENV']['PATH'] = os.environ.get('PATH', '[]')
windows_env['ENV']['INCLUDE'] = os.environ.get('INCLUDE', '[]')
windows_env['ENV']['LIB'] = os.environ.get('LIB', '[]')
windows_env.AppendENVPath('PATH',
                          '$SOURCE_ROOT/third_party/gnu_binutils/files')

(windows_debug_env,
 windows_optimized_env) = GenerateOptimizationLevels(windows_env)

if ARGUMENTS.get('sdl', 'hermetic') != 'none':
  # These will only apply to sdl!=none builds!
  windows_debug_env.Append(CPPDEFINES = ['_DLL', '_MT'])
  windows_optimized_env.Append(CPPDEFINES = ['_DLL', '_MT'])
  # SDL likes DLLs
  if '/MT' in windows_optimized_env['CCFLAGS']:
    windows_optimized_env.FilterOut(CCFLAGS=['/MT']);
    windows_optimized_env.Append(CCFLAGS=['/MD']);
  if '/MTd' in windows_debug_env['CCFLAGS']:
    windows_debug_env.FilterOut(CCFLAGS=['/MTd']);
    windows_debug_env.Append(CCFLAGS=['/MDd']);
    # this doesn't feel right, but fixes dbg-win
    windows_debug_env.Append(LINKFLAGS = ['/NODEFAULTLIB:msvcrt'])
  # make source level debugging a little easier
  if '/Z7' not in windows_debug_env['CCFLAGS']:
    if '/Zi' not in windows_debug_env['CCFLAGS']:
      windows_debug_env.Append(CCFLAGS=['/Z7'])

# TODO(bradnelson): This does not quite work yet (c.f.
# service_runtime/build.scons)
# move some debug info close to the binaries where the debugger can find it
# This only matter when we the "wrong" version of VisualStudio is used
n = windows_debug_env.Command('dummy', [], 'echo INSTALL WINDOWS DEBUG DATA')
windows_debug_env.Alias('windows_debug_data_install', n)

n = windows_debug_env.Replicate(
    '${STAGING_DIR}',
    '${VC80_DIR}/vc/redist/Debug_NonRedist/x86/Microsoft.VC80.DebugCRT')

windows_debug_env.Alias('windows_debug_data_install', n)

n = windows_debug_env.Replicate(
    '${STAGING_DIR}',
    '${VC80_DIR}/vc/redist/x86/Microsoft.VC80.CRT')

windows_debug_env.Alias('windows_debug_data_install', n)
# ----------------------------------------------------------

unix_like_env = base_env.Clone()
unix_like_env.Prepend(
  CFLAGS = ['-std=gnu99' ],
  CCFLAGS = [
    # '-malign-double',
    '-Werror',
    '-Wall',
    '-pedantic',
    #'-Wextra',
    #'-Wno-long-long',
    #'-Wswitch-enum',
    #'-Wsign-compare',
    '-fvisibility=hidden',
  ],
  CXXFLAGS=['-std=c++98'],
  LIBPATH=['/usr/lib'],
  LIBS = ['pthread', 'ssl', 'crypto'],
)

# ----------------------------------------------------------

mac_env = unix_like_env.Clone(
    BUILD_TYPE = '${OPTIMIZATION_LEVEL}-mac',
    BUILD_TYPE_DESCRIPTION = 'MacOS ${OPTIMIZATION_LEVEL} build',
    tools = ['target_platform_mac'],
    # TODO(bradnelson): this should really be able to live in unix_like_env
    #                   but can't due to what the target_platform_x module is
    #                   doing.
    LINK = '$CXX',
    PLUGIN_SUFFIX = '.bundle',
)

mac_env.Append(
    CCFLAGS = ['-mmacosx-version-min=10.4'],
    # TODO(bradnelson): remove UNIX_LIKE_CFLAGS when scons bug is fixed
    CPPDEFINES = [['NACL_WINDOWS', '0'],
                  ['NACL_OSX', '1'],
                  ['NACL_LINUX', '0'],
                  ['MAC_OS_X_VERSION_MIN_REQUIRED', 'MAC_OS_X_VERSION_10_4'],
                  # defining _DARWIN_C_SOURCE breaks 10.4
                  #['_DARWIN_C_SOURCE', '1'],
                  ['__STDC_LIMIT_MACROS', '1'],
                  ],
)

(mac_debug_env, mac_optimized_env) = GenerateOptimizationLevels(mac_env)

# ----------------------------------------------------------

linux_env = unix_like_env.Clone(
    BUILD_TYPE = '${OPTIMIZATION_LEVEL}-linux',
    BUILD_TYPE_DESCRIPTION = 'Linux ${OPTIMIZATION_LEVEL} build',
    tools = ['target_platform_linux'],
    # TODO(bradnelson): this should really be able to live in unix_like_env
    #                   but can't due to what the target_platform_x module is
    #                   doing.
    LINK = '$CXX',
)

# -m32 and -L/usr/lib32 are needed to do 32-bit builds on 64-bit
# user-space machines; requires ia32-libs-dev to be installed; or,
# failing that, ia32-libs and symbolic links *manually* created for
# /usr/lib32/libssl.so and /usr/lib32/libcrypto.so to the current
# /usr/lib32/lib*.so.version (tested with ia32-libs 2.2ubuntu11; no
# ia32-libs-dev was available for testing).
# Additional symlinks of this sort are needed for gtk,
# see src/trusted/nonnacl_util/build.scons.

linux_env.SetDefault(
    # NOTE: look into http://www.scons.org/wiki/DoxygenBuilder
    DOXYGEN = ARGUMENTS.get('DOXYGEN', '/usr/bin/doxygen'),
)

# Prepend so we can disable warnings via Append
linux_env.Prepend(
    ASFLAGS = ['-m32', ],
    # TODO(robertm): move these flags up to unix_like_env.
    #                currently mac issues prevent us from doing that
    CCFLAGS = ['-m32',
               # NOTE: not available with older gcc/g++ versions
               # '-fdiagnostics-show-option',
               ],
    CPPDEFINES = [['NACL_WINDOWS', '0'],
                  ['NACL_OSX', '0'],
                  ['NACL_LINUX', '1'],
                  ['_BSD_SOURCE', '1'],
                  ['_POSIX_C_SOURCE', '199506'],
                  ['_XOPEN_SOURCE', '600'],
                  ['_GNU_SOURCE', '1'],
                  ['__STDC_LIMIT_MACROS', '1'],
                  ],
    LIBS = ['rt'],
    LINKFLAGS = ['-m32', '-L/usr/lib32'],
)

if linux_env['BUILD_ARCHITECTURE'] == 'x86':
  if linux_env['BUILD_SUBARCH'] == '32':
    linux_env.Prepend(
        ASFLAGS = ['-m32', ],
        CCFLAGS = ['-m32', ],
        LINKFLAGS = ['-m32', '-L/usr/lib32'],
        )

  else:
    assert linux_env['BUILD_SUBARCH'] == '64'
    linux_env.Prepend(
        ASFLAGS = ['-m64', ],
        CCFLAGS = ['-m64', ],
        LINKFLAGS = ['-m64', '-L/usr/lib64'],
        )
elif linux_env['BUILD_ARCHITECTURE'] == 'arm':
  linux_env.Replace(CC=os.getenv('ARM_CC', 'NO-ARM-CC-SPECIFIED'),
                    CXX=os.getenv('ARM_CXX', 'NO-ARM-CXX-SPECIFIED'),
                    LD=os.getenv('ARM_LD', 'NO-ARM-LD-SPECIFIED'),
                    ASFLAGS=[],
                    LIBPATH=['${LIB_DIR}',
                             os.getenv('ARM_LIB_DIR', '').split()],
                    CCFLAGS=['-march=armv6'],
                    LINKFLAGS=ARGUMENTS.get('ARM_LINKFLAGS', ''),
                    )
  # NOTE(pmarch): eliminate the need for this
  linux_env.Append(CPPDEFINES=['NACL_ARM'])
else:
  Banner('Strange platform: %s' % BUILD_NAME)

# TODO(robert): support for arm builds

(linux_debug_env, linux_optimized_env) = GenerateOptimizationLevels(linux_env)



# ----------------------------------------------------------
# The nacl_env is used to build native_client modules
# using a special tool chain which produces platform
# independent binaries
# NOTE: this loads stuff from:
# third_party/software_construction_toolkit/files/site_scons/site_tools/naclsdk.py
# ----------------------------------------------------------
nacl_env = pre_base_env.Clone(
    tools = ['naclsdk'],
    BUILD_TYPE = 'nacl',
    BUILD_TYPE_DESCRIPTION = 'NaCl module build',
    NACL_BUILD_FAMILY = 'UNTRUSTED',
    # TODO: explain this
    LINK = '$CXX',

    EXTRA_CFLAGS = [],
    EXTRA_CCFLAGS = [],
    EXTRA_CXXFLAGS = [],
    EXTRA_LIBS = [],

    # always optimize binaries
    CCFLAGS = ['-O2',
               '-mfpmath=sse',
               '-msse',
               '-fomit-frame-pointer',
               '-Wall',
               '-fdiagnostics-show-option',
               '-pedantic',
               '-Werror',
               '${EXTRA_CCFLAGS}'],
    CPPPATH = ['$SOURCE_ROOT'],
    CFLAGS = ['${EXTRA_CFLAGS}'],
    CXXFLAGS = ['${EXTRA_CXXFLAGS}'],
    LIBS = ['${EXTRA_LIBS}'],

    # from third_party/software_construction_toolkit/files/site_scons/site_tools/naclsdk.py
    LIBPATH = ['$NACL_SDK_LIB'],
)

Banner('Building nexe binaries using sdk at [%s]' %
       nacl_env.subst('$NACL_SDK_ROOT'))


nacl_env.Append(
    BUILD_SCONSCRIPTS = [
      ####  ALPHABETICALLY SORTED ####
      'common/console/nacl.scons',

      'tests/app_lib/nacl.scons',
      'tests/autoloader/nacl.scons',
      'tests/contest_issues/nacl.scons',
      'tests/earth/nacl.scons',
      'tests/fib/nacl.scons',
      'tests/file/nacl.scons',
      'tests/hello_world/nacl.scons',
      'tests/imc_shm_mmap/nacl.scons',
      'tests/life/nacl.scons',
      'tests/mandel/nacl.scons',
      'tests/mandel_nav/nacl.scons',
      'tests/many/nacl.scons',
      'tests/mmap/nacl.scons',
      'tests/native_worker/nacl.scons',
      'tests/noop/nacl.scons',
      'tests/nrd_xfer/nacl.scons',
      'tests/null/nacl.scons',
      'tests/nullptr/nacl.scons',
      'tests/selenium_dummy/nacl.scons',
      'tests/srpc/nacl.scons',
      'tests/srpc_hw/nacl.scons',
      'tests/syscalls/nacl.scons',
      'tests/threads/nacl.scons',
      'tests/tone/nacl.scons',
      'tests/vim/nacl.scons',
      'tests/voronoi/nacl.scons',

      'tools/tests/nacl.scons',
      ####  ALPHABETICALLY SORTED ####
   ],
)

environment_list.append(nacl_env)
# ----------------------------------------------------------
# We force this into a separate env so that the tests in nacl_env
# have NO access to any libraries build here but need to link them
# from the sdk libdir
# ----------------------------------------------------------
nacl_extra_sdk_env = pre_base_env.Clone(
    tools = ['naclsdk'],
    BUILD_TYPE = 'nacl_extra_sdk',
    BUILD_TYPE_DESCRIPTION = 'NaCl SDK extra library build',
    NACL_BUILD_FAMILY = 'UNTRUSTED',
    CFLAGS = ['-std=gnu99'],
    # TODO: explain this
    CCFLAGS = ['-O3',
               '-Werror',
               '-Wall',
               '-Wswitch-enum',
               '-g',
               '-fno-builtin',
               '-fno-stack-protector',
               '-DNACL_BLOCK_SHIFT=5',
               '-fdiagnostics-show-option',
               '-pedantic',
               ],
    CPPPATH = ['$SOURCE_ROOT'],
    LINK = '$CXX',
)


nacl_extra_sdk_env.Append(
    BUILD_SCONSCRIPTS = [
      ####  ALPHABETICALLY SORTED ####
      'src/include/nacl/nacl.scons',
      'src/shared/imc/nacl.scons',
      'src/shared/npruntime/nacl.scons',
      'src/shared/srpc/nacl.scons',
      'src/untrusted/av/nacl.scons',
      'src/untrusted/nacl/nacl.scons',
      'src/untrusted/pthread/nacl.scons',
      'src/untrusted/stubs/nacl.scons',
      'src/untrusted/unimpl/nacl.scons',
      ####  ALPHABETICALLY SORTED ####
   ],
)

environment_list.append(nacl_extra_sdk_env)

# ----------------------------------------------------------
# Targets for updating sdk headers and libraries
# NACL_SDK_ROOT is defined by
# third_party/software_construction_toolkit/files/site_scons/site_tools/naclsdk.py
# NOTE: our task here is complicated by the fact that there might already
#       some (outdated) headers/libraries at the new location
#       One of the hacks we employ here is to make every library depend
#       on the installation on ALL headers (sdk_headers)

# Contains all the headers to be installed
sdk_headers = nacl_extra_sdk_env.Alias('extra_sdk_update_header', [])
# Contains all the libraries and .o files to be installed
nacl_extra_sdk_env.Alias('extra_sdk_update', [])


def AddHeaderToSdk(env, nodes, path=None):
  if path is None:
    path = 'nacl/include/nacl/'

  n = env.Replicate('${NACL_SDK_ROOT}/' + path , nodes)
  env.Alias('extra_sdk_update_header', n)
  return n

nacl_extra_sdk_env.AddMethod(AddHeaderToSdk)

def AddLibraryToSdk(env, nodes, path=None):
  # NOTE: hack see comment
  nacl_extra_sdk_env.Requires(nodes, sdk_headers)
  if path is None:
    path = 'nacl/lib'

  n = env.ReplicatePublished('${NACL_SDK_ROOT}/' + path, nodes, 'link')
  env.Alias('extra_sdk_update', n)
  return n

nacl_extra_sdk_env.AddMethod(AddLibraryToSdk)

def AddObjectToSdk(env, nodes, path=None):
  # NOTE: hack see comment
  nacl_extra_sdk_env.Requires(nodes, sdk_headers)
  if path is None:
    path = 'nacl/lib'

  n = env.Replicate('${NACL_SDK_ROOT}/' + path, nodes)
  env.Alias('extra_sdk_update', n)
  return n

nacl_extra_sdk_env.AddMethod(AddObjectToSdk)

# NOTE: a helpful target to test the sdk_extra magic
#       combine this with a 'MODE=nacl -c'
# TODO: find a way to cleanup the libs on top of the headers
nacl_extra_sdk_env.Command('extra_sdk_clean', [],
                           ['rm -rf ${NACL_SDK_ROOT}/nacl/include/nacl*'])

# ----------------------------------------------------------
# CODE COVERAGE
# ----------------------------------------------------------
windows_coverage_env = windows_env.Clone(
    tools = ['code_coverage'],
    BUILD_TYPE = 'coverage-win',
    BUILD_TYPE_DESCRIPTION = 'Windows code coverage build',
    # TODO(bradnelson): switch nacl to common testing process so this won't be
    #    needed.
    MANIFEST_FILE = None,
)
# TODO(bradnelson): switch nacl to common testing process so this won't be
#    needed.
windows_coverage_env['LINKCOM'] = windows_coverage_env.Action([
    windows_coverage_env.get('LINKCOM', []),
    '$COVERAGE_VSINSTR /COVERAGE ${TARGET}'])
windows_coverage_env.FilterOut(BUILD_GROUPS = ['default'])
windows_coverage_env.Append(LINKFLAGS = ['/NODEFAULTLIB:msvcrt'])
environment_list.append(windows_coverage_env)

# TODO(bradnelson): this test current doesn't interact well with coverage.
#   This needs to be fixed.
windows_coverage_env.FilterOut(
    COVERAGE_TARGETS = ['small_tests', 'medium_tests'],
)

mac_coverage_env = mac_env.Clone(
    tools = ['code_coverage'],
    BUILD_TYPE = 'coverage-mac',
    BUILD_TYPE_DESCRIPTION = 'MacOS code coverage build',
    # Strict doesnt't currently work for coverage because the path to gcov is
    # magically baked into the compiler.
    LIBS_STRICT = False,
)
mac_coverage_env.FilterOut(BUILD_GROUPS = ['default'])
environment_list.append(mac_coverage_env)

linux_coverage_env = linux_debug_env.Clone(
    tools = ['code_coverage'],
    BUILD_TYPE = 'coverage-linux',
    BUILD_TYPE_DESCRIPTION = 'Linux code coverage build',
    # Strict doesnt't currently work for coverage because the path to gcov is
    # magically baked into the compiler.
    LIBS_STRICT = False,
)
linux_coverage_env.FilterOut(BUILD_GROUPS = ['default'])
linux_coverage_env['OPTIONAL_COVERAGE_LIBS'] = '$COVERAGE_LIBS'
environment_list.append(linux_coverage_env)

# ----------------------------------------------------------
# VARIOUS HELPERS
# ----------------------------------------------------------

doc_env = pre_base_env.Clone(
  NACL_BUILD_FAMILY = 'NO_PLATFORM',
  BUILD_TYPE = 'doc',
  BUILD_TYPE_DESCRIPTION = 'Documentation build',
  HOST_PLATFORMS = '*',
  HOST_PLATFORM_SUFFIX='',
)
environment_list.append(doc_env)
doc_env.Append(
  BUILD_SCONSCRIPTS = [
    'documentation',
    ],
)

# ----------------------------------------------------------
# Environment Massaging
# ----------------------------------------------------------
RELEVANT_CONFIG = ['NACL_BUILD_FAMILY',
                   'BUILD_TYPE',
                   'TARGET_ROOT',
                   'OBJ_ROOT',
                   'BUILD_TYPE_DESCRIPTION',
                   ]

MAYBE_RELEVANT_CONFIG = ['BUILD_OS',
                         'BUILD_ARCHITECTURE',
                         'BUILD_SUBARCH',
                         'TARGET_OS',
                         'TARGET_ARCHITECTURE',
                         'TARGET_SUBARCH',
                         ]

def DumpCompilerVersion(cc, env):
  if cc.startswith('gcc'):
    os.system(env.subst('${CC} --v'))
  elif cc.startswith('cl'):
    import subprocess
    p = subprocess.Popen(env.subst('${CC} /V'),
                         bufsize=1000*1000,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    stderr = p.stderr.read()
    stdout = p.stdout.read()
    retcode = p.wait()
    print stderr[0:stderr.find("\r")]
  elif cc.startswith('nacl-gcc'):
    os.system(env.subst('${NACL_SDK_ROOT}/bin/${CC} --v'))
  else:
    print "UNKNOWN COMPILER"


def SanityCheckAndMapExtraction(all_envs, selected_envs):
  # simple completeness check
  for env in all_envs:
    for tag in RELEVANT_CONFIG:
      assert tag in   env
      assert env[tag]

  # collect build families and enforce now dup rules
  family_map = {}
  for env in selected_envs:
    family = env['NACL_BUILD_FAMILY']
    if family not in family_map:
      family_map[family] = env
    else:
      print '\n\n\n\n'
      print 'You are using incompatible environments simultaneously'
      print
      print '%s vs %s' % (env['BUILD_TYPE'],
                         family_map[family]['BUILD_TYPE'])
      print """
      Please specfy the exact environments you require, e.g.
      MODE=dbg-linux,nacl

      """
      assert 0

  # sytem info
  if ARGUMENTS.get('sysinfo', 1):
    os.system(pre_base_env.subst('${PYTHON} tools/sysinfo.py'))

  Banner("The following environments have been configured")
  for family in family_map:
    env = family_map[family]
    for tag in RELEVANT_CONFIG:
      print "%s:  %s" % (tag, env.subst(env[tag]))
    for tag in MAYBE_RELEVANT_CONFIG:
      if tag in env:
        print "%s:  %s" % (tag, env.subst(env[tag]))
    cc = env.subst('${CC}')
    print 'CC:', cc
    if ARGUMENTS.get('sysinfo', 1) and not env.Bit('prebuilt'):
      DumpCompilerVersion(cc, env)
    print

  if 'TRUSTED' not in family_map or 'UNTRUSTED' not in family_map:
    Banner('W A R N I N G:')
    print "Not all families have envs this may cause some tests to not run"
  return family_map

def ExportSpecialFamilyVars(selected_envs, family_map):
    for family in family_map:
      family_env = family_map[family]
      for env in selected_envs:
        env[family + '_ENV'] = family_env

# ----------------------------------------------------------
# Blank out defaults.
Default(None)


# Apply optional supplement if present in the directory tree.
if os.path.exists(pre_base_env.subst('$MAIN_DIR/supplement/supplement.scons')):
  SConscript('supplement/supplement.scons',
      exports=['environment_list', 'linux_env'])

selected_envs = FilterEnvironments(environment_list)
family_map = SanityCheckAndMapExtraction(environment_list, selected_envs)
ExportSpecialFamilyVars(selected_envs, family_map)

BuildEnvironments(environment_list)

# Change default to build everything, but not run tests.
Default(['all_programs', 'all_bundles', 'all_test_programs', 'all_libraries'])

# ----------------------------------------------------------
# Generate a solution, defer to the end.
solution_env = base_env.Clone(tools = ['visual_studio_solution'])
solution = solution_env.Solution(
    'nacl', environment_list,
    exclude_pattern = '.*third_party.*',
    extra_build_targets = {
        'Firefox': 'c:/Program Files/Mozilla FireFox/firefox.exe',
    },
)
solution_env.Alias('solution', solution)
