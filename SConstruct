# -*- python -*-
# Copyright 2008 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import glob
import os
import stat
import sys
sys.path.append("./common")
import sets
from SCons.Errors import UserError


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
# Method to make sure -pedantic, etc, are not stripped from the
# default env, since occasionally an engineer will be tempted down the
# dark -- but wide and well-trodden -- path of expediency and stray
# from the path of correctness.

def EnsureRequiredBuildWarnings(env):
  if env.Bit('linux') or env.Bit('mac'):
    required_env_flags = sets.Set(['-pedantic', '-Wall', '-Werror' ])
    ccflags = sets.Set(env.get('CCFLAGS'))

    if not required_env_flags.issubset(ccflags):
      raise UserError('required build flags missing: '
                      + ' '.join(required_env_flags.difference(ccflags)))
  else:
    # windows get a pass for now
    pass

pre_base_env.AddMethod(EnsureRequiredBuildWarnings)


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
                   sel_ldr_flags=['-d'],
                   args=[]):

  # NOTE: that the variable TRUSTED_ENV is set by ExportSpecialFamilyVars()
  if 'TRUSTED_ENV' not in env:
    print ('WARNING: no trusted env specified skipping demo %s\n' % target)
    return []

  trusted_env = env['TRUSTED_ENV']
  sel_ldr = trusted_env.File('${STAGING_DIR}/'
                             + '${PROGPREFIX}sel_ldr${PROGSUFFIX}')
  deps = [sel_ldr, nexe]
  command = (['${SOURCES[0].abspath}'] + sel_ldr_flags +
             ['-f', '${SOURCES[1].abspath}', '--'] + args)

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
                          sel_ldr_flags=None,
                          loader='sel_ldr',
                          size='medium',
                          **extra):
  if env['BUILD_SUBARCH'] == '64':
    return []

  # NOTE: that the variable TRUSTED_ENV is set by ExportSpecialFamilyVars()
  if 'TRUSTED_ENV' not in env:
    print 'WARNING: no trusted env specified skipping test %s' % name
    return []

  if sel_ldr_flags is None:
    sel_ldr_flags = []

  trusted_env = env['TRUSTED_ENV']
  sel_ldr = trusted_env.File('${STAGING_DIR}/${PROGPREFIX}%s${PROGSUFFIX}' %
                             loader)

  # Temporarily: ignore ABI mismatch on ARM.
  if env['BUILD_ARCHITECTURE'] == 'arm':
    sel_ldr_flags = sel_ldr_flags + ['-I', '-d']
    # TODO(robertm): get rid of -d when arm tests stabilize

  command = [sel_ldr] + sel_ldr_flags  + ['-f'] + command

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

def CommandTestAgainstGoldenOutput(env, name, command, size='small',
                                   direct_emulation=True, **extra):
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

  if env.get('EMULATOR', ''):
    if direct_emulation:
      command = ['${EMULATOR}'] + command
    else:
      orig = extra.get('osenv', '')
      if orig: orig = orig + ','
      extra['osenv'] = (orig +
                        'EMULATOR=%s' %  env['EMULATOR'].replace(' ', r'\ '))

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

  # If we are testing build output captured from elsewhere,
  # ignore build dependencies.
  if ARGUMENTS.get('built_elsewhere'):
    env.Ignore(name, deps)

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
# for generation of a promiscuous sel_ldr
# ----------------------------------------------------------
if ARGUMENTS.get('dangerous_debug_disable_inner_sandbox'):
  pre_base_env.Append(
      # NOTE: this also affects .S files
      CPPDEFINES=['DANGEROUS_DEBUG_MODE_DISABLE_INNER_SANDBOX'],
      )

# ----------------------------------------------------------
DeclareBit('chrome',
           'Build the plugin as a static library to be linked with Chrome')
pre_base_env.SetBitFromOption('chrome', False)
if pre_base_env.Bit('chrome'):
  pre_base_env.Append(
    CPPDEFINES = [
        ['CHROME_BUILD', 1],
    ],
  )
  # To build for Chrome sdl=none must be used
  ARGUMENTS['sdl'] = 'none'
else:
  pre_base_env.Append(
    CPPDEFINES = [
        ['NACL_STANDALONE', 1],
    ],
  )

# ----------------------------------------------------------
base_env = pre_base_env.Clone()
base_env.Append(
  BUILD_SUBTYPE = '',
  CPPDEFINES = [
    ['NACL_BLOCK_SHIFT', '5'],
    ['NACL_BLOCK_SIZE', '32'],
    ['NACL_BUILD_ARCH', '${BUILD_ARCHITECTURE}' ],
    ['NACL_BUILD_SUBARCH', '${BUILD_SUBARCH}' ],
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

base_env.Append(
  BUILD_SCONSCRIPTS = [
    'src/trusted/service_runtime/build.scons',
    'src/shared/imc/build.scons',
    'src/shared/npruntime/build.scons',
    'src/shared/platform/build.scons',
    'src/shared/srpc/build.scons',
    'src/shared/utils/build.scons',
    'src/third_party_mod/gtest/build.scons',
    'src/trusted/desc/build.scons',
    'src/trusted/nonnacl_util/build.scons',
    'src/trusted/plugin/build.scons',
    'src/trusted/platform_qualify/build.scons',
    'src/trusted/sandbox/build.scons',
    # TODO: This file has an early out in case we are building for ARM
    #       Needs to be cleaned up
    'src/trusted/validator_x86/build.scons',
    'tests/python_version/build.scons',
    'tests/tools/build.scons',
    'installer/build.scons'
    ],
  )

if base_env['TARGET_ARCHITECTURE'] == 'arm':
  base_env.Append(
      BUILD_SCONSCRIPTS = [
        'src/trusted/validator_arm/build.scons',
        'src/trusted/validator_arm/v2/build.scons',
      ])
elif base_env['TARGET_ARCHITECTURE'] == 'x86':
  pass
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

* sel_ldr:            scons --mode=opt-linux sel_ldr

* build the plugin:         scons --mode=opt-linux npGoogleNaClPlugin
*      or:                  scons --mode=opt-linux src/trusted/plugin
* install the plugin:       scons --verbose firefox_install
* install pre-built plugin: scons --prebuilt firefox_install

* build libs needed by sdk: scons --mode=nacl_extra_sdk extra_sdk_update
* purge libs needed by sdk: scons --mode=nacl_extra_sdk extra_sdk_clean
* rebuild sdk:              scons --mode=nacl_extra_sdk \
                                extra_sdk_clean \
                                extra_sdk_update_header \
                                install_libpthread \
                                extra_sdk_update

* dump system info    scons --mode=nacl,opt-linux dummy
Options:
--------
sdl=<mode>        where <mode>:

                    'none': don't use SDL (default)
                    'local': use locally installed SDL
                    'hermetic': use the hermetic SDL copy

naclsdk_mode=<mode>   where <mode>:

                    'local': use locally installed sdk kit
                    'download': use the download copy (default)
                    'custom:<path>': use kit at <path>
                    'manual': use settings from env vars NACL_SDL_xxx


--prebuilt          Do not build things, just do install steps

--download          Download the sdk and unpack it at the appropriate place

--verbose           Full command line logging before command execution

pp=1                Use command line pretty printing (more concise output)

sysinfo=            Suppress verbose system info printing

naclsdk_validate=0  Suppress presence check of sdk



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
    # TODO(new_hire): '/Wp64' should be in here.
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
# -Wdeclaration-after-statement is desirable because MS studio does
# not allow declarations after statements in a block, and since much
# of our code is portable and primarily initially tested on Linux,
# it'd be nice to get the build error earlier rather than later
# (building and testing on Linux is faster).
unix_like_env.Prepend(
  CFLAGS = ['-std=gnu99', '-Wdeclaration-after-statement' ],
  CCFLAGS = [
    # '-malign-double',
    '-Werror',
    '-Wall',
    '-pedantic',
    '-Wextra',
    '-Wno-long-long',
    '-Wswitch-enum',
    '-Wsign-compare',
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
EMULATOR = None

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
  # NOTE: this hack allows us to propagate the emulator to the untrusted env
  # TODO(robertm): clean this up
  EMULATOR = os.getenv('ARM_EMU', '')
  linux_env.Replace(CC=os.getenv('ARM_CC', 'NO-ARM-CC-SPECIFIED'),
                    CXX=os.getenv('ARM_CXX', 'NO-ARM-CXX-SPECIFIED'),
                    LD=os.getenv('ARM_LD', 'NO-ARM-LD-SPECIFIED'),
                    EMULATOR=EMULATOR,
                    ASFLAGS=[],
                    LIBPATH=['${LIB_DIR}',
                             os.getenv('ARM_LIB_DIR', '').split()],
                    CCFLAGS=['-march=armv7a','-pedantic','-Wall','-Werror'],
                    LINKFLAGS=os.getenv('ARM_LINKFLAGS', ''),
                    )

  linux_env.Append(LIBS=['rt', 'dl', 'pthread', 'ssl', 'crypto'])
else:
  Banner('Strange platform: %s' % BUILD_NAME)

# TODO(robert): support for arm builds

(linux_debug_env, linux_optimized_env) = GenerateOptimizationLevels(linux_env)


# ----------------------------------------------------------
# The nacl_env is used to build native_client modules
# using a special tool chain which produces platform
# independent binaries
# NOTE: this loads stuff from: site_scons/site_tools/naclsdk.py
# ----------------------------------------------------------
nacl_env = pre_base_env.Clone(
    tools = ['naclsdk'],
    BUILD_TYPE = 'nacl',
    BUILD_TYPE_DESCRIPTION = 'NaCl module build',
    NACL_BUILD_FAMILY = 'UNTRUSTED',

    EXTRA_CFLAGS = [],
    EXTRA_CCFLAGS = [],
    EXTRA_CXXFLAGS = [],
    EXTRA_LIBS = [],

    # always optimize binaries
    CCFLAGS = ['-O2',
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
)

# limit the majority of test to x86 for now
if (nacl_env['BUILD_ARCHITECTURE'] == 'x86' and
    nacl_env['TARGET_ARCHITECTURE'] == 'x86'):
  nacl_env.Append(
      CCFLAGS = ['-mfpmath=sse',
                 '-msse',
# NOTE: The flags below would make explicit it completely explciit
#       which include directories the nacl tool chains is using.
#       They have been verified to work.
#                 '-nostdinc',
#                 '-isystem',
#                 '${NACL_SDK_INCLUDE}/../../lib/gcc/nacl/4.2.2/include',
#                 '-isystem',
#                 '${NACL_SDK_INCLUDE}',
#                 '-isystem',
#                 '${NACL_SDK_INCLUDE}/c++/4.2.2',
#                 '-isystem',
#                 '${NACL_SDK_INCLUDE}/c++/4.2.2/nacl',
                 ]
      )

  nacl_env.Append(
      BUILD_SCONSCRIPTS = [
          ####  ALPHABETICALLY SORTED ####
          'tests/app_lib/nacl.scons',
          'tests/autoloader/nacl.scons',
          'tests/contest_issues/nacl.scons',
          'tests/fib/nacl.scons',
          'tests/file/nacl.scons',
          'tests/hello_world/nacl.scons',
          'tests/imc_shm_mmap/nacl.scons',
          'tests/mandel/nacl.scons',
          'tests/mmap/nacl.scons',
          'tests/nanosleep/nacl.scons',
          'tests/native_worker/nacl.scons',
          'tests/noop/nacl.scons',
          'tests/npapi_bridge/nacl.scons',
          'tests/npapi_hw/nacl.scons',
          'tests/npapi_pi/nacl.scons',
          'tests/nrd_xfer/nacl.scons',
          'tests/nthread_nice/nacl.scons',
          'tests/null/nacl.scons',
          'tests/nullptr/nacl.scons',
          'tests/srpc/nacl.scons',
          'tests/srpc_hw/nacl.scons',
          'tests/srpc_without_pthread/nacl.scons',
          'tests/sysbasic/nacl.scons',
          'tests/syscalls/nacl.scons',
          'tests/threads/nacl.scons',
          'tests/time/nacl.scons',
          'tests/vim/nacl.scons',
          ####  ALPHABETICALLY SORTED ####
          ],
      )

  if ARGUMENTS.get('sdl', 'hermetic') != 'none':
    nacl_env.Append(
        BUILD_SCONSCRIPTS = [
            ####  ALPHABETICALLY SORTED ####
            'common/console/nacl.scons',

            'tests/earth/nacl.scons',
            'tests/life/nacl.scons',
            'tests/mandel_nav/nacl.scons',
            'tests/many/nacl.scons',
            'tests/selenium_dummy/nacl.scons',
            'tests/tone/nacl.scons',
            'tests/voronoi/nacl.scons',

            'tools/tests/nacl.scons',
            ])

if (nacl_env['BUILD_ARCHITECTURE'] == 'arm' and
    nacl_env['TARGET_ARCHITECTURE'] == 'arm'):
  # NOTE: we change the linker command line to make it possible to
  #       sneak in startup and cleanup code
  linkcom = (nacl_env['LINKCOM'].replace('$LINK ', '$LINK $LINKFLAGS_FIRST ') +
              ' $LINKFLAGS_LAST')
  nacl_env['LINKCOM'] = linkcom

  nacl_env.Prepend(
      LINKFLAGS_FIRST = ['${NACL_SDK_LIB}/crt1.o','${NACL_SDK_LIB}/crti.o',],
      # NOTE: order is important
      LIBS = ['srpc', 'c', 'nacl'],
      LINKFLAGS_LAST = ['-lc', '-lgcc', '-lunimpl', '${NACL_SDK_LIB}/crtn.o',],
      EMULATOR  = EMULATOR,
      )

  # TODO(robertm): merge this with list above as soon as they are similar
  #                enough
  nacl_env.Append(
      BUILD_SCONSCRIPTS = [
#      NOTE: The commented out case are earmarked to be tried next
#      'tests/app_lib/nacl.scons',
      'tests/autoloader/nacl.scons',
#      'tests/contest_issues/nacl.scons',
      'tests/fib/nacl.scons',
#      'tests/file/nacl.scons',
      'tests/hello_world/nacl.scons',
      'tests/imc_shm_mmap/nacl.scons',
      'tests/mandel/nacl.scons',
      'tests/mmap/nacl.scons',
      'tests/native_worker/nacl.scons',
      'tests/noop/nacl.scons',
      'tests/nrd_xfer/nacl.scons',
#      'tests/nthread_nice/nacl.scons',
      'tests/null/nacl.scons',
      'tests/nullptr/nacl.scons',
      # NOTE: does not run sel_univesal test which invokes sel_ldr
      'tests/srpc/nacl.scons',
      'tests/srpc_hw/nacl.scons',
      'tests/srpc_without_pthread/nacl.scons',
      'tests/sysbasic/nacl.scons',
#      'tests/syscalls/nacl.scons',
#      'tests/threads/nacl.scons',
      'tests/vim/nacl.scons',

      'tests/arm_service_runtime/nacl.scons',
      ])

environment_list.append(nacl_env)


# ----------------------------------------------------------
# Possibly install an sdk by downloading it
# ----------------------------------------------------------
# TODO: explore using a less heavy weight mechanism
# NOTE: this uses stuff from: site_scons/site_tools/naclsdk.py
import SCons.Script

SCons.Script.AddOption('--download',
                       dest='download',
                       metavar='DOWNLOAD',
                       default=False,
                       action='store_true',
                       help='allow tools to download')

if nacl_env.GetOption('download'):
  nacl_env.DownloadSdk()

# ----------------------------------------------------------
# Sanity check whether we are ready to build nacl modules
# ----------------------------------------------------------
# NOTE: this uses stuff from: site_scons/site_tools/naclsdk.py
if int(ARGUMENTS.get('naclsdk_validate', "1")):
  nacl_env.ValidateSdk()

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
               '-fdiagnostics-show-option',
               '-pedantic',
               ],

    CPPPATH = ['$SOURCE_ROOT'],
    LINK = '$CXX',
    CPPDEFINES = [
      ['NACL_BUILD_ARCH', '${BUILD_ARCHITECTURE}' ],
      ['NACL_BUILD_SUBARCH', '${BUILD_SUBARCH}' ],
      ['NACL_BLOCK_SHIFT', '5' ],
      ],
)

if nacl_extra_sdk_env.Bit('host_windows'):
  # NOTE: This is needed because Windows builds are case-insensitive.
  # Without this we use nacl-as, which doesn't handle include directives, etc.
  nacl_extra_sdk_env.Replace(ASCOM = '${CCCOM}',)
else:
  nacl_extra_sdk_env.Replace(ASFLAGS = '$CCFLAGS',)


nacl_extra_sdk_env.Append(
    BUILD_SCONSCRIPTS = [
      ####  ALPHABETICALLY SORTED ####
      'src/include/nacl/nacl.scons',
      'src/shared/imc/nacl.scons',
      'src/shared/npruntime/nacl.scons',
      'src/shared/srpc/nacl.scons',
      'src/untrusted/nacl/nacl.scons',
      'src/untrusted/pthread/nacl.scons',
      'src/untrusted/stubs/nacl.scons',
      'src/untrusted/unimpl/nacl.scons',
      ####  ALPHABETICALLY SORTED ####
   ],
)

if ARGUMENTS.get('sdl', 'hermetic') != 'none':
  nacl_extra_sdk_env.Append(
      BUILD_SCONSCRIPTS = ['src/untrusted/av/nacl.scons',])

# TODO(robertm): remove this ASAP, we currently have llvm issue with c++
if nacl_extra_sdk_env['TARGET_ARCHITECTURE'] == 'arm':
  nacl_extra_sdk_env.FilterOut(
      CCFLAGS = ['-Werror',])
  nacl_extra_sdk_env.Append(
      CFLAGS = ['-Werror',])

environment_list.append(nacl_extra_sdk_env)

# ----------------------------------------------------------
# Targets for updating sdk headers and libraries
# NACL_SDK_XXX vars are defined by  site_scons/site_tools/naclsdk.py
# NOTE: our task here is complicated by the fact that there might already
#       some (outdated) headers/libraries at the new location
#       One of the hacks we employ here is to make every library depend
#       on the installation on ALL headers (sdk_headers)

# Contains all the headers to be installed
sdk_headers = nacl_extra_sdk_env.Alias('extra_sdk_update_header', [])
# Contains all the libraries and .o files to be installed
nacl_extra_sdk_env.Alias('extra_sdk_update', [])


def AddHeaderToSdk(env, nodes):
  n = env.Replicate('${NACL_SDK_INCLUDE}/nacl/', nodes)
  env.Alias('extra_sdk_update_header', n)
  return n

nacl_extra_sdk_env.AddMethod(AddHeaderToSdk)

def AddLibraryToSdk(env, nodes):
  # NOTE: hack see comment
  nacl_extra_sdk_env.Requires(nodes, sdk_headers)

  n = env.ReplicatePublished('${NACL_SDK_LIB}/', nodes, 'link')
  env.Alias('extra_sdk_update', n)
  return n

nacl_extra_sdk_env.AddMethod(AddLibraryToSdk)

def AddObjectToSdk(env, nodes):
  # NOTE: hack see comment
  nacl_extra_sdk_env.Requires(nodes, sdk_headers)

  n = env.Replicate('${NACL_SDK_LIB}/', nodes)
  env.Alias('extra_sdk_update', n)
  return n

nacl_extra_sdk_env.AddMethod(AddObjectToSdk)

# NOTE: a helpful target to test the sdk_extra magic
#       combine this with a 'MODE=nacl -c'
# TODO: the cleanup of libs is not complete
nacl_extra_sdk_env.Command('extra_sdk_clean', [],
                           ['rm -rf ${NACL_SDK_INCLUDE}/nacl*',
                            'rm -rf ${NACL_SDK_LIB}/libgoogle*',
                            'rm -rf ${NACL_SDK_LIB}/crt[1in].*'])

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
                         'EMULATOR',
                         ]

def DumpCompilerVersion(cc, env):
  if 'gcc' in cc:
    env.Execute(env.Action('${CC} --v -c'))
    env.Execute(env.Action('${CC} -print-search-dirs'))
    env.Execute(env.Action('${CC} -print-libgcc-file-name'))
  elif cc.startswith('cl'):
    import subprocess
    try:
      p = subprocess.Popen(env.subst('${CC} /V'),
                           bufsize=1000*1000,
                           stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE)
    except WindowsError:
      print 'ERROR: Visual Studio tools are not in your path.'
      print '       Please run vcvars32.bat'
      print '       Typically located in:'
      print '          c:\\Program Files\\Microsoft Visual Studio 8\\VC\\Bin'
      sys.exit(1)
    stderr = p.stderr.read()
    stdout = p.stdout.read()
    retcode = p.wait()
    print stderr[0:stderr.find("\r")]
  else:
    print "UNKNOWN COMPILER"


def SanityCheckAndMapExtraction(all_envs, selected_envs):
  # simple completeness check
  for env in all_envs:
    for tag in RELEVANT_CONFIG:
      assert tag in env
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

  Banner("The following environments have been configured")
  for family in family_map:
    env = family_map[family]
    for tag in RELEVANT_CONFIG:
      assert tag in env
      print "%s:  %s" % (tag, env.subst(env.get(tag)))
    for tag in MAYBE_RELEVANT_CONFIG:
        print "%s:  %s" % (tag, env.subst(env.get(tag)))
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

# print sytem info (optionally)
if ARGUMENTS.get('sysinfo', 1):
  Banner('SCONS ARGS:' + str(sys.argv))
  os.system(pre_base_env.subst('${PYTHON} tools/sysinfo.py'))



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
